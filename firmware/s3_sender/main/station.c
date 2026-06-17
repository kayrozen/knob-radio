/*
 * station.c — see station.h.
 *
 * The playlist lives in NVS as a JSON array (the portal will rewrite it); the
 * current index is a separate NVS key. Both fall back to sane defaults so the
 * device works out of the box. The public station_t exposes const pointers into
 * the mutable backing store.
 */
#include "station.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "nvs.h"
#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "station";

#define NVS_NS        "preset"
#define NVS_KEY_LIST  "pl_json"
#define NVS_KEY_IDX   "pl_idx"

typedef struct {
    char name[STATION_NAME_MAX];
    char tag[STATION_TAG_MAX];
    char url[STATION_URL_MAX];
    char favicon[STATION_URL_MAX];
    station_sched_t sched;
} entry_t;

static entry_t   s_store[STATION_MAX];
static station_t s_view[STATION_MAX];   /* const-pointer view into s_store */
static size_t    s_count;
static atomic_int s_current = 0;

/* A handful of public streams covering MP3/AAC Icecast and HLS. */
static const station_t s_defaults[] = {
    { "SomaFM Groove Salad", "MP3 stream", "http://ice1.somafm.com/groovesalad-128-mp3", "https://somafm.com/img/groovesalad.jpg", {0} },
    { "SomaFM DEF CON",      "MP3 stream", "http://ice1.somafm.com/defcon-256-mp3",      "https://somafm.com/img/defcon.jpg",      {0} },
    { "SomaFM Drone Zone",   "AAC stream", "http://ice1.somafm.com/dronezone-128-aac",   "https://somafm.com/img/dronezone.jpg",   {0} },
    { "BBC World (HLS)",     "HLS stream", "http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_world_service/bbc_world_service.isml/bbc_world_service-audio%3d96000.norewind.m3u8", "", {0} },
};
#define DEFAULT_COUNT (sizeof(s_defaults) / sizeof(s_defaults[0]))

static void copy_str(char *dst, size_t cap, const char *src)
{
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

static void set_entry(size_t i, const station_t *s)
{
    copy_str(s_store[i].name,    sizeof(s_store[i].name),    s->name);
    copy_str(s_store[i].tag,     sizeof(s_store[i].tag),     s->tag);
    copy_str(s_store[i].url,     sizeof(s_store[i].url),     s->url);
    copy_str(s_store[i].favicon, sizeof(s_store[i].favicon), s->favicon);
    s_store[i].sched = s->sched;
}

/* Rebuild the const-pointer view after the backing store changes. */
static void rebuild_view(void)
{
    for (size_t i = 0; i < s_count; i++) {
        s_view[i].name    = s_store[i].name;
        s_view[i].tag     = s_store[i].tag;
        s_view[i].url     = s_store[i].url;
        s_view[i].favicon = s_store[i].favicon;
        s_view[i].sched   = s_store[i].sched;
    }
}

static void load_defaults(void)
{
    s_count = DEFAULT_COUNT;
    for (size_t i = 0; i < s_count; i++) {
        set_entry(i, &s_defaults[i]);
    }
}

/* Parse a JSON array of {name,tag,url,favicon} into the backing store.
 * Returns the number of entries parsed (0 on failure -> caller keeps defaults). */
static size_t parse_json(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return 0;
    }
    size_t n = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        if (n >= STATION_MAX) {
            break;
        }
        const cJSON *name = cJSON_GetObjectItem(item, "name");
        const cJSON *tag  = cJSON_GetObjectItem(item, "tag");
        const cJSON *url  = cJSON_GetObjectItem(item, "url");
        const cJSON *fav  = cJSON_GetObjectItem(item, "favicon");
        if (!cJSON_IsString(url)) {
            continue;   /* a station needs at least a URL */
        }
        station_t s = {
            .name    = cJSON_IsString(name) ? name->valuestring : "",
            .tag     = cJSON_IsString(tag)  ? tag->valuestring  : "",
            .url     = url->valuestring,
            .favicon = cJSON_IsString(fav)  ? fav->valuestring  : "",
        };
        const cJSON *sch = cJSON_GetObjectItem(item, "sched");
        if (cJSON_IsObject(sch)) {
            const cJSON *mode  = cJSON_GetObjectItem(sch, "mode");
            const cJSON *days  = cJSON_GetObjectItem(sch, "days");
            const cJSON *start = cJSON_GetObjectItem(sch, "start");
            const cJSON *end   = cJSON_GetObjectItem(sch, "end");
            s.sched.mode      = cJSON_IsNumber(mode)  ? (uint8_t)mode->valuedouble  : 0;
            s.sched.days      = cJSON_IsNumber(days)  ? (uint8_t)days->valuedouble  : 0;
            s.sched.start_min = cJSON_IsNumber(start) ? (uint16_t)start->valuedouble : 0;
            s.sched.end_min   = cJSON_IsNumber(end)   ? (uint16_t)end->valuedouble   : 0;
        }
        set_entry(n++, &s);
    }
    cJSON_Delete(root);
    return n;
}

