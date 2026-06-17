/*
 * display_st77916.h — ST77916 round panel (QSPI) bring-up + backlight (plan §2.2).
 *
 * Owns the panel: QSPI bus, esp_lcd_st77916 init, and a PWM (LEDC) backlight so
 * the UI can dim at night. The LVGL glue stays in ui.c — this module just hands
 * back the panel + IO handles to register with esp_lvgl_port, plus a brightness
 * control. Pin assignments are centralized here (see the pinout note in the .c);
 * they need confirmation against the JC3636K518 schematic.
 */
#ifndef DISPLAY_ST77916_H
#define DISPLAY_ST77916_H

#include <stdint.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_H_RES          360
#define DISPLAY_V_RES          360
#define DISPLAY_BITS_PER_PIXEL 16

/*
 * Bring up the QSPI bus + ST77916 panel and the backlight (off initially).
 * On success returns ESP_OK and writes the panel + IO handles for the caller
 * to register with esp_lvgl_port.
 */
esp_err_t display_st77916_init(esp_lcd_panel_io_handle_t *out_io,
                               esp_lcd_panel_handle_t *out_panel);

/* Set backlight brightness, 0..100 %. 0 turns the backlight fully off. */
void display_st77916_set_brightness(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_ST77916_H */
