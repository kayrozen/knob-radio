/*
 * ui.c — see ui.h.
 *
 * The round-panel bring-up (ST77916 QSPI + PWM backlight) lives in
 * display_st77916.c; this file owns the LVGL layer on top: esp_lvgl_port glue
 * and the preset screen. Panel pins are centralized in display_st77916.c.
 */
#include "ui.h"
#include "station.h"
#include "display_st77916.h"

#include <string.h>

#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "ui";

#define LCD_H_RES           DISPLAY_H_RES
#define LCD_V_RES           DISPLAY_V_RES
#define LCD_BIT_PER_PIXEL   DISPLAY_BITS_PER_PIXEL

/* --- Brand palette (dark theme, modern orange accent) --- */
#define COLOR_BG      lv_color_hex(0x111111)
#define COLOR_ACCENT  lv_color_hex(0xE5734A)   /* vivid orange   */
#define COLOR_TEXT    lv_color_hex(0xFFFFFF)
#define COLOR_MUTED   lv_color_hex(0x555555)

#define MAX_DOTS      8                        /* preset indicator dots cap */

static lv_obj_t *s_lbl_station;
static lv_obj_t *s_lbl_type;
static lv_obj_t *s_dots[MAX_DOTS];
static int       s_dot_count;
static int       s_active_dot;

/* Build the preset screen (round 360x360). Layout, top to bottom:
 * decorative ring, "PRESET" caption, preset dots, cover-art tile, station name
 * + type, the prev/play/next control bar, and a wifi/bt/battery status row.
 *
 * Navigation is driven by the rotary encoder (see encoder.c); the control-bar
 * buttons are drawn to match the design but are not wired to a touch input yet
 * (no touch driver is brought up on this board). Adding an LVGL touch indev and
 * routing the buttons through the same station-change path is a follow-up. */
