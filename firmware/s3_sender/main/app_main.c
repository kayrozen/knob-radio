/*
 * app_main.c — ESP32-S3 sender: PCM -> COBS frame -> UART forward link.
 *
 * Phases A-D: synthesizes a test PCM signal and streams it over the COBS/UART
 * forward link to the U4WDH bridge, honoring return-channel backpressure. This
 * validates the link, framing, jitter and drift control without WiFi.
 *
 * Phase E (documented, not built here): swap pcm_source for the ESP-ADF
 * pipeline (WiFi -> http/hls_stream -> decoder -> resample -> PCM) and add the
 * LVGL UI on the rotary encoder. The uart_writer framing path is unchanged.
 */
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "pcm_source.h"
#include "uart_writer.h"
#include "backpressure_rx.h"

static const char *TAG = "s3_main";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "hello S3 — PCM -> COBS/UART forward link");

    pcm_source_init();
    backpressure_rx_start();   /* must be listening before we start pacing */
    uart_writer_start();

    ESP_LOGI(TAG, "free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
}
