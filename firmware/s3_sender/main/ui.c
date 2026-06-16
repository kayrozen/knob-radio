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

static lv_obj_t *s_lbl_index;
static lv_obj_t *s_lbl_name;

static void build_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_lbl_index = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_index, lv_color_hex(0x00C0FF), 0);
    lv_obj_align(s_lbl_index, LV_ALIGN_CENTER, 0, -40);

    s_lbl_name = lv_label_create(scr);
    lv_label_set_long_mode(s_lbl_name, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_name, LCD_H_RES - 80);
    lv_obj_set_style_text_color(s_lbl_name, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_lbl_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_name, LV_ALIGN_CENTER, 0, 10);
}

void ui_set_station(int index, const char *name)
{
    if (!s_lbl_name) {
        return;
    }
    /* LVGL is not thread-safe; take the port lock. */
    if (lvgl_port_lock(0)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d / %d", index + 1, (int)station_count());
        lv_label_set_text(s_lbl_index, buf);
        lv_label_set_text(s_lbl_name, name ? name : "");
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

    const station_t *st = station_current_station();
    ui_set_station(station_current(), st ? st->name : "");
    ESP_LOGI(TAG, "UI up on %dx%d round panel", LCD_H_RES, LCD_V_RES);
}
