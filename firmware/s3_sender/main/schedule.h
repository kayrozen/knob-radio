/*
 * schedule.h — pure auto-play schedule selection (no ESP-IDF deps).
 *
 * Given the presets' schedules and the current local weekday + minute-of-day,
 * pick the preset that should be playing now. Pure C so it is unit-tested on
 * the host (test/host) and reused by the firmware's time-aware wrapper.
 */
#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <stddef.h>
#include "station.h"

#ifdef __cplusplus
extern "C" {
#endif

/* True if `s` is an auto schedule whose window contains (weekday, minute).
 * weekday: 0=Mon .. 6=Sun. minute: 0..1439. Windows may wrap past midnight
 * (end_min < start_min). */
int schedule_active(const station_sched_t *s, int weekday, int minute);

/* Index of the first preset whose schedule is active at (weekday, minute),
 * or -1 if none. */
int schedule_pick(const station_sched_t *scheds, size_t n, int weekday, int minute);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULE_H */
