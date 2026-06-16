/*
 * audio_output.c — see audio_output.h.
 */
#include "audio_output.h"
#include "pcm_source.h"
#include "uart_writer.h"
#include "backpressure_rx.h"
#include "dac_control.h"
#include "pcm_link_proto.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "audio_out";

static audio_output_mode_t s_mode = AUDIO_OUTPUT_BT;

/* Analog feeder: pull PCM from the source and push it straight to the DAC.
 * Pinned to core 1 (audio), mirroring the UART writer's placement. */
static void dac_feed_task(void *arg)
{
    (void)arg;
    static uint8_t buf[PCM_LINK_PAYLOAD_BYTES];
    for (;;) {
        pcm_source_read(buf, sizeof(buf));
        dac_control_write(buf, sizeof(buf));
    }
}

void audio_output_start(audio_output_mode_t mode)
{
    s_mode = mode;

    if (mode == AUDIO_OUTPUT_ANALOG) {
        ESP_LOGI(TAG, "output: ANALOG (S3 I2S -> PCM5100)");
        /* The DAC's XSMT mute is on the U4WDH; ask it to unmute over the
         * control plane (the link is still wired even though no audio flows
         * to the U4WDH in analog mode). */
        uart_writer_init_link();
        uint8_t unmute = 0;   /* 0 = un-mute, 1 = mute */
        uart_writer_send_control(PCM_LINK_CTRL_DAC_MUTE, &unmute, 1);

        if (dac_control_init() == ESP_OK) {
            xTaskCreatePinnedToCore(dac_feed_task, "dac_feed", 4096, NULL, 12,
                                    NULL, 1);
        }
        return;
    }

    /* AUDIO_OUTPUT_BT (default). */
    ESP_LOGI(TAG, "output: BT (COBS/UART -> U4WDH A2DP)");
    backpressure_rx_start();   /* listen before we start pacing */
    uart_writer_start();       /* core 1: pcm_source -> COBS -> UART */
}

audio_output_mode_t audio_output_mode(void)
{
    return s_mode;
}
