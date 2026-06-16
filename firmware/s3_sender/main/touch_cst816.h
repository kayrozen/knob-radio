/*
 * touch_cst816.h — CST816S capacitive touch -> LVGL input device (plan §2.2/§2.3).
 *
 * The CST816S sits on the S3's shared I2C bus (i2c_bus.c); this module creates
 * the esp_lcd_touch device on that bus and registers it with esp_lvgl_port as
 * an LVGL indev, so touch reaches the UI widgets. The rotary encoder stays the
 * primary control (safe while driving); touch is the secondary input.
 */
#ifndef TOUCH_CST816_H
#define TOUCH_CST816_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bring up the CST816S on the shared I2C bus and attach it to `disp` as an
 * LVGL touch indev. The bus is initialized on demand (idempotent). */
esp_err_t touch_cst816_init(lv_disp_t *disp);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_CST816_H */
