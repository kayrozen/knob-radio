/*
 * station.h — the preset station list and current selection.
 *
 * Tiny, fixed list for the prototype (the plan's Phase E only tests switching,
 * not a full playlist). The current index is updated by the encoder and read
 * by the pipeline and UI; access is atomic.
 */
#ifndef STATION_H
#define STATION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;   /* shown on the display */
    const char *url;    /* stream URL (Icecast MP3/AAC, or HLS .m3u8) */
} station_t;

void        station_init(void);
size_t      station_count(void);
int         station_current(void);            /* current index */
const station_t *station_get(int index);
const station_t *station_current_station(void);

/* Advance the selection by `delta` (wraps). Returns the new index. */
int         station_advance(int delta);

#ifdef __cplusplus
}
#endif

#endif /* STATION_H */
