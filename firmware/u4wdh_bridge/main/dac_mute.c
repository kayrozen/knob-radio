/*
 * dac_mute.c — see dac_mute.h.
 */
#include "dac_mute.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "dac_mute";

/* PCM5100 XSMT line -> U4WDH GPIO32 (ESP32_IO32 = XSMT on the schematic). */
#define XSMT_GPIO   32

void dac_mute_init(void)
{
    gpio_config_t cfg = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << XSMT_GPIO,
    };
    gpio_config(&cfg);
    gpio_set_level(XSMT_GPIO, 0);   /* start muted */
    ESP_LOGI(TAG, "XSMT on GPIO%d, muted", XSMT_GPIO);
}

void dac_mute_set(bool muted)
{
    gpio_set_level(XSMT_GPIO, muted ? 0 : 1);
    ESP_LOGI(TAG, "DAC %s", muted ? "muted" : "un-muted");
}
