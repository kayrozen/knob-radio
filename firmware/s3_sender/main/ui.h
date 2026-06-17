/*
 * ui.h — LVGL UI on the round display (Phase E).
 *
 * Shows the current station. Runs entirely on core 0 (LVGL task + tick) so it
 * cannot starve the audio/UART work pinned to core 1 — the plan's "UI ne starve
 * pas l'audio" requirement.
 *
 * NOTE: the panel bring-up (ST77916 QSPI, JC3636W518V2) is board-specific and
 * has not been validated on hardware; pins live in ui.c. The LVGL layer above
 * it is generic.
 */
#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Navigation request from a UI control (touch buttons). delta = -1 prev / +1
 * next; the handler advances the station, exactly like an encoder detent. */
typedef void (*ui_nav_cb_t)(int delta);

/* Initialize the display + LVGL and show the initial station. `nav_cb` (may be
 * NULL) is invoked when the on-screen prev/next buttons are tapped. */
void ui_start(ui_nav_cb_t nav_cb);

/* Update the displayed station (safe to call from any task). */
void ui_set_station(int index, const char *name);

/* Set the cover-art image from an `lv_img_dsc_t *` (passed as a void* to keep
 * LVGL out of this header). NULL restores the placeholder glyph. Safe to call
 * from any task. */
void ui_set_cover(const void *lv_img_dsc);

/* Transient full-screen states (boot, Wi-Fi, pairing, error). UI_STATUS_NONE
 * dismisses the overlay and returns to the preset screen. `detail` (may be
 * NULL) is appended to the message, e.g. an SSID or error string. Safe to call
 * from any task. */
typedef enum {
    UI_STATUS_NONE = 0,
    UI_STATUS_BOOT,
    UI_STATUS_WIFI,
    UI_STATUS_PAIRING,
    UI_STATUS_ERROR,
} ui_status_t;

void ui_show_status(ui_status_t status, const char *detail);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
