/*
 * app_main.c — ESP32-U4WDH bridge: UART/COBS PCM -> jitter buffer -> A2DP.
 *
 * The "dumb, reliable bridge" half of the dual-chip prototype. No WiFi, no
 * codecs, no UI — just receive PCM frames, buffer them, and stream SBC to the
 * car. All complexity lives on the S3.
 */
#include <stdlib.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pcm_link_proto.h"
#include "jitter_buffer.h"
#include "uart_reader.h"
#include "backpressure.h"
#include "a2dp_bridge.h"

static const char *TAG = "u4wdh_main";

/* Jitter-buffer capacity (no PSRAM on the U4WDH). Sized for the max target
 * depth: 44100 * 4 B * 420 ms = ~73 KiB. Allocated from the internal DRAM heap
 * at startup *before* the BT stack, rather than as a static .bss array: the
 * Bluedroid + controller static footprint already fills most of DRAM, so a
 * 73 KiB .bss array overflows the segment at link time. Heap allocation lets
 * the prototype measure exactly how much internal RAM survives the BT bringup
 * (the deliverable's "RAM libre U4WDH" metric). */
#define JB_CAPACITY  ((size_t)PCM_LINK_SAMPLE_RATE_HZ * PCM_LINK_BYTES_PER_SAMPLE * \
                      PCM_LINK_JITTER_MAX_MS / 1000)

static jitter_buffer_t  s_jb;
static uart_reader_ctx_t s_rx_ctx;

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "hello U4WDH — PCM/UART -> A2DP bridge");
    ESP_LOGI(TAG, "internal DRAM free at boot: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    /* Grab the jitter buffer from internal RAM first so the audio path is
     * guaranteed its memory before the BT stack claims the rest. */
    uint8_t *backing = heap_caps_malloc(JB_CAPACITY,
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (backing == NULL) {
        ESP_LOGE(TAG, "FATAL: could not allocate %u-byte jitter buffer",
                 (unsigned)JB_CAPACITY);
        abort();
    }
    jb_init(&s_jb, backing, JB_CAPACITY);
    ESP_LOGI(TAG, "jitter buffer: %u bytes (~%d ms) allocated",
             (unsigned)JB_CAPACITY, PCM_LINK_JITTER_MAX_MS);

    /* Core 1: forward UART RX + COBS reassembly + return-channel backpressure. */
    uart_reader_start(&s_rx_ctx, &s_jb);
    backpressure_start(&s_jb);

    /* Core 0: classic-BT controller + A2DP source. */
    a2dp_bridge_start(&s_jb);

    ESP_LOGI(TAG, "internal DRAM free after BT bringup: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}
