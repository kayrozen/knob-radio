/*
 * uart_writer.c — see uart_writer.h.
 */
#include "uart_writer.h"
#include "pcm_source.h"
#include "backpressure_rx.h"
#include "pcm_link_proto.h"
#include "pcm_frame.h"
#include "control_msg.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "uart_tx";

#define FORWARD_UART_NUM   UART_NUM_1
#define TX_RING_BYTES      (8 * 1024)

/* One frame carries PCM_LINK_PAYLOAD_BYTES of audio. At 44.1 kHz stereo 16-bit
 * that is PAYLOAD/4 sample-pairs; nominal frame period in microseconds: */
#define SAMPLES_PER_FRAME  (PCM_LINK_PAYLOAD_BYTES / PCM_LINK_BYTES_PER_SAMPLE)
#define FRAME_PERIOD_US    ((double)SAMPLES_PER_FRAME * 1000000.0 / PCM_LINK_SAMPLE_RATE_HZ)

static void uart_tx_task(void *arg)
{
    (void)arg;
    static uint8_t payload[PCM_LINK_PAYLOAD_BYTES];
    static uint8_t wire[PCM_LINK_FRAME_MAX_WIRE];

    uint8_t seq = 0;
    uint32_t frames = 0, bytes_on_wire = 0;
    int64_t next_us = esp_timer_get_time();
    TickType_t last_log = xTaskGetTickCount();

    for (;;) {
        pcm_source_read(payload, sizeof(payload));

        size_t w = pcm_frame_build(seq++, payload, sizeof(payload), wire);
        int written = uart_write_bytes(FORWARD_UART_NUM, wire, w);
        if (written > 0) {
            bytes_on_wire += (uint32_t)written;
        }
        frames++;

        /* Pace to the (backpressure-adjusted) nominal frame period. */
        double rate = backpressure_rate();
        next_us += (int64_t)(FRAME_PERIOD_US * rate);
        int64_t now = esp_timer_get_time();
        int64_t sleep_us = next_us - now;
        if (sleep_us > 1000) {
            vTaskDelay(pdMS_TO_TICKS((sleep_us) / 1000));
        } else if (sleep_us < -50000) {
            /* Fell badly behind (e.g. UART backed up): resync the clock. */
            next_us = now;
        }

        if (xTaskGetTickCount() - last_log > pdMS_TO_TICKS(5000)) {
            last_log = xTaskGetTickCount();
            ESP_LOGI(TAG, "frames=%lu wire=%lu B rate=%.3f",
                     (unsigned long)frames, (unsigned long)bytes_on_wire, rate);
        }
    }
}

void uart_writer_init_link(void)
{
    static bool installed;
    if (installed) {
        return;
    }
    const uart_config_t cfg = {
        .baud_rate = PCM_LINK_FORWARD_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    /* TX buffer enables DMA-backed, non-blocking-ish writes. RX ring receives
     * the return-channel backpressure bytes (read by backpressure_rx). */
    ESP_ERROR_CHECK(uart_driver_install(FORWARD_UART_NUM, 1024, TX_RING_BYTES,
                                        0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(FORWARD_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(FORWARD_UART_NUM, PCM_LINK_S3_TX_GPIO,
                                 PCM_LINK_S3_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    installed = true;

    ESP_LOGI(TAG, "UART1 TX GPIO%d / RX GPIO%d @ %d baud",
             PCM_LINK_S3_TX_GPIO, PCM_LINK_S3_RX_GPIO, PCM_LINK_FORWARD_BAUD);
}

void uart_writer_send_control(uint8_t op, const uint8_t *args, uint16_t len)
{
    static uint8_t seq;
    uint8_t wire[PCM_LINK_FRAME_MAX_WIRE];
    size_t w = pcm_link_ctrl_build_frame(seq++, op, args, len, wire);
    if (w) {
        uart_write_bytes(FORWARD_UART_NUM, wire, w);
    }
}

void uart_writer_start(void)
{
    uart_writer_init_link();
    ESP_LOGI(TAG, "audio writer up; frame period %.3f ms", FRAME_PERIOD_US / 1000.0);
    xTaskCreatePinnedToCore(uart_tx_task, "uart_tx", 4096, NULL, 12, NULL, 1);
}
