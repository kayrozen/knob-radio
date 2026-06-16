/*
 * backpressure.c — see backpressure.h.
 */
#include "backpressure.h"
#include "pcm_link_proto.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "backpressure";

#define RETURN_UART_NUM   UART_NUM_1   /* shared full-duplex UART */
#define POLL_PERIOD_MS    100

static void backpressure_task(void *arg)
{
    jitter_buffer_t *jb = (jitter_buffer_t *)arg;
    pcm_link_bp_cmd_t last = PCM_LINK_BP_NORMAL;

    for (;;) {
        uint32_t ms = jb_filled_ms(jb);
        pcm_link_bp_cmd_t cmd;
        if (ms > PCM_LINK_JITTER_HIGH_MS) {
            cmd = PCM_LINK_BP_SLOWDOWN;
        } else if (ms < PCM_LINK_JITTER_LOW_MS) {
            cmd = PCM_LINK_BP_SPEEDUP;
        } else {
            cmd = PCM_LINK_BP_NORMAL;
        }

        /* Send on change, and also resend NORMAL periodically as a heartbeat
         * so a lost command does not strand the producer at one extreme. */
        static int heartbeat = 0;
        if (cmd != last || (++heartbeat % 10) == 0) {
            uint8_t b = (uint8_t)cmd;
            uart_write_bytes(RETURN_UART_NUM, &b, 1);
            if (cmd != last) {
                ESP_LOGD(TAG, "jb=%lums -> cmd 0x%02x", (unsigned long)ms, b);
            }
            last = cmd;
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
    }
}

void backpressure_start(jitter_buffer_t *jb)
{
    xTaskCreatePinnedToCore(backpressure_task, "backpressure", 2560, jb, 6,
                            NULL, 1);
}
