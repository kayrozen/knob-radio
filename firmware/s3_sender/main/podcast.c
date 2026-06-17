/*
 * podcast.c — see podcast.h.
 */
#include "podcast.h"
#include "podcast_parse.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "nvs.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"

static const char *TAG = "podcast";

/* The newest enclosure sits near the top of the feed; cap the download so a big
 * feed doesn't cost much RAM/time. */
#define FEED_MAX_BYTES   (192 * 1024)

bool podcast_resolve(const char *feed_url, char *out, size_t cap)
{
    if (!feed_url || !out || cap == 0) {
        return false;
    }
    esp_http_client_config_t cfg = {
        .url               = feed_url,
        .timeout_ms        = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        return false;
    }

    bool ok = false;
    char *buf = malloc(FEED_MAX_BYTES + 1);
    if (buf && esp_http_client_open(c, 0) == ESP_OK) {
        esp_http_client_fetch_headers(c);
        int total = 0, r;
        while (total < FEED_MAX_BYTES &&
               (r = esp_http_client_read(c, buf + total, FEED_MAX_BYTES - total)) > 0) {
            total += r;
        }
        buf[total] = '\0';
        esp_http_client_close(c);
        if (podcast_extract_enclosure(buf, out, cap) > 0) {
            ok = true;
            ESP_LOGI(TAG, "resolved latest episode: %s", out);
        } else {
            ESP_LOGW(TAG, "no enclosure in feed: %s", feed_url);
        }
    }
    free(buf);
    esp_http_client_cleanup(c);
    return ok;
}

/* ----------------------------------------------------------------------- *
 *  Resume position (per feed), keyed by a hash of the feed URL.
 * ----------------------------------------------------------------------- */
#define NVS_NS  "podpos"

static void pos_key(const char *url, char *key /* >=10 */)
{
    uint32_t h = 2166136261u;            /* FNV-1a */
    for (const char *p = url; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    snprintf(key, 10, "p%08lx", (unsigned long)h);
}

uint32_t podcast_pos_get(const char *feed_url)
{
    if (!feed_url) {
        return 0;
    }
    char key[10];
    pos_key(feed_url, key);
    nvs_handle_t h;
    uint32_t v = 0;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u32(h, key, &v);
        nvs_close(h);
    }
    return v;
}

void podcast_pos_set(const char *feed_url, uint32_t byte_offset)
{
    if (!feed_url) {
        return;
    }
    char key[10];
    pos_key(feed_url, key);
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, key, byte_offset);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "saved position %lu", (unsigned long)byte_offset);
}
