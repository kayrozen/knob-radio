/*
 * touch_cst816.c — see touch_cst816.h.
 */
#include "touch_cst816.h"
#include "board_pins.h"
#include "i2c_bus.h"
#include "display_st77916.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lvgl_port.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "touch";

esp_err_t touch_cst816_init(lv_disp_t *disp)
{
    ESP_RETURN_ON_ERROR(i2c_bus_init(), TAG, "i2c bus");

    /* Touch panel-IO on the shared I2C bus (new i2c_master handle). */
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    io_cfg.scl_speed_hz = I2C_BUS_FREQ_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_bus_handle(), &io_cfg, &io),
                        TAG, "touch io");

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max        = DISPLAY_H_RES,
        .y_max        = DISPLAY_V_RES,
        .rst_gpio_num = BOARD_TP_RST,
        .int_gpio_num = BOARD_TP_INT,
        .levels = {
            .reset     = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy  = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    esp_lcd_touch_handle_t tp = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_cst816s(io, &tp_cfg, &tp),
                        TAG, "cst816s");

    const lvgl_port_touch_cfg_t lv_cfg = {
        .disp   = disp,
        .handle = tp,
    };
    ESP_RETURN_ON_FALSE(lvgl_port_add_touch(&lv_cfg) != NULL, ESP_FAIL,
                        TAG, "lvgl touch");

    ESP_LOGI(TAG, "CST816S touch up (INT=%d RST=%d, shared I2C)",
             BOARD_TP_INT, BOARD_TP_RST);
    return ESP_OK;
}