/* Serialize the backing store to a malloc'd JSON string (caller frees). */
static char *serialize_json(void)
{
    cJSON *root = cJSON_CreateArray();
    for (size_t i = 0; i < s_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name",    s_store[i].name);
        cJSON_AddStringToObject(o, "tag",     s_store[i].tag);
        cJSON_AddStringToObject(o, "url",     s_store[i].url);
        cJSON_AddStringToObject(o, "favicon", s_store[i].favicon);
        cJSON *sch = cJSON_CreateObject();
        cJSON_AddNumberToObject(sch, "mode",  s_store[i].sched.mode);
        cJSON_AddNumberToObject(sch, "days",  s_store[i].sched.days);
        cJSON_AddNumberToObject(sch, "start", s_store[i].sched.start_min);
        cJSON_AddNumberToObject(sch, "end",   s_store[i].sched.end_min);
        cJSON_AddItemToObject(o, "sched", sch);
        cJSON_AddItemToArray(root, o);
    }
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

static void save_playlist(void)
{
    char *json = serialize_json();
    if (!json) {
        return;
    }
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, NVS_KEY_LIST, json);
        nvs_commit(h);
        nvs_close(h);
    }
    cJSON_free(json);
}

static void persist_index(int idx)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, NVS_KEY_IDX, (uint8_t)idx);
        nvs_commit(h);
        nvs_close(h);
    }
}

void station_init(void)
{
    load_defaults();

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        /* Playlist: probe length, then read + parse. */
        size_t len = 0;
        if (nvs_get_str(h, NVS_KEY_LIST, NULL, &len) == ESP_OK && len > 0) {
            char *json = malloc(len);
            if (json && nvs_get_str(h, NVS_KEY_LIST, json, &len) == ESP_OK) {
                size_t n = parse_json(json);
                if (n > 0) {
                    s_count = n;
                }
            }
            free(json);
        }
        /* Current index. */
        uint8_t idx = 0;
        if (nvs_get_u8(h, NVS_KEY_IDX, &idx) == ESP_OK && idx < s_count) {
            atomic_store(&s_current, idx);
        }
        nvs_close(h);
    }

    rebuild_view();
    ESP_LOGI(TAG, "%u stations, current=%d", (unsigned)s_count,
             atomic_load(&s_current));
}

size_t station_count(void)
{
    return s_count;
}

int station_current(void)
{
    return atomic_load(&s_current);
}

const station_t *station_get(int index)
{
    if (index < 0 || (size_t)index >= s_count) {
        return NULL;
    }
    return &s_view[index];
}

const station_t *station_current_station(void)
{
    return &s_view[atomic_load(&s_current)];
}

int station_advance(int delta)
{
    int n = (int)s_count;
    int cur = atomic_load(&s_current);
    int next = ((cur + delta) % n + n) % n;   /* wrap, handle negatives */
    atomic_store(&s_current, next);
    persist_index(next);
    return next;
}

void station_set_current(int index)
{
    if (index < 0 || (size_t)index >= s_count) {
        return;
    }
    atomic_store(&s_current, index);
    persist_index(index);
}

size_t station_set_playlist(const station_t *entries, size_t count)
{
    if (count > STATION_MAX) {
        count = STATION_MAX;
    }
    if (count == 0) {
        return s_count;   /* refuse to wipe the list */
    }
    s_count = count;
    for (size_t i = 0; i < count; i++) {
        set_entry(i, &entries[i]);
    }
    atomic_store(&s_current, 0);
    rebuild_view();
    save_playlist();
    persist_index(0);
    return s_count;
}
