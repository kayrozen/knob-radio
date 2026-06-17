/*
 * time_sync.h — wall-clock time for the auto-play scheduler.
 *
 * Applies the stored timezone, starts SNTP (the S3 has WiFi), and exposes the
 * current local weekday + minute-of-day so the scheduler can decide which
 * preset should be playing now.
 */
#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Apply the timezone (from settings) and start SNTP. `on_first_sync` (may be
 * NULL) is called once, the first time the clock is set. */
void time_sync_start(void (*on_first_sync)(void));

/* Current local time as weekday (0=Mon..6=Sun) + minute-of-day (0..1439).
 * Returns false until the clock has been set by SNTP. */
bool time_now_local(int *weekday, int *minute);

#ifdef __cplusplus
}
#endif

#endif /* TIME_SYNC_H */
