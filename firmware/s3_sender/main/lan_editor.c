/*
 * lan_editor.c — see lan_editor.h.
 *
 * Serves the installer's preset editor (the same web/install.html + assets,
 * embedded in the firmware) from the device at <name>.local. The page detects
 * it is being served by a device (a successful GET /api/presets) and switches
 * to "device mode": it prefills the current name + presets and, on save, POSTs
 * the config JSON back to /api/presets instead of flashing over USB.
 *
 *   GET  /api/presets  -> { device_name, timezone, playlist:[ ... ] }
 *   POST /api/presets  <- the installer config object; persist + reboot.
 */
#include "lan_editor.h"
#include "station.h"
#include "settings.h"
#include "provision_apply.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "mdns.h"
#include "cJSON.h"

static const char *TAG = "lan_editor";

/* --- Embedded installer bundle (see CMakeLists EMBED_TXTFILES) --- */
extern const char index_html_start[]     asm("_binary_install_html_start");
extern const char index_html_end[]       asm("_binary_install_html_end");
extern const char install_css_start[]    asm("_binary_install_css_start");
extern const char install_css_end[]      asm("_binary_install_css_end");
extern const char led_css_start[]        asm("_binary_led_states_css_start");
extern const char led_css_end[]          asm("_binary_led_states_css_end");
extern const char install_js_start[]     asm("_binary_install_js_start");
extern const char install_js_end[]       asm("_binary_install_js_end");
extern const char preset_css_start[]     asm("_binary_preset_css_start");
extern const char preset_css_end[]       asm("_binary_preset_css_end");
extern const char preset_js_start[]      asm("_binary_preset_js_start");
extern const char preset_js_end[]        asm("_binary_preset_js_end");

static esp_err_t send_asset(httpd_req_t *req, const char *type,
                            const char *start, const char *end)
{
    httpd_resp_set_type(req, type);
    httpd_resp_send(req, start, end - start - 1);   /* -1: drop the trailing NUL */
    return ESP_OK;
}

static esp_err_t h_index(httpd_req_t *r)       { return send_asset(r, "text/html", index_html_start, index_html_end); }
static esp_err_t h_install_css(httpd_req_t *r) { return send_asset(r, "text/css", install_css_start, install_css_end); }
static esp_err_t h_led_css(httpd_req_t *r)     { return send_asset(r, "text/css", led_css_start, led_css_end); }
static esp_err_t h_install_js(httpd_req_t *r)  { return send_asset(r, "application/javascript", install_js_start, install_js_end); }
static esp_err_t h_preset_css(httpd_req_t *r)  { return send_asset(r, "text/css", preset_css_start, preset_css_end); }
static esp_err_t h_preset_js(httpd_req_t *r)   { return send_asset(r, "application/javascript", preset_js_start, preset_js_end); }

/* GET /api/presets -> current config in the installer's payload shape. */
static esp_err_t h_get_presets(httpd_req_t *req)
{
    char name[SETTINGS_STR_MAX], tz[SETTINGS_STR_MAX];
    settings_get_device_name(name);
    settings_get_timezone(tz);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "provision_version", 1);
    cJSON_AddStringToObject(root, "device_name", name);
    cJSON_AddStringToObject(root, "timezone", tz);
    cJSON *arr = cJSON_AddArrayToObject(root, "playlist");

    size_t n = station_count();
    for (size_t i = 0; i < n; i++) {
        const station_t *s = station_get((int)i);
        if (!s) {
            continue;
        }
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", s->name);
        cJSON_AddStringToObject(o, "url", s->url);
        cJSON_AddStringToObject(o, "kind", s->is_podcast ? "podcast" : "live");

        cJSON *sch = cJSON_CreateObject();
        if (s->sched.mode) {
            cJSON_AddStringToObject(sch, "mode", "auto");
            cJSON *days = cJSON_AddArrayToObject(sch, "days");
            for (int d = 0; d < 7; d++) {
                cJSON_AddItemToArray(days, cJSON_CreateNumber((s->sched.days >> d) & 1));
            }
            unsigned smin = s->sched.start_min % 1440u;   /* bound to a day so */
            unsigned emin = s->sched.end_min   % 1440u;   /* HH stays 2 digits */
            char hhmm[6];
            snprintf(hhmm, sizeof(hhmm), "%02u:%02u", smin / 60u, smin % 60u);
            cJSON_AddStringToObject(sch, "start", hhmm);
            snprintf(hhmm, sizeof(hhmm), "%02u:%02u", emin / 60u, emin % 60u);
            cJSON_AddStringToObject(sch, "end", hhmm);
        } else {
            cJSON_AddStringToObject(sch, "mode", "manual");
        }
        cJSON_AddItemToObject(o, "schedule", sch);
        cJSON_AddItemToArray(arr, o);
    }

    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, s ? s : "{}");
    cJSON_free(s);
    return ESP_OK;
}

static void reboot_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1200));
    esp_restart();
}

/* POST /api/presets <- installer config object; persist, ack, reboot. */
static esp_err_t h_post_presets(httpd_req_t *req)
{
    int total = req->content_len;
    if (total <= 0 || total > 8192) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
        return ESP_FAIL;
    }
    char *body = malloc(total + 1);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
        return ESP_FAIL;
    }
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, body + got, total - got);
        if (r <= 0) {
            free(body);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "recv");
            return ESP_FAIL;
        }
        got += r;
    }
    body[total] = '\0';

    bool ok = provision_apply_json(body);
    free(body);
    if (!ok) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid config");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "presets updated over LAN; rebooting");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "ok");
    xTaskCreate(reboot_task, "reboot", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static void http_start(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 12;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 8192;   /* POST handler parses JSON + ~1.8KB provision bufs */
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGW(TAG, "http server did not start");
        return;
    }
    const httpd_uri_t routes[] = {
        { .uri = "/",                   .method = HTTP_GET,  .handler = h_index },
        { .uri = "/index.html",         .method = HTTP_GET,  .handler = h_index },
        { .uri = "/install.html",       .method = HTTP_GET,  .handler = h_index },
        { .uri = "/assets/install.css", .method = HTTP_GET,  .handler = h_install_css },
        { .uri = "/assets/led-states.css", .method = HTTP_GET, .handler = h_led_css },
        { .uri = "/assets/install.js",  .method = HTTP_GET,  .handler = h_install_js },
        { .uri = "/assets/preset.css",  .method = HTTP_GET,  .handler = h_preset_css },
        { .uri = "/assets/preset.js",   .method = HTTP_GET,  .handler = h_preset_js },
        { .uri = "/api/presets",        .method = HTTP_GET,  .handler = h_get_presets },
        { .uri = "/api/presets",        .method = HTTP_POST, .handler = h_post_presets },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
}

void lan_editor_start(void)
{
    char name[SETTINGS_STR_MAX];
    settings_get_device_name(name);

    esp_err_t e = mdns_init();
    if (e == ESP_OK || e == ESP_ERR_INVALID_STATE) {   /* already up is fine */
        mdns_hostname_set(name);                 /* -> <name>.local */
        mdns_instance_name_set("Preset");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
        ESP_LOGI(TAG, "mDNS up: http://%s.local/", name);
    } else {
        ESP_LOGW(TAG, "mDNS init failed; editor still reachable by IP");
    }

    http_start();
}
