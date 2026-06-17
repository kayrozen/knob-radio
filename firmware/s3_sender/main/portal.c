/*
 * portal.c — see portal.h.
 */
#include "portal.h"
#include "settings.h"
#include "station.h"

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "lwip/sockets.h"

static const char *TAG = "portal";

#define AP_IP   "192.168.4.1"

/* ----------------------------------------------------------------------- *
 *  DNS: answer every A query with our AP IP so any URL opens the portal.
 * ----------------------------------------------------------------------- */
static void dns_task(void *arg)
{
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        vTaskDelete(NULL);
        return;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    uint8_t buf[512];
    for (;;) {
        struct sockaddr_in src;
        socklen_t sl = sizeof(src);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&src, &sl);
        if (n < (int)sizeof(uint16_t) * 6) {
            continue;
        }
        /* Turn the query into a response: set QR + answer count, append an
         * A record pointing at the AP IP (a pointer to the question name). */
        buf[2] |= 0x80;            /* QR = response */
        buf[3] |= 0x80;            /* RA */
        buf[7] = 1;                /* ANCOUNT = 1 */
        if (n + 16 > (int)sizeof(buf)) {
            continue;
        }
        uint8_t *p = buf + n;
        *p++ = 0xC0; *p++ = 0x0C;             /* name: pointer to offset 12 */
        *p++ = 0x00; *p++ = 0x01;             /* type A */
        *p++ = 0x00; *p++ = 0x01;             /* class IN */
        *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x3C;  /* TTL 60 */
        *p++ = 0x00; *p++ = 0x04;             /* RDLENGTH 4 */
        *p++ = 192; *p++ = 168; *p++ = 4; *p++ = 1;          /* RDATA */
        sendto(sock, buf, p - buf, 0, (struct sockaddr *)&src, sl);
    }
}

/* ----------------------------------------------------------------------- *
 *  HTTP: one config page + a save handler; everything else redirects here.
 * ----------------------------------------------------------------------- */

