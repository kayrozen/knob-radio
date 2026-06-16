/*
 * encoder.c — see encoder.h.
 */
#include "encoder.h"
#include "station.h"

#include "driver/pulse_cnt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "encoder";

#define PCNT_HIGH_LIMIT  1000
#define PCNT_LOW_LIMIT  -1000
#define COUNTS_PER_DETENT 4     /* typical for EC1-style quadrature encoders */

static pcnt_unit_handle_t s_unit;
static encoder_cb_t       s_cb;

static void encoder_task(void *arg)
{
    (void)arg;
    int last_detent = 0;
    for (;;) {
        int count = 0;
        pcnt_unit_get_count(s_unit, &count);
        int detent = count / COUNTS_PER_DETENT;
        int delta = detent - last_detent;
        if (delta != 0) {
            last_detent = detent;
            int idx = station_advance(delta);
            ESP_LOGI(TAG, "detent %+d -> station %d", delta, idx);
            if (s_cb) {
                s_cb(idx);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void encoder_start(int gpio_a, int gpio_b, encoder_cb_t cb)
{
    s_cb = cb;

    pcnt_unit_config_t unit_cfg = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit  = PCNT_LOW_LIMIT,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_cfg, &s_unit));

    pcnt_glitch_filter_config_t filter = { .max_glitch_ns = 1000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_unit, &filter));

    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num  = gpio_a,
        .level_gpio_num = gpio_b,
    };
    pcnt_channel_handle_t chan_a;
    ESP_ERROR_CHECK(pcnt_new_channel(s_unit, &chan_a_cfg, &chan_a));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num  = gpio_b,
        .level_gpio_num = gpio_a,
    };
    pcnt_channel_handle_t chan_b;
    ESP_ERROR_CHECK(pcnt_new_channel(s_unit, &chan_b_cfg, &chan_b));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(s_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_unit));

    ESP_LOGI(TAG, "encoder on GPIO A=%d B=%d", gpio_a, gpio_b);
    /* Core 0: UI-side input, away from the audio/UART core. */
    xTaskCreatePinnedToCore(encoder_task, "encoder", 3072, NULL, 5, NULL, 0);
}
