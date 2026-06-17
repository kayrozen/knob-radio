/*
 * schedule.c — see schedule.h.
 */
#include "schedule.h"

int schedule_active(const station_sched_t *s, int weekday, int minute)
{
    if (!s || s->mode != 1) {
        return 0;                         /* manual / on-demand */
    }
    if (weekday < 0 || weekday > 6 || !(s->days & (1u << weekday))) {
        return 0;                         /* not enabled today */
    }
    if (s->start_min == s->end_min) {
        return 0;                         /* empty window */
    }
    if (s->start_min < s->end_min) {
        return minute >= s->start_min && minute < s->end_min;
    }
    /* Window wraps past midnight (e.g. 22:00 -> 06:00). */
    return minute >= s->start_min || minute < s->end_min;
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