/* URL-decode `in` into `out` (cap bytes), in place of '+' and %XX escapes. */
static void url_decode(const char *in, char *out, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < cap; i++) {
        if (in[i] == '+') {
            out[o++] = ' ';
        } else if (in[i] == '%' && in[i + 1] && in[i + 2]) {
            char h[3] = { in[i + 1], in[i + 2], 0 };
            out[o++] = (char)strtol(h, NULL, 16);
            i += 2;
        } else {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
}

/* Extract form field `key` from an x-www-form-urlencoded `body` into `out`. */
static bool form_field(const char *body, const char *key, char *out, size_t cap)
{
    char pat[40];
    snprintf(pat, sizeof(pat), "%s=", key);
    const char *p = strstr(body, pat);
    if (!p) {
        out[0] = '\0';
        return false;
    }
    p += strlen(pat);
    const char *end = strchr(p, '&');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    char raw[256];
    if (len >= sizeof(raw)) {
        len = sizeof(raw) - 1;
    }
    memcpy(raw, p, len);
    raw[len] = '\0';
    url_decode(raw, out, cap);
    return true;
}

static esp_err_t root_get(httpd_req_t *req)
{
    char name[SETTINGS_STR_MAX];
    char ssid[SETTINGS_STR_MAX];
    char pass[SETTINGS_STR_MAX];
    settings_get_device_name(name);
    settings_get_wifi(ssid, pass);
    int mode = settings_get_output_mode(0);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req,
        "<!DOCTYPE html><html><head><meta name=viewport "
        "content='width=device-width,initial-scale=1'>"
        "<title>Preset setup</title><style>body{font-family:sans-serif;"
        "background:#111;color:#eee;max-width:480px;margin:auto;padding:16px}"
        "input,select{width:100%;padding:8px;margin:4px 0 12px;box-sizing:border-box;"
        "background:#222;color:#eee;border:1px solid #444;border-radius:6px}"
        "button{background:#E5734A;color:#fff;border:0;padding:12px;width:100%;"
        "border-radius:8px;font-size:16px}h1{color:#E5734A}h3{margin-bottom:0}"
        "</style></head><body><h1>Preset</h1><form method=POST action=/save>");

    char line[640];
    snprintf(line, sizeof(line),
        "<h3>Device name</h3><input name=name value='%s'>"
        "<h3>Wi-Fi network</h3><input name=ssid value='%s' placeholder=SSID>"
        "<input name=pass type=password placeholder=Password>"
        "<h3>Output</h3><select name=mode>"
        "<option value=0 %s>Bluetooth (car)</option>"
        "<option value=1 %s>Analog line-out (AUX)</option></select>",
        name, ssid, mode == 0 ? "selected" : "", mode == 1 ? "selected" : "");
    httpd_resp_sendstr_chunk(req, line);

    httpd_resp_sendstr_chunk(req, "<h3>Presets</h3>");
    for (size_t i = 0; i < STATION_MAX; i++) {
        const station_t *s = station_get((int)i);
        snprintf(line, sizeof(line),
            "<input name=n%u value='%s' placeholder='Station %u name'>"
            "<input name=u%u value='%s' placeholder='Stream URL'>",
            (unsigned)i, s ? s->name : "", (unsigned)(i + 1),
            (unsigned)i, s ? s->url : "");
        httpd_resp_sendstr_chunk(req, line);
    }

    httpd_resp_sendstr_chunk(req, "<button type=submit>Save &amp; reboot</button>"
                                  "</form></body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t save_post(httpd_req_t *req)
{
    int total = req->content_len;
    if (total <= 0 || total > 4096) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad form");
        return ESP_FAIL;
    }
    char *body = malloc(total + 1);
    if (!body) {
        return ESP_FAIL;
    }
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, body + got, total - got);
        if (r <= 0) {
            free(body);
            return ESP_FAIL;
        }
        got += r;
    }
    body[total] = '\0';

    char name[SETTINGS_STR_MAX], ssid[SETTINGS_STR_MAX], pass[SETTINGS_STR_MAX], mode[8];
    if (form_field(body, "name", name, sizeof(name)) && name[0]) {
        settings_set_device_name(name);
    }
    if (form_field(body, "ssid", ssid, sizeof(ssid)) && ssid[0]) {
        form_field(body, "pass", pass, sizeof(pass));
        settings_set_wifi(ssid, pass);
    }
    if (form_field(body, "mode", mode, sizeof(mode))) {
        settings_set_output_mode(atoi(mode));
    }

    /* Rebuild the playlist from the n0/u0.. fields (URL required per slot). */
    static station_t entries[STATION_MAX];
    static char nbuf[STATION_MAX][STATION_NAME_MAX];
    static char ubuf[STATION_MAX][STATION_URL_MAX];
    size_t count = 0;
    for (size_t i = 0; i < STATION_MAX; i++) {
        char k[8];
        snprintf(k, sizeof(k), "u%u", (unsigned)i);
        if (!form_field(body, k, ubuf[count], STATION_URL_MAX) || !ubuf[count][0]) {
            continue;
        }
        snprintf(k, sizeof(k), "n%u", (unsigned)i);
        form_field(body, k, nbuf[count], STATION_NAME_MAX);
        entries[count].name    = nbuf[count];
        entries[count].tag     = "";
        entries[count].url     = ubuf[count];
        entries[count].favicon = "";
        count++;
    }
    if (count > 0) {
        station_set_playlist(entries, count);
    }
    free(body);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, "<html><body style='font-family:sans-serif;background:"
        "#111;color:#eee;text-align:center;padding-top:40px'><h2>Saved.</h2>"
        "<p>Rebooting…</p></body></html>");

    ESP_LOGI(TAG, "config saved (%u presets); rebooting", (unsigned)count);
    vTaskDelay(pdMS_TO_TICKS(1200));
    esp_restart();
    return ESP_OK;
}

/* Any other path -> 302 to the portal root (captive-portal probe handling). */
static esp_err_t redirect_get(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" AP_IP "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static void http_start(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.lru_purge_enable = true;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        return;
    }
    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_get };
    httpd_uri_t save = { .uri = "/save", .method = HTTP_POST, .handler = save_post };
    httpd_uri_t any  = { .uri = "/*", .method = HTTP_GET, .handler = redirect_get };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
    httpd_register_uri_handler(server, &any);
}

void portal_start(char *ap_ssid_out)
{
    /* Shared with wifi_sta init; tolerate already-initialized. */
    esp_netif_init();
    esp_err_t e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "event loop: %s", esp_err_to_name(e));
    }
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wcfg);

    /* SSID: "<device name>" trimmed to fit; open network for easy setup. */
    char name[SETTINGS_STR_MAX];
    settings_get_device_name(name);

    wifi_config_t ap = {
        .ap = {
            .channel        = 1,
            .max_connection = 4,
            .authmode       = WIFI_AUTH_OPEN,
        },
    };
    size_t slen = strnlen(name, sizeof(ap.ap.ssid));
    memcpy(ap.ap.ssid, name, slen);
    ap.ap.ssid_len = slen;
    if (ap_ssid_out) {
        memcpy(ap_ssid_out, name, slen);
        ap_ssid_out[slen] = '\0';
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    xTaskCreate(dns_task, "portal_dns", 3072, NULL, 5, NULL);
    http_start();

    ESP_LOGI(TAG, "captive portal up: join AP '%s', open http://%s/", name, AP_IP);
}