static void build_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    /* Decorative outer ring (progress/volume placeholder). */
    lv_obj_t *arc = lv_arc_create(scr);
    lv_obj_set_size(arc, 300, 300);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc, COLOR_MUTED, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_arc_set_value(arc, 40);
    lv_obj_center(arc);

    /* "PRESET" caption. */
    lv_obj_t *cap = lv_label_create(scr);
    lv_label_set_text(cap, "PRESET");
    lv_obj_set_style_text_color(cap, COLOR_MUTED, 0);
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
    lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 30);

    /* Preset dots — one per station. */
    s_dot_count = (int)station_count();
    if (s_dot_count > MAX_DOTS) {
        s_dot_count = MAX_DOTS;
    }
    lv_obj_t *dots = lv_obj_create(scr);
    lv_obj_remove_style_all(dots);
    lv_obj_set_size(dots, 20 * s_dot_count, 15);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(dots, LV_ALIGN_TOP_MID, 0, 55);
    for (int i = 0; i < s_dot_count; i++) {
        s_dots[i] = lv_obj_create(dots);
        lv_obj_set_size(s_dots[i], 8, 8);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_dots[i], 0, 0);
        lv_obj_set_style_bg_color(s_dots[i], COLOR_MUTED, 0);
    }

    /* Cover-art tile (rounded square) with a placeholder audio glyph. */
    lv_obj_t *cover = lv_obj_create(scr);
    lv_obj_set_size(cover, 65, 65);
    lv_obj_set_style_radius(cover, 12, 0);
    lv_obj_set_style_bg_color(cover, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_color(cover, COLOR_MUTED, 0);
    lv_obj_set_style_border_width(cover, 1, 0);
    lv_obj_align(cover, LV_ALIGN_CENTER, 0, -35);
    lv_obj_t *cover_icon = lv_label_create(cover);
    lv_label_set_text(cover_icon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(cover_icon, COLOR_ACCENT, 0);
    lv_obj_center(cover_icon);

    /* Station name + type. */
    s_lbl_station = lv_label_create(scr);
    lv_label_set_long_mode(s_lbl_station, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_station, LCD_H_RES - 120);
    lv_obj_set_style_text_align(s_lbl_station, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_lbl_station, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_lbl_station, &lv_font_montserrat_20, 0);
    lv_obj_align(s_lbl_station, LV_ALIGN_CENTER, 0, 20);

    s_lbl_type = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_type, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_lbl_type, &lv_font_montserrat_12, 0);
    lv_obj_align(s_lbl_type, LV_ALIGN_CENTER, 0, 46);

    /* Control bar: prev / play / next (visual; encoder drives navigation). */
    lv_obj_t *btn_prev = lv_btn_create(scr);
    lv_obj_set_size(btn_prev, 45, 45);
    lv_obj_set_style_radius(btn_prev, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_prev, COLOR_BG, 0);
    lv_obj_set_style_border_color(btn_prev, COLOR_MUTED, 0);
    lv_obj_set_style_border_width(btn_prev, 1, 0);
    lv_obj_align(btn_prev, LV_ALIGN_CENTER, -65, 85);
    lv_obj_t *l_prev = lv_label_create(btn_prev);
    lv_label_set_text(l_prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_color(l_prev, COLOR_TEXT, 0);
    lv_obj_center(l_prev);

    lv_obj_t *btn_play = lv_btn_create(scr);
    lv_obj_set_size(btn_play, 55, 55);
    lv_obj_set_style_radius(btn_play, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_play, COLOR_ACCENT, 0);
    lv_obj_align(btn_play, LV_ALIGN_CENTER, 0, 85);
    lv_obj_t *l_play = lv_label_create(btn_play);
    lv_label_set_text(l_play, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_color(l_play, COLOR_TEXT, 0);
    lv_obj_center(l_play);

    lv_obj_t *btn_next = lv_btn_create(scr);
    lv_obj_set_size(btn_next, 45, 45);
    lv_obj_set_style_radius(btn_next, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_next, COLOR_BG, 0);
    lv_obj_set_style_border_color(btn_next, COLOR_MUTED, 0);
    lv_obj_set_style_border_width(btn_next, 1, 0);
    lv_obj_align(btn_next, LV_ALIGN_CENTER, 65, 85);
    lv_obj_t *l_next = lv_label_create(btn_next);
    lv_label_set_text(l_next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(l_next, COLOR_TEXT, 0);
    lv_obj_center(l_next);

    /* System status row: wifi / bluetooth / battery. */
    lv_obj_t *status = lv_obj_create(scr);
    lv_obj_remove_style_all(status);
    lv_obj_set_size(status, 110, 20);
    lv_obj_set_flex_flow(status, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -15);
    const char *icons[] = { LV_SYMBOL_WIFI, LV_SYMBOL_BLUETOOTH, LV_SYMBOL_BATTERY_FULL };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *ic = lv_label_create(status);
        lv_label_set_text(ic, icons[i]);
        lv_obj_set_style_text_color(ic, COLOR_MUTED, 0);
    }
}

void ui_set_station(int index, const char *name)
{
    if (!s_lbl_station) {
        return;
    }
    /* LVGL is not thread-safe; take the port lock. */
    if (lvgl_port_lock(0)) {
        lv_label_set_text(s_lbl_station, name ? name : "");

        const station_t *st = station_get(index);
        lv_label_set_text(s_lbl_type, (st && st->tag) ? st->tag : "");

        /* Move the active preset dot. */
        if (s_active_dot >= 0 && s_active_dot < s_dot_count) {
            lv_obj_set_style_bg_color(s_dots[s_active_dot], COLOR_MUTED, 0);
        }
        if (index >= 0 && index < s_dot_count) {
            lv_obj_set_style_bg_color(s_dots[index], COLOR_ACCENT, 0);
            s_active_dot = index;
        }
        lvgl_port_unlock();
    }
}

void ui_start(void)
{
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(display_st77916_init(&io, &panel));

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = io,
        .panel_handle  = panel,
        .buffer_size   = LCD_H_RES * LCD_V_RES / 8,
        .double_buffer = true,
        .hres          = LCD_H_RES,
        .vres          = LCD_V_RES,
        .flags = { .buff_dma = true },
    };
    lvgl_port_add_disp(&disp_cfg);

    if (lvgl_port_lock(0)) {
        build_screen();
        lvgl_port_unlock();
    }

    s_active_dot = -1;   /* no dot lit yet; ui_set_station lights the current */
    const station_t *st = station_current_station();
    ui_set_station(station_current(), st ? st->name : "");

    /* Raise the backlight now that the screen shows content, not noise. */
    display_st77916_set_brightness(100);
    ESP_LOGI(TAG, "UI up on %dx%d round panel", LCD_H_RES, LCD_V_RES);
}
