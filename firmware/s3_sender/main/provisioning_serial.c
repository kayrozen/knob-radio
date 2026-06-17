/*
 * provisioning_serial.c — see provisioning_serial.h.
 */
#include "provisioning_serial.h"
#include "station.h"
#include "settings.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "cJSON.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "provision";

#define PROVISION_PREFIX  "PROVISION:"
#define LINE_MAX          4096

static void respond(const char *s)
{
    usb_serial_jtag_write_bytes((const uint8_t *)s, strlen(s), pdMS_TO_TICKS(200));
}

/* Map a playlist entry's kind/codec/bitrate to the short subtitle tag. */
static void make_tag(const cJSON *item, char *out, size_t cap)
{
    const cJSON *kind  = cJSON_GetObjectItem(item, "kind");
    const cJSON *codec = cJSON_GetObjectItem(item, "codec");
    const cJSON *brate = cJSON_GetObjectItem(item, "bitrate");

    if (cJSON_IsString(kind) && strcmp(kind->valuestring, "podcast") == 0) {
        snprintf(out, cap, "Podcast");
    } else if (cJSON_IsString(codec) && codec->valuestring[0]) {
        if (cJSON_IsNumber(brate)) {
            snprintf(out, cap, "%s %dk", codec->valuestring, (int)brate->valuedouble);
        } else {
            snprintf(out, cap, "%s", codec->valuestring);
        }
    } else {
        snprintf(out, cap, "Live");
    }
}

/* Parse one PROVISION JSON body, persist name + playlist. Returns true on a
 * usable config (at least one station with a URL). */
static bool apply_provision(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }
    const cJSON *name = cJSON_GetObjectItem(root, "device_name");
    const cJSON *pl   = cJSON_GetObjectItem(root, "playlist");
    if (!cJSON_IsArray(pl)) {
        cJSON_Delete(root);
        return false;
    }
    if (cJSON_IsString(name) && name->valuestring[0]) {
        settings_set_device_name(name->valuestring);
    }

    /* Static backing store for the station_t pointers (this task only). */
    static station_t entries[STATION_MAX];
    static char nbuf[STATION_MAX][STATION_NAME_MAX];
    static char ubuf[STATION_MAX][STATION_URL_MAX];
    static char tbuf[STATION_MAX][STATION_TAG_MAX];

    size_t n = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, pl) {
        if (n >= STATION_MAX) {
            break;
        }
        const cJSON *url = cJSON_GetObjectItem(item, "url");
        if (!cJSON_IsString(url) || !url->valuestring[0]) {
            continue;   /* a preset needs a URL */
        }
        const cJSON *nm = cJSON_GetObjectItem(item, "name");
        strncpy(ubuf[n], url->valuestring, STATION_URL_MAX - 1);
        ubuf[n][STATION_URL_MAX - 1] = '\0';
        strncpy(nbuf[n], cJSON_IsString(nm) ? nm->valuestring : "", STATION_NAME_MAX - 1);
        nbuf[n][STATION_NAME_MAX - 1] = '\0';
        make_tag(item, tbuf[n], STATION_TAG_MAX);

        entries[n].name    = nbuf[n];
        entries[n].tag     = tbuf[n];
        entries[n].url     = ubuf[n];
        entries[n].favicon = "";   /* installer payload carries no logo URL */
        n++;
    }
    cJSON_Delete(root);

    if (n == 0) {
        return false;
    }
    station_set_playlist(entries, n);
    ESP_LOGI(TAG, "provisioned %u presets", (unsigned)n);
    return true;
}

static void provision_task(void *arg)
{
    (void)arg;
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&cfg);

    char *line = malloc(LINE_MAX);
    if (!line) {
        vTaskDelete(NULL);
        return;
    }
    size_t len = 0;
    const size_t plen = strlen(PROVISION_PREFIX);

    for (;;) {
        uint8_t c;
        if (usb_serial_jtag_read_bytes(&c, 1, portMAX_DELAY) != 1) {
            continue;
        }
        if (c == '\r') {
            continue;
        }
        if (c != '\n') {
            if (len < LINE_MAX - 1) {
                line[len++] = (char)c;
            } else {
                len = 0;   /* overrun: drop the line */
            }
            continue;
        }

        line[len] = '\0';
        if (len > plen && strncmp(line, PROVISION_PREFIX, plen) == 0) {
            if (apply_provision(line + plen)) {
                respond("OK\n");
                vTaskDelay(pdMS_TO_TICKS(400));   /* let the reply drain */
                esp_restart();
            } else {
                respond("ERR:invalid provisioning payload\n");
            }
        }
        len = 0;
    }
}

void provisioning_serial_start(void)
{
    xTaskCreate(provision_task, "provision", 6144, NULL, 5, NULL);
}
