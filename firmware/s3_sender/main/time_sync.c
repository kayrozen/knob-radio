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

/* IANA -> POSIX TZ map (the installer sends the browser's IANA name). Covers
 * the population-heavy zones per continent; DST rules per the 2024+ tzdata.
 * A stored value that is already a POSIX TZ string passes straight through, so
 * an unlisted zone can always be provisioned explicitly. */
static const struct { const char *iana; const char *posix; } TZ_MAP[] = {
    /* North America */
    { "America/Toronto",     "EST5EDT,M3.2.0,M11.1.0"   },
    { "America/Montreal",    "EST5EDT,M3.2.0,M11.1.0"   },
    { "America/New_York",    "EST5EDT,M3.2.0,M11.1.0"   },
    { "America/Detroit",     "EST5EDT,M3.2.0,M11.1.0"   },
    { "America/Chicago",     "CST6CDT,M3.2.0,M11.1.0"   },
    { "America/Winnipeg",    "CST6CDT,M3.2.0,M11.1.0"   },
    { "America/Denver",      "MST7MDT,M3.2.0,M11.1.0"   },
    { "America/Edmonton",    "MST7MDT,M3.2.0,M11.1.0"   },
    { "America/Phoenix",     "MST7"                     },
    { "America/Regina",      "CST6"                     },
    { "America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"   },
    { "America/Vancouver",   "PST8PDT,M3.2.0,M11.1.0"   },
    { "America/Tijuana",     "PST8PDT,M3.2.0,M11.1.0"   },
    { "America/Anchorage",   "AKST9AKDT,M3.2.0,M11.1.0" },
    { "Pacific/Honolulu",    "HST10"                    },
    { "America/Halifax",     "AST4ADT,M3.2.0,M11.1.0"   },
    { "America/St_Johns",    "NST3:30NDT,M3.2.0,M11.1.0" },
    { "America/Mexico_City", "CST6"                     },
    /* Central / South America */
    { "America/Bogota",      "<-05>5"                   },
    { "America/Lima",        "<-05>5"                   },
    { "America/Caracas",     "<-04>4"                   },
    { "America/Santiago",    "<-04>4<-03>,M9.1.6/24,M4.1.6/24" },
    { "America/Sao_Paulo",   "<-03>3"                   },
    { "America/Argentina/Buenos_Aires", "<-03>3"        },
    /* Europe */
    { "Europe/London",       "GMT0BST,M3.5.0/1,M10.5.0" },
    { "Europe/Dublin",       "IST-1GMT0,M10.5.0,M3.5.0/1" },
    { "Europe/Lisbon",       "WET0WEST,M3.5.0/1,M10.5.0" },
    { "Europe/Paris",        "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Brussels",     "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Madrid",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Berlin",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Zurich",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Rome",         "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Amsterdam",    "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Vienna",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Prague",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Warsaw",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Budapest",     "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Stockholm",    "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Oslo",         "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Copenhagen",   "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Helsinki",     "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Athens",       "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Bucharest",    "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Kyiv",         "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Istanbul",     "<+03>-3"                  },
    { "Europe/Moscow",       "MSK-3"                    },
    /* Africa / Middle East */
    { "Africa/Casablanca",   "<+01>-1"                  },
    { "Africa/Lagos",        "WAT-1"                    },
    { "Africa/Cairo",        "EET-2EEST,M4.5.5/0,M10.5.4/24" },
    { "Africa/Johannesburg", "SAST-2"                   },
    { "Africa/Nairobi",      "EAT-3"                    },
    { "Asia/Jerusalem",      "IST-2IDT,M3.4.4/26,M10.5.0" },
    { "Asia/Dubai",          "<+04>-4"                  },
    { "Asia/Riyadh",         "<+03>-3"                  },
    /* Asia */
    { "Asia/Karachi",        "PKT-5"                    },
    { "Asia/Kolkata",        "IST-5:30"                 },
    { "Asia/Dhaka",          "<+06>-6"                  },
    { "Asia/Bangkok",        "<+07>-7"                  },
    { "Asia/Jakarta",        "WIB-7"                    },
    { "Asia/Singapore",      "<+08>-8"                  },
    { "Asia/Kuala_Lumpur",   "<+08>-8"                  },
    { "Asia/Hong_Kong",      "HKT-8"                    },
    { "Asia/Shanghai",       "CST-8"                    },
    { "Asia/Taipei",         "CST-8"                    },
    { "Asia/Manila",         "PST-8"                    },
    { "Asia/Seoul",          "KST-9"                    },
    { "Asia/Tokyo",          "JST-9"                    },
    /* Oceania */
    { "Australia/Perth",     "AWST-8"                   },
    { "Australia/Adelaide",  "ACST-9:30ACDT,M10.1.0,M4.1.0/3" },
    { "Australia/Brisbane",  "AEST-10"                  },
    { "Australia/Sydney",    "AEST-10AEDT,M10.1.0,M4.1.0/3" },
    { "Australia/Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
    { "Pacific/Auckland",    "NZST-12NZDT,M9.5.0,M4.1.0/3" },
    { "UTC",                 "UTC0" },
    { "Etc/UTC",             "UTC0" },
};

/* An IANA name always contains '/' (except UTC, mapped above); a POSIX TZ
 * string never does but always carries a digit ("EST5EDT", "<+07>-7"). Lets an
 * unlisted zone be provisioned directly as a POSIX rule. */
static bool looks_posix(const char *tz)
{
    if (strchr(tz, '/') && !strchr(tz, ',')) {
        return false;   /* "Area/City" shape */
    }
    for (const char *p = tz; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            return true;
        }
    }
    return false;
}

static const char *iana_to_posix(const char *iana)
{
    for (size_t i = 0; i < sizeof(TZ_MAP) / sizeof(TZ_MAP[0]); i++) {
        if (strcmp(iana, TZ_MAP[i].iana) == 0) {
            return TZ_MAP[i].posix;
        }
    }
    if (looks_posix(iana)) {
        return iana;   /* already a POSIX TZ rule: use verbatim */
    }
    ESP_LOGW(TAG, "timezone '%s' not in the map -> schedules will run in UTC; "
             "provision a POSIX TZ string to override", iana);
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
