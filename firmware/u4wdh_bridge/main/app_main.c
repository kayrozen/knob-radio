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
#include "dac_mute.h"

static const char *TAG = "u4wdh_main";

/* Bytes of audio for a given depth in milliseconds at the link sample rate. */
#define JB_BYTES_FOR_MS(ms) ((size_t)PCM_LINK_SAMPLE_RATE_HZ * \
                             PCM_LINK_BYTES_PER_SAMPLE * (ms) / 1000)

/* Internal DRAM to leave free *after* allocating the jitter buffer, for the
 * UART driver ring (~16 KiB), task stacks, and the BT/A2DP buffers Bluedroid
 * allocates while a stream is active. Conservative. */
#define JB_RUNTIME_HEADROOM   (50 * 1024)

/* Step the fallback search by this much per attempt. */
#define JB_FALLBACK_STEP_MS   20

static jitter_buffer_t  s_jb;
static uart_reader_ctx_t s_rx_ctx;

/*
 * Allocate the largest jitter buffer in [MIN, MAX] ms that fits internal DRAM
 * while leaving JB_RUNTIME_HEADROOM free. Returns the backing pointer (and the
 * depth actually obtained via *got_ms), or NULL if even MIN does not fit.
 */
static uint8_t *alloc_jitter_buffer(size_t *got_bytes, uint32_t *got_ms)
{
    for (uint32_t ms = PCM_LINK_JITTER_MAX_MS;
         ms >= PCM_LINK_JITTER_MIN_MS;
         ms -= JB_FALLBACK_STEP_MS) {

        size_t want = JB_BYTES_FOR_MS(ms);
        size_t freeb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (freeb < want + JB_RUNTIME_HEADROOM) {
            continue; /* would not leave enough for BT/UART/stacks */
        }
        uint8_t *p = heap_caps_malloc(want, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (p) {
            *got_bytes = want;
            *got_ms    = ms;
            return p;
        }
        /* Allocation failed despite the size check (fragmentation) — keep
         * stepping down. */
    }
    return NULL;
}

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

    /* DAC starts muted; the S3 un-mutes over the control plane in analog mode. */
    dac_mute_init();

    /* Bring up the BT stack FIRST so it claims its (large) static + dynamic
     * DRAM before we size the jitter buffer from what survives. */
    a2dp_bridge_init();
    ESP_LOGI(TAG, "internal DRAM free after BT bringup: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    /* Size the jitter buffer to fit, degrading depth gracefully if needed. */
    size_t   jb_bytes = 0;
    uint32_t jb_ms    = 0;
    uint8_t *backing  = alloc_jitter_buffer(&jb_bytes, &jb_ms);
    if (backing == NULL) {
        ESP_LOGE(TAG, "FATAL: cannot allocate even a %d ms jitter buffer "
                 "(%u bytes free) — too little RAM after BT",
                 PCM_LINK_JITTER_MIN_MS,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        abort();
    }
    jb_init(&s_jb, backing, jb_bytes);

    if (jb_ms < PCM_LINK_JITTER_MAX_MS) {
        ESP_LOGW(TAG, "jitter buffer DEGRADED to %lu ms (%u bytes); nominal is "
                 "%d ms — less drift/loss headroom",
                 (unsigned long)jb_ms, (unsigned)jb_bytes, PCM_LINK_JITTER_MAX_MS);
    } else {
        ESP_LOGI(TAG, "jitter buffer: %lu ms (%u bytes) — nominal",
                 (unsigned long)jb_ms, (unsigned)jb_bytes);
    }

    /* Now wire the audio path: callback source, then the producers. */
    a2dp_bridge_set_buffer(&s_jb);

    /* Core 1: forward UART RX + COBS reassembly + return-channel backpressure. */
    uart_reader_start(&s_rx_ctx, &s_jb);
    backpressure_start(&s_jb);

    /* Core 0: begin discovery / connection to the sink. */
    a2dp_bridge_start_discovery();

    ESP_LOGI(TAG, "ready; internal DRAM free: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}
