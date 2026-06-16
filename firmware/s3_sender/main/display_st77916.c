/*
 * display_st77916.c — see display_st77916.h.
 *
 * --------------------------------------------------------------------------
 * PINOUT — VERIFY against the JC3636K518 schematic before trusting the panel.
 * --------------------------------------------------------------------------
 * Two sources disagree and neither is confirmed on this board:
 *
 *   (a) The JC3636W518V2 LVGL port these values came from:
 *       SCLK=40, CS=21, D0..D3=46/45/42/41, RST=none, BL=5.
 *   (b) The product plan §2.1 ("confirmés"): QSPI on GPIO13-18 (individual
 *       SCLK/CS/D0-D3 assignment unspecified), RST=GPIO21, BLK=GPIO47, plus
 *       an LCD_TE tearing-effect line.
 *
 * (a) is specific enough to wire, so it is used here; (b) reassigns 21 (CS->RST)
 * and the backlight (5->47), which can't both be right. Resolve against the
 * schematic and update the block below — it is the single source of truth. The
 * choice does not affect whether the firmware builds, only whether pixels show.
 */
#include "display_st77916.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_st77916.h"
#include "esp_log.h"

static const char *TAG = "display";

/* --- Panel pins (see the pinout note above). --- */
#define LCD_HOST            SPI2_HOST
#define PIN_LCD_SCLK        40
#define PIN_LCD_CS          21
#define PIN_LCD_D0          46
#define PIN_LCD_D1          45
#define PIN_LCD_D2          42
#define PIN_LCD_D3          41
#define PIN_LCD_RST         (-1)
#define PIN_LCD_BL          5
/* #define PIN_LCD_TE       -1   // tearing-effect sync line (plan §2.2); unused */

/* --- Backlight PWM (LEDC) --- */
#define BL_LEDC_TIMER       LEDC_TIMER_0
#define BL_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define BL_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BL_LEDC_DUTY_RES    LEDC_TIMER_10_BIT       /* 0..1023 */
#define BL_LEDC_FREQ_HZ     5000
#define BL_DUTY_MAX         1023

static bool s_bl_pwm;   /* true once the LEDC backlight is configured */

static void backlight_init(void)
{
    if (PIN_LCD_BL < 0) {
        return;
    }
    const ledc_timer_config_t tcfg = {
        .speed_mode      = BL_LEDC_MODE,
        .timer_num       = BL_LEDC_TIMER,
        .duty_resolution = BL_LEDC_DUTY_RES,
        .freq_hz         = BL_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&tcfg) != ESP_OK) {
        return;
    }
    const ledc_channel_config_t ccfg = {
        .gpio_num   = PIN_LCD_BL,
        .speed_mode = BL_LEDC_MODE,
        .channel    = BL_LEDC_CHANNEL,
        .timer_sel  = BL_LEDC_TIMER,
        .duty       = 0,            /* start dark; UI raises it after init */
        .hpoint     = 0,
    };
    if (ledc_channel_config(&ccfg) != ESP_OK) {
        return;
    }
    s_bl_pwm = true;
}

void display_st77916_set_brightness(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    if (!s_bl_pwm) {
        /* No PWM (e.g. no BL pin): fall back to a plain GPIO on/off. */
        if (PIN_LCD_BL >= 0) {
            gpio_set_level(PIN_LCD_BL, percent ? 1 : 0);
        }
        return;
    }
    uint32_t duty = (uint32_t)percent * BL_DUTY_MAX / 100;
    ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL);
}

esp_err_t display_st77916_init(esp_lcd_panel_io_handle_t *out_io,
                               esp_lcd_panel_handle_t *out_panel)
{
    if (!out_io || !out_panel) {
        return ESP_ERR_INVALID_ARG;
    }

    backlight_init();

    const spi_bus_config_t buscfg = ST77916_PANEL_BUS_QSPI_CONFIG(
        PIN_LCD_SCLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3,
        DISPLAY_H_RES * DISPLAY_V_RES * DISPLAY_BITS_PER_PIXEL / 8);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    const esp_lcd_panel_io_spi_config_t io_cfg =
        ST77916_PANEL_IO_QSPI_CONFIG(PIN_LCD_CS, NULL, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_cfg, &io));

    st77916_vendor_config_t vendor_cfg = {
        .flags = { .use_qspi_interface = 1 },
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = DISPLAY_BITS_PER_PIXEL,
        .vendor_config  = &vendor_cfg,
    };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    *out_io = io;
    *out_panel = panel;
    ESP_LOGI(TAG, "ST77916 %dx%d up (QSPI, CS=%d SCLK=%d)",
             DISPLAY_H_RES, DISPLAY_V_RES, PIN_LCD_CS, PIN_LCD_SCLK);
    return ESP_OK;
}
