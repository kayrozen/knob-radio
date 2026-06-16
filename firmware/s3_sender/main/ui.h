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

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
