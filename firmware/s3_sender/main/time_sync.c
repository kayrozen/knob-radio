/*
 * time_sync.c — see time_sync.h.
 */
#include "time_sync.h"
#include "settings.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

#include "esp_sntp.h"
#include "esp_log.h"

static const char *TAG = "time";

static void (*s_on_first_sync)(void);
static bool s_synced;

/* Minimal IANA -> POSIX TZ map (the installer sends the browser's IANA name).
 * Falls back to UTC for anything not listed; extend as needed. */
static const struct { const char *iana; const char *posix; } TZ_MAP[] = {
    { "America/Toronto",     "EST5EDT,M3.2.0,M11.1.0"   },
    { "America/Montreal",    "EST5EDT,M3.2.0,M11.1.0"   },
    { "America/New_York",    "EST5EDT,M3.2.0,M11.1.0"   },
    { "America/Chicago",     "CST6CDT,M3.2.0,M11.1.0"   },
    { "America/Denver",      "MST7MDT,M3.2.0,M11.1.0"   },
    { "America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"   },
    { "America/Vancouver",   "PST8PDT,M3.2.0,M11.1.0"   },
    { "America/Halifax",     "AST4ADT,M3.2.0,M11.1.0"   },
    { "Europe/Paris",        "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Brussels",     "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Madrid",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Berlin",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Zurich",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/London",       "GMT0BST,M3.5.0/1,M10.5.0"   },
    { "UTC",                 "UTC0" },
};

static const char *iana_to_posix(const char *iana)
{
    for (size_t i = 0; i < sizeof(TZ_MAP) / sizeof(TZ_MAP[0]); i++) {
        if (strcmp(iana, TZ_MAP[i].iana) == 0) {
            return TZ_MAP[i].posix;
        }
    }
    return "UTC0";
}

static void on_sntp(struct timeval *tv)
{
    (void)tv;
    if (!s_synced) {
        s_synced = true;
        ESP_LOGI(TAG, "clock set");
        if (s_on_first_sync) {
            s_on_first_sync();
        }
    }
}

void time_sync_start(void (*on_first_sync)(void))
{
    s_on_first_sync = on_first_sync;

    char iana[SETTINGS_STR_MAX];
    settings_get_timezone(iana);
    const char *posix = iana_to_posix(iana);
    setenv("TZ", posix, 1);
    tzset();
    ESP_LOGI(TAG, "tz %s -> %s", iana, posix);

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(on_sntp);
    esp_sntp_init();
}

bool time_now_local(int *weekday, int *minute)
{
    if (!s_synced) {
        return false;
    }
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year < (2023 - 1900)) {
        return false;   /* not actually set yet */
    }
    if (weekday) {
        *weekday = (tm.tm_wday + 6) % 7;   /* tm_wday 0=Sun -> 0=Mon */
    }
    if (minute) {
        *minute = tm.tm_hour * 60 + tm.tm_min;
    }
    return true;
}
