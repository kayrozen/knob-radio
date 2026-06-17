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
#include "esp_heap_caps.h"
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
    /* 192 KB is hard to find contiguous in internal RAM once fragmented; prefer
     * PSRAM (present on the R8 module), falling back to internal if there's none. */
    char *buf = heap_caps_malloc(FEED_MAX_BYTES + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        buf = malloc(FEED_MAX_BYTES + 1);
    }
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
 *  Resume position, keyed by a hash of the feed URL. The record also pins the
 *  episode (a hash of its audio URL) so a newer episode resets the position.
 * ----------------------------------------------------------------------- */
#define NVS_NS  "podpos"

typedef struct {
    uint32_t episode;   /* hash of the episode audio URL the offset belongs to */
    uint32_t offset;    /* source byte offset within that episode              */
} pos_rec_t;

static uint32_t fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    for (; s && *s; s++) {
        h ^= (uint8_t)*s;
        h *= 16777619u;
    }
    return h;
}

static void pos_key(const char *feed_url, char *key /* >=10 */)
{
    snprintf(key, 10, "p%08lx", (unsigned long)fnv1a(feed_url));
}

uint32_t podcast_pos_get(const char *feed_url, const char *episode_url)
{
    if (!feed_url || !episode_url) {
        return 0;
    }
    char key[10];
    pos_key(feed_url, key);
    pos_rec_t rec = { 0, 0 };
    nvs_handle_t h;
    size_t sz = sizeof(rec);
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_blob(h, key, &rec, &sz);
        nvs_close(h);
    }
    if (rec.episode != fnv1a(episode_url)) {
        return 0;   /* a newer episode dropped -> start it from the beginning */
    }
    return rec.offset;
}

void podcast_pos_set(const char *feed_url, const char *episode_url,
                     uint32_t byte_offset)
{
    if (!feed_url || !episode_url) {
        return;
    }
    char key[10];
    pos_key(feed_url, key);
    pos_rec_t rec = { fnv1a(episode_url), byte_offset };
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, key, &rec, sizeof(rec));
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "saved position %lu", (unsigned long)byte_offset);
}
