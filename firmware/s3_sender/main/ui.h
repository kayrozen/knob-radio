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

/* Initialize the display + LVGL and show the initial station. */
void ui_start(void);

/* Update the displayed station (safe to call from any task). */
void ui_set_station(int index, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
