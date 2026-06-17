/*
 * ui.c — see ui.h.
 *
 * The round-panel bring-up (ST77916 QSPI + PWM backlight) lives in
 * display_st77916.c; this file owns the LVGL layer (plan §2.4): a preset
 * (main) screen, a settings screen reachable by a tap (brightness, output
 * mode, info), and a transient overlay for boot / Wi-Fi / pairing / error.
 * Panel pins are centralized in display_st77916.c.
 */
#include "ui.h"
#include "station.h"
#include "display_st77916.h"
#include "touch_cst816.h"
#include "audio_output.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nvs.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "ui";

static ui_nav_cb_t s_nav_cb;   /* invoked by the prev/next touch buttons */

#define LCD_H_RES           DISPLAY_H_RES
#define LCD_V_RES           DISPLAY_V_RES
#define LCD_BIT_PER_PIXEL   DISPLAY_BITS_PER_PIXEL

/* --- Brand palette (dark theme, modern orange accent) --- */
#define COLOR_BG      lv_color_hex(0x111111)
#define COLOR_ACCENT  lv_color_hex(0xE5734A)   /* vivid orange   */
#define COLOR_TEXT    lv_color_hex(0xFFFFFF)
#define COLOR_MUTED   lv_color_hex(0x555555)

#define MAX_DOTS      8                        /* preset indicator dots cap */

#define NVS_NS            "preset"
#define NVS_KEY_BRIGHT    "brightness"
#define DEFAULT_BRIGHT    80

static lv_obj_t *s_scr_main;
static lv_obj_t *s_scr_settings;
static lv_obj_t *s_overlay;        /* transient state, on the top layer */
static lv_obj_t *s_overlay_icon;
static lv_obj_t *s_overlay_text;

static lv_obj_t *s_lbl_station;
static lv_obj_t *s_lbl_type;
static lv_obj_t *s_icon_bt;        /* BT status icon (accented in BT mode) */
static lv_obj_t *s_cover_img;      /* album art (hidden until a logo loads) */
static lv_obj_t *s_cover_glyph;    /* placeholder shown when there is no art */
static lv_obj_t *s_dots[MAX_DOTS];
static int       s_dot_count;
static int       s_active_dot;

static uint8_t   s_brightness = DEFAULT_BRIGHT;

/* --------------------------------------------------------------------- */
/* Brightness persistence                                                 */
/* --------------------------------------------------------------------- */

static void brightness_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        uint8_t v = 0;
        if (nvs_get_u8(h, NVS_KEY_BRIGHT, &v) == ESP_OK && v >= 10 && v <= 100) {
            s_brightness = v;
        }
        nvs_close(h);
    }
}

static void brightness_save(uint8_t v)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, NVS_KEY_BRIGHT, v);
        nvs_commit(h);
        nvs_close(h);
    }
}

/* --------------------------------------------------------------------- */
/* Event callbacks                                                        */
/* --------------------------------------------------------------------- */

/* prev/next button -> nav callback. delta passed as the event user-data. */
static void nav_event_cb(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_nav_cb) {
        s_nav_cb(delta);
    }
}

