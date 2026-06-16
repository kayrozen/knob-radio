/*
 * app_main.c — ESP32-U4WDH bridge: UART/COBS PCM -> jitter buffer -> A2DP.
 *
 * The "dumb, reliable bridge" half of the dual-chip prototype. No WiFi, no
 * codecs, no UI — just receive PCM frames, buffer them, and stream SBC to the
 * car. All complexity lives on the S3.
 */
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pcm_link_proto.h"
#include "jitter_buffer.h"
#include "uart_reader.h"
#include "backpressure.h"
#include "a2dp_bridge.h"

static const char *TAG = "u4wdh_main";

/* Jitter-buffer backing in SRAM (no PSRAM on the U4WDH). Sized for the max
 * target depth: 44100 * 4 B * 420 ms = ~73 KiB. The deliverable reports how
 * much SRAM remains free after the BT stack is up. */
#define JB_CAPACITY  ((size_t)PCM_LINK_SAMPLE_RATE_HZ * PCM_LINK_BYTES_PER_SAMPLE * \
                      PCM_LINK_JITTER_MAX_MS / 1000)
static uint8_t s_jb_backing[JB_CAPACITY];

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
    ESP_LOGI(TAG, "jitter buffer capacity: %u bytes (~%d ms)",
             (unsigned)JB_CAPACITY, PCM_LINK_JITTER_MAX_MS);

    jb_init(&s_jb, s_jb_backing, JB_CAPACITY);

    /* Core 1: forward UART RX + COBS reassembly + return-channel backpressure. */
    uart_reader_start(&s_rx_ctx, &s_jb);
    backpressure_start(&s_jb);

    /* Core 0: classic-BT controller + A2DP source. */
    a2dp_bridge_start(&s_jb);

    ESP_LOGI(TAG, "free heap after bringup: %lu bytes",
             (unsigned long)esp_get_free_heap_size());
}
