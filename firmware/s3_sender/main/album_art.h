/*
 * album_art.h — station logo on the board screen (plan §7.3).
 *
 * Fetches a station's favicon over HTTP(S) on a background task and hands the
 * JPEG to LVGL (which decodes it) for the cover tile. This is the guaranteed
 * album-art path for v1: it works in both Bluetooth and analog output modes
 * and has no Bluetooth dependency. Pushing art to the car (AVRCP cover art) is
 * post-v1 R&D (plan §7.1).
 */
#ifndef ALBUM_ART_H
#define ALBUM_ART_H

#ifdef __cplusplus
extern "C" {
#endif

/* Create the fetch task and register LVGL's JPEG decoder. Call after ui_start. */
void album_art_start(void);

/* Request the cover for `url` (the station favicon). An empty/NULL url clears
 * the cover back to the placeholder. The newest request wins; fetching runs off
 * the caller's thread. */
void album_art_load(const char *url);

#ifdef __cplusplus
}
#endif

#endif /* ALBUM_ART_H */
