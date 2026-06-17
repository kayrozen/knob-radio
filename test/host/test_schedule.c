/*
 * test_schedule.c — host unit tests for the pure auto-play scheduler.
 */
#include "schedule.h"

#include <stdio.h>

static int g_failures = 0;
static int g_checks   = 0;

#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            g_failures++;                                                  \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);                  \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)

#define MON 0
#define TUE 1
#define SAT 5
#define SUN 6
#define WEEKDAYS 0x1F   /* Mon..Fri */

static void test_active(void)
{
    printf("test_active\n");
    /* Mon-Fri, 07:00-09:00. */
    station_sched_t s = { .mode = 1, .days = WEEKDAYS, .start_min = 420, .end_min = 540 };

    CHECK(schedule_active(&s, TUE, 480), "weekday mid-window should be active");
    CHECK(schedule_active(&s, MON, 420), "start edge is inclusive");
    CHECK(!schedule_active(&s, MON, 540), "end edge is exclusive");
    CHECK(!schedule_active(&s, TUE, 360), "before window inactive");
    CHECK(!schedule_active(&s, SAT, 480), "disabled day inactive");

    /* Manual never matches. */
    station_sched_t m = { .mode = 0, .days = 0x7F, .start_min = 0, .end_min = 1439 };
    CHECK(!schedule_active(&m, TUE, 600), "manual never active");

    /* Empty window. */
    station_sched_t e = { .mode = 1, .days = 0x7F, .start_min = 600, .end_min = 600 };
    CHECK(!schedule_active(&e, TUE, 600), "empty window inactive");

    /* Wraps past midnight: 22:00 -> 06:00. */
    station_sched_t w = { .mode = 1, .days = 0x7F, .start_min = 1320, .end_min = 360 };
    CHECK(schedule_active(&w, TUE, 1380), "23:00 active in wrap window");
    CHECK(schedule_active(&w, TUE, 300),  "05:00 active in wrap window");
    CHECK(!schedule_active(&w, TUE, 720), "12:00 inactive in wrap window");
}

static void test_pick(void)
{
    printf("test_pick\n");
    station_sched_t scheds[5] = {
        { .mode = 0 },                                                   /* manual */
        { .mode = 1, .days = WEEKDAYS, .start_min = 360, .end_min = 540 }, /* 06-09 */
        { .mode = 1, .days = WEEKDAYS, .start_min = 900, .end_min = 1080 },/* 15-18 */
        { .mode = 1, .days = 0x60,     .start_min = 540, .end_min = 660 }, /* Sat-Sun 09-11 */
        { .mode = 0 },
    };

    CHECK(schedule_pick(scheds, 5, TUE, 480) == 1, "morning preset picked");
    CHECK(schedule_pick(scheds, 5, TUE, 1000) == 2, "evening preset picked");
    CHECK(schedule_pick(scheds, 5, SAT, 600) == 3, "weekend preset picked");
    CHECK(schedule_pick(scheds, 5, TUE, 720) == -1, "no match -> -1");
    CHECK(schedule_pick(scheds, 5, SUN, 600) == 3, "sunday weekend preset");

    /* First match wins when two windows overlap. */
    station_sched_t overlap[2] = {
        { .mode = 1, .days = 0x7F, .start_min = 0, .end_min = 1439 },
        { .mode = 1, .days = 0x7F, .start_min = 0, .end_min = 1439 },
    };
    CHECK(schedule_pick(overlap, 2, MON, 600) == 0, "first match wins");
}

int main(void)
{
    printf("=== schedule host tests ===\n");
    test_active();
    test_pick();
    printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
