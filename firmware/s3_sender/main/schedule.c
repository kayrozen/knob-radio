/*
 * schedule.c — see schedule.h.
 */
#include "schedule.h"

int schedule_active(const station_sched_t *s, int weekday, int minute)
{
    if (!s || s->mode != 1) {
        return 0;                         /* manual / on-demand */
    }
    if (weekday < 0 || weekday > 6) {
        return 0;
    }
    if (s->start_min == s->end_min) {
        return 0;                         /* empty window */
    }

    const int wraps = s->start_min > s->end_min;   /* spills past midnight */

    /* A window that STARTS today (the part on its own day). For a wrapping
     * window that's everything from start_min to midnight; otherwise the plain
     * [start, end) interval. */
    if (s->days & (1u << weekday)) {
        if (wraps ? (minute >= s->start_min)
                  : (minute >= s->start_min && minute < s->end_min)) {
            return 1;
        }
    }

    /* The tail of a wrapping window that STARTED yesterday and ran past midnight
     * into today's early hours — active even if today itself isn't scheduled. */
    if (wraps) {
        int prev = (weekday + 6) % 7;     /* yesterday */
        if ((s->days & (1u << prev)) && minute < s->end_min) {
            return 1;
        }
    }

    return 0;
}

int schedule_pick(const station_sched_t *scheds, size_t n, int weekday, int minute)
{
    if (!scheds) {
        return -1;
    }
    for (size_t i = 0; i < n; i++) {
        if (schedule_active(&scheds[i], weekday, minute)) {
            return (int)i;
        }
    }
    return -1;
}
