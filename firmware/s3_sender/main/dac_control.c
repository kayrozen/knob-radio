/*
 * dac_control.c — see dac_control.h.
 */
#include "dac_control.h"
#include "board_pins.h"
#include "pcm_link_proto.h"

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "dac";

static i2s_chan_handle_t s_tx;

esp_err_t dac_control_init(void)
{
    if (s_tx) {
        return ESP_OK;
    }

    /* CH445P source select: drive low to route the S3's I2S to the DAC.
     * (GPIO0 is a boot strap, but it is only sampled at reset; safe as an
     * output afterwards.) */
    gpio_config_t sw = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BOARD_I2S_SWITCH_IN,
    };
    gpio_config(&sw);
    gpio_set_level(BOARD_I2S_SWITCH_IN, 0);

    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel: %s", esp_err_to_name(err));
        return err;
    }

    const i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(PCM_LINK_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BOARD_I2S_DAC_BCK,
            .ws   = BOARD_I2S_DAC_LRCK,
            .dout = BOARD_I2S_DAC_DIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { 0 },
        },
    };
    err = i2s_channel_init_std_mode(s_tx, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s init std: %s", esp_err_to_name(err));
        return err;
    }
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));

    ESP_LOGI(TAG, "I2S DAC up (BCK=%d WS=%d DOUT=%d), CH445P->S3",
             BOARD_I2S_DAC_BCK, BOARD_I2S_DAC_LRCK, BOARD_I2S_DAC_DIN);
    return ESP_OK;
}

size_t dac_control_write(const uint8_t *pcm, size_t len)
{
    if (!s_tx) {
        return 0;
    }
    size_t written = 0;
    if (i2s_channel_write(s_tx, pcm, len, &written, portMAX_DELAY) != ESP_OK) {
        return 0;
    }
    return written;
}
