/*
 * station.c — see station.h.
 */
#include "station.h"
#include <stdatomic.h>

/* A handful of public streams covering MP3/AAC Icecast and HLS, so Phase E can
 * exercise each codec path. Swap freely; the prototype only validates switching
 * and per-codec decode, not the specific stations. */
static const station_t s_stations[] = {
    { "SomaFM Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3" },
    { "SomaFM DEF CON",      "http://ice1.somafm.com/defcon-256-mp3"      },
    { "SomaFM Drone Zone",   "http://ice1.somafm.com/dronezone-128-aac"   },
    { "BBC World (HLS)",     "http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_world_service/bbc_world_service.isml/bbc_world_service-audio%3d96000.norewind.m3u8" },
};

#define STATION_N (sizeof(s_stations) / sizeof(s_stations[0]))

static atomic_int s_current = 0;

void station_init(void)
{
    atomic_store(&s_current, 0);
}

size_t station_count(void)
{
    return STATION_N;
}

int station_current(void)
{
    return atomic_load(&s_current);
}

const station_t *station_get(int index)
{
    if (index < 0 || (size_t)index >= STATION_N) {
        return NULL;
    }
    return &s_stations[index];
}

const station_t *station_current_station(void)
{
    return &s_stations[atomic_load(&s_current)];
}

int station_advance(int delta)
{
    int n = (int)STATION_N;
    int cur = atomic_load(&s_current);
    int next = ((cur + delta) % n + n) % n;   /* wrap, handle negatives */
    atomic_store(&s_current, next);
    return next;
}
