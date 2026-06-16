/*
 * ui.c — see ui.h.
 *
 * Panel: Guition JC3636W518V2, 360x360 round, ST77916 over QSPI. Brought up via
 * the managed esp_lcd_st77916 driver + esp_lvgl_port. Pin assignments below are
 * the best-known values for this module and MUST be confirmed against the board
 * before trusting the display; they do not affect whether the firmware builds.
 */
#include "ui.h"
#include "station.h"

#include <string.h>

#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_st77916.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "ui";

/* --- JC3636W518V2 panel pins (QSPI). VERIFY against the board. --- */
#define LCD_HOST            SPI2_HOST
#define LCD_H_RES           360
#define LCD_V_RES           360
#define LCD_BIT_PER_PIXEL   16

#define PIN_LCD_SCLK        40
#define PIN_LCD_CS          21
#define PIN_LCD_D0          46
#define PIN_LCD_D1          45
#define PIN_LCD_D2          42
#define PIN_LCD_D3          41
#define PIN_LCD_RST         -1
#define PIN_LCD_BL          5

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
    if (PIN_LCD_BL >= 0) {
        gpio_config_t bk = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << PIN_LCD_BL,
        };
        gpio_config(&bk);
        gpio_set_level(PIN_LCD_BL, 1);
    }

    const spi_bus_config_t buscfg = ST77916_PANEL_BUS_QSPI_CONFIG(
        PIN_LCD_SCLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3,
        LCD_H_RES * LCD_V_RES * LCD_BIT_PER_PIXEL / 8);
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
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config  = &vendor_cfg,
    };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

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
    ESP_LOGI(TAG, "UI up on %dx%d round panel", LCD_H_RES, LCD_V_RES);
}
