/*
 * station.h — the preset station list and current selection (plan §8/§9).
 *
 * Up to STATION_MAX presets, persisted in NVS (the captive portal will edit
 * them later) with a remembered current index. On first boot the list falls
 * back to a built-in default set. The current index is updated by the encoder /
 * touch and read by the pipeline and UI; index access is atomic.
 */
#ifndef STATION_H
#define STATION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STATION_MAX        5
#define STATION_NAME_MAX   48
#define STATION_TAG_MAX    24
#define STATION_URL_MAX    256

typedef struct {
    const char *name;     /* shown on the display                       */
    const char *tag;      /* short subtitle, e.g. codec/kind            */
    const char *url;      /* stream URL (Icecast MP3/AAC, or HLS .m3u8) */
    const char *favicon;  /* station logo URL for album art (Phase 9)   */
} station_t;

/* Load the playlist + current index from NVS (defaults on first boot). */
void        station_init(void);

size_t      station_count(void);
int         station_current(void);            /* current index */
const station_t *station_get(int index);
const station_t *station_current_station(void);

/* Advance the selection by `delta` (wraps); persists the new index. Returns it. */
int         station_advance(int delta);

/* Replace the whole playlist (e.g. from the captive portal) and persist it.
 * `entries` is an array of `count` (<= STATION_MAX) stations; strings are
 * copied. Resets the current index to 0. Returns the stored count. */
size_t      station_set_playlist(const station_t *entries, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* STATION_H */