/* play/pause button -> toggle the icon (real pause is a later phase). */
static void play_event_cb(lv_event_t *e)
{
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
    const char *txt = lv_label_get_text(label);
    lv_label_set_text(label, strcmp(txt, LV_SYMBOL_PLAY) == 0
                                 ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void open_settings_cb(lv_event_t *e)
{
    (void)e;
    lv_scr_load(s_scr_settings);
}

static void close_settings_cb(lv_event_t *e)
{
    (void)e;
    lv_scr_load(s_scr_main);
}

static void brightness_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int v = lv_slider_get_value(slider);
    s_brightness = (uint8_t)v;
    display_st77916_set_brightness(s_brightness);
    brightness_save(s_brightness);
}

/* --------------------------------------------------------------------- */
/* Main (preset) screen                                                   */
/* --------------------------------------------------------------------- */

static void build_main(lv_obj_t *scr)
{
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

    /* Settings (gear) button — opens the settings screen (tap at a stop). */
    lv_obj_t *gear = lv_btn_create(scr);
    lv_obj_set_size(gear, 34, 34);
    lv_obj_set_style_radius(gear, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(gear, COLOR_BG, 0);
    lv_obj_set_style_border_color(gear, COLOR_MUTED, 0);
    lv_obj_set_style_border_width(gear, 1, 0);
    lv_obj_align(gear, LV_ALIGN_TOP_MID, 105, 40);
    lv_obj_add_event_cb(gear, open_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *gear_lbl = lv_label_create(gear);
    lv_label_set_text(gear_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear_lbl, COLOR_MUTED, 0);
    lv_obj_center(gear_lbl);

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
    lv_obj_set_style_clip_corner(cover, true, 0);   /* round the art too */
    lv_obj_align(cover, LV_ALIGN_CENTER, 0, -35);
    s_cover_glyph = lv_label_create(cover);
    lv_label_set_text(s_cover_glyph, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(s_cover_glyph, COLOR_ACCENT, 0);
    lv_obj_center(s_cover_glyph);
    /* Album-art image overlays the glyph; hidden until a logo is fetched. */
    s_cover_img = lv_img_create(cover);
    lv_obj_center(s_cover_img);
    lv_obj_add_flag(s_cover_img, LV_OBJ_FLAG_HIDDEN);

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

    /* Control bar: prev / play / next. */
    lv_obj_t *btn_prev = lv_btn_create(scr);
    lv_obj_set_size(btn_prev, 45, 45);
    lv_obj_set_style_radius(btn_prev, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_prev, COLOR_BG, 0);
    lv_obj_set_style_border_color(btn_prev, COLOR_MUTED, 0);
    lv_obj_set_style_border_width(btn_prev, 1, 0);
    lv_obj_align(btn_prev, LV_ALIGN_CENTER, -65, 85);
    lv_obj_add_event_cb(btn_prev, nav_event_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)-1);
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
    lv_obj_add_event_cb(btn_play, play_event_cb, LV_EVENT_CLICKED, l_play);

    lv_obj_t *btn_next = lv_btn_create(scr);
    lv_obj_set_size(btn_next, 45, 45);
    lv_obj_set_style_radius(btn_next, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_next, COLOR_BG, 0);
    lv_obj_set_style_border_color(btn_next, COLOR_MUTED, 0);
    lv_obj_set_style_border_width(btn_next, 1, 0);
    lv_obj_align(btn_next, LV_ALIGN_CENTER, 65, 85);
    lv_obj_add_event_cb(btn_next, nav_event_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)1);
    lv_obj_t *l_next = lv_label_create(btn_next);
    lv_label_set_text(l_next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(l_next, COLOR_TEXT, 0);
    lv_obj_center(l_next);

    /* System status row: wifi / bluetooth / battery. The BT icon is accented
     * when Bluetooth output is the active sink (vs analog AUX). */
    lv_obj_t *status = lv_obj_create(scr);
    lv_obj_remove_style_all(status);
    lv_obj_set_size(status, 110, 20);
    lv_obj_set_flex_flow(status, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -15);

    lv_obj_t *icon_wifi = lv_label_create(status);
    lv_label_set_text(icon_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(icon_wifi, COLOR_MUTED, 0);

    s_icon_bt = lv_label_create(status);
    lv_label_set_text(s_icon_bt, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(s_icon_bt,
        audio_output_mode() == AUDIO_OUTPUT_BT ? COLOR_ACCENT : COLOR_MUTED, 0);

    lv_obj_t *icon_batt = lv_label_create(status);
    lv_label_set_text(icon_batt, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(icon_batt, COLOR_MUTED, 0);
}

/* --------------------------------------------------------------------- */
/* Settings screen (touch, at a stop): brightness, output mode, info      */
/* --------------------------------------------------------------------- */

static void build_settings(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    /* Back button. */
    lv_obj_t *back = lv_btn_create(scr);
    lv_obj_set_size(back, 34, 34);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(back, COLOR_BG, 0);
    lv_obj_set_style_border_color(back, COLOR_MUTED, 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_align(back, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_add_event_cb(back, close_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT, 0);
    lv_obj_center(back_lbl);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 80);

    /* Brightness. */
    lv_obj_t *bl = lv_label_create(scr);
    lv_label_set_text(bl, LV_SYMBOL_EYE_OPEN "  Brightness");
    lv_obj_set_style_text_color(bl, COLOR_MUTED, 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_align(bl, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 180);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, s_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, COLOR_MUTED, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, COLOR_ACCENT, LV_PART_KNOB);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(slider, brightness_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Output mode (read-only here; runtime switching is the portal's job). */
    lv_obj_t *mode = lv_label_create(scr);
    lv_label_set_text(mode, audio_output_mode() == AUDIO_OUTPUT_ANALOG
                                ? LV_SYMBOL_AUDIO "  Output: Analog (AUX)"
                                : LV_SYMBOL_BLUETOOTH "  Output: Bluetooth");
    lv_obj_set_style_text_color(mode, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(mode, &lv_font_montserrat_14, 0);
    lv_obj_align(mode, LV_ALIGN_CENTER, 0, 45);

    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text(info, "Preset radio");
    lv_obj_set_style_text_color(info, COLOR_MUTED, 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);
    lv_obj_align(info, LV_ALIGN_BOTTOM_MID, 0, -30);
}

/* --------------------------------------------------------------------- */
/* Transient state overlay (top layer, over any screen)                   */
/* --------------------------------------------------------------------- */

static void build_overlay(void)
{
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_bg_color(s_overlay, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_center(s_overlay);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    s_overlay_icon = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_overlay_icon, COLOR_ACCENT, 0);
    lv_obj_align(s_overlay_icon, LV_ALIGN_CENTER, 0, -20);

    s_overlay_text = lv_label_create(s_overlay);
    lv_label_set_long_mode(s_overlay_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_overlay_text, LCD_H_RES - 100);
    lv_obj_set_style_text_align(s_overlay_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_overlay_text, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_overlay_text, &lv_font_montserrat_14, 0);
    lv_obj_align(s_overlay_text, LV_ALIGN_CENTER, 0, 20);
}

void ui_show_status(ui_status_t status, const char *detail)
{
    if (!s_overlay) {
        return;
    }
    if (!lvgl_port_lock(0)) {
        return;
    }
    if (status == UI_STATUS_NONE) {
        lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
        lvgl_port_unlock();
        return;
    }

    const char *icon = LV_SYMBOL_REFRESH;
    const char *msg  = "Starting";
    switch (status) {
    case UI_STATUS_WIFI:    icon = LV_SYMBOL_WIFI;      msg = "Connecting Wi-Fi"; break;
    case UI_STATUS_PAIRING: icon = LV_SYMBOL_BLUETOOTH; msg = "Pairing";          break;
    case UI_STATUS_ERROR:   icon = LV_SYMBOL_WARNING;   msg = "Error";            break;
    case UI_STATUS_BOOT:
    default:                icon = LV_SYMBOL_REFRESH;   msg = "Starting";         break;
    }
    lv_label_set_text(s_overlay_icon, icon);
    lv_obj_set_style_text_color(s_overlay_icon,
        status == UI_STATUS_ERROR ? lv_color_hex(0xE05050) : COLOR_ACCENT, 0);

    char buf[96];
    if (detail && detail[0]) {
        snprintf(buf, sizeof(buf), "%s\n%s", msg, detail);
    } else {
        snprintf(buf, sizeof(buf), "%s", msg);
    }
    lv_label_set_text(s_overlay_text, buf);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();
}

/* --------------------------------------------------------------------- */

void ui_set_cover(const void *dsc)
{
    if (!s_cover_img) {
        return;
    }
    if (!lvgl_port_lock(0)) {
        return;
    }
    if (dsc) {
        lv_img_set_src(s_cover_img, dsc);
        /* Scale the decoded logo to fit the ~60px tile, centered. */
        lv_img_set_pivot(s_cover_img, 0, 0);
        lv_coord_t iw = lv_obj_get_width(s_cover_img);
        lv_coord_t ih = lv_obj_get_height(s_cover_img);
        lv_coord_t m = iw > ih ? iw : ih;
        if (m > 0) {
            lv_img_set_zoom(s_cover_img, (uint16_t)(60 * 256 / m));
        }
        lv_obj_center(s_cover_img);
        lv_obj_clear_flag(s_cover_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_cover_glyph, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_cover_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_cover_glyph, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

void ui_set_station(int index, const char *name)
{
    if (!s_lbl_station) {
        return;
    }
    if (lvgl_port_lock(0)) {
        lv_label_set_text(s_lbl_station, name ? name : "");

        const station_t *st = station_get(index);
        lv_label_set_text(s_lbl_type, (st && st->tag) ? st->tag : "");

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

void ui_start(ui_nav_cb_t nav_cb)
{
    s_nav_cb = nav_cb;
    brightness_load();

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
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

    /* CST816S touch as an LVGL indev. Non-fatal: panel + encoder still work. */
    esp_err_t terr = touch_cst816_init(disp);
    if (terr != ESP_OK) {
        ESP_LOGW(TAG, "touch unavailable: %s (buttons inert, encoder still works)",
                 esp_err_to_name(terr));
    }

    if (lvgl_port_lock(0)) {
        s_scr_main     = lv_obj_create(NULL);
        s_scr_settings = lv_obj_create(NULL);
        build_main(s_scr_main);
        build_settings(s_scr_settings);
        build_overlay();
        lv_scr_load(s_scr_main);
        lvgl_port_unlock();
    }

    s_active_dot = -1;   /* no dot lit yet; ui_set_station lights the current */
    const station_t *st = station_current_station();
    ui_set_station(station_current(), st ? st->name : "");

    /* Raise the backlight to the saved level now that there is content. */
    display_st77916_set_brightness(s_brightness);
    ESP_LOGI(TAG, "UI up on %dx%d round panel (brightness %u%%)",
             LCD_H_RES, LCD_V_RES, s_brightness);
}
