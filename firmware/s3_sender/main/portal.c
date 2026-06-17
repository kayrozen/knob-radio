/*
 * portal.c — see portal.h.
 *
 * Single-phase captive portal (plan §3) serving the designed two-step flow:
 *   /          -> WiFi page  (scan real networks, save creds, connect STA)
 *   /bluetooth -> Bluetooth page (scan + pair the car/speaker via the U4WDH)
 * Static assets (HTML/CSS/JS) are embedded in the firmware. WiFi is scanned on
 * the STA interface (the AP stays up, APSTA); BT scan/pair is orchestrated over
 * the control plane (BT_SCAN_START -> BT_SCAN_RESULT -> BT_PAIR). Pairing
 * finishes setup and reboots into normal operation.
 */
#include "portal.h"
#include "settings.h"
#include "pcm_link_proto.h"
#include "uart_writer.h"
#include "backpressure_rx.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "lwip/sockets.h"

static const char *TAG = "portal";
#define AP_IP   "192.168.4.1"

/* --- Embedded web bundle (see CMakeLists EMBED_TXTFILES) --- */
extern const char wifi_html_start[]   asm("_binary_wifi_html_start");
extern const char wifi_html_end[]     asm("_binary_wifi_html_end");
extern const char bt_html_start[]     asm("_binary_bluetooth_html_start");
extern const char bt_html_end[]       asm("_binary_bluetooth_html_end");
extern const char preset_css_start[]  asm("_binary_preset_css_start");
extern const char preset_css_end[]    asm("_binary_preset_css_end");
extern const char portal_css_start[]  asm("_binary_portal_css_start");
extern const char portal_css_end[]    asm("_binary_portal_css_end");
extern const char preset_js_start[]   asm("_binary_preset_js_start");
extern const char preset_js_end[]     asm("_binary_preset_js_end");
extern const char portal_js_start[]   asm("_binary_portal_js_start");
extern const char portal_js_end[]     asm("_binary_portal_js_end");

/* --- BT scan results, filled from the control-plane BT_SCAN_RESULT frames --- */
typedef struct { uint8_t mac[6]; char name[40]; } bt_dev_t;
static bt_dev_t s_bt[12];
static int      s_bt_n;
static SemaphoreHandle_t s_bt_lock;

static void on_scan_result(const uint8_t *mac, const char *name)
{
    if (!s_bt_lock) {
        return;
    }
    xSemaphoreTake(s_bt_lock, portMAX_DELAY);
    for (int i = 0; i < s_bt_n; i++) {
        if (memcmp(s_bt[i].mac, mac, 6) == 0) {     /* already have it */
            xSemaphoreGive(s_bt_lock);
            return;
        }
    }
    if (s_bt_n < (int)(sizeof(s_bt) / sizeof(s_bt[0]))) {
        memcpy(s_bt[s_bt_n].mac, mac, 6);
        strncpy(s_bt[s_bt_n].name, name, sizeof(s_bt[s_bt_n].name) - 1);
        s_bt[s_bt_n].name[sizeof(s_bt[s_bt_n].name) - 1] = '\0';
        s_bt_n++;
    }
    xSemaphoreGive(s_bt_lock);
}

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
        buf[2] |= 0x80;
        buf[3] |= 0x80;
        buf[7] = 1;
        if (n + 16 > (int)sizeof(buf)) {
            continue;
        }
        uint8_t *p = buf + n;
        *p++ = 0xC0; *p++ = 0x0C;
        *p++ = 0x00; *p++ = 0x01;
        *p++ = 0x00; *p++ = 0x01;
        *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x3C;
        *p++ = 0x00; *p++ = 0x04;
        *p++ = 192; *p++ = 168; *p++ = 4; *p++ = 1;
        sendto(sock, buf, p - buf, 0, (struct sockaddr *)&src, sl);
    }
}

/* ----------------------------------------------------------------------- *
 *  Form parsing (x-www-form-urlencoded)
 * ----------------------------------------------------------------------- */
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

static int read_body(httpd_req_t *req, char *buf, size_t cap)
{
    int total = req->content_len;
    if (total <= 0 || (size_t)total >= cap) {
        return -1;
    }
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, buf + got, total - got);
        if (r <= 0) {
            return -1;
        }
        got += r;
    }
    buf[total] = '\0';
    return total;
}

/* ----------------------------------------------------------------------- *
 *  Static asset + page handlers
 * ----------------------------------------------------------------------- */
static esp_err_t send_asset(httpd_req_t *req, const char *type,
                            const char *start, const char *end)
{
    httpd_resp_set_type(req, type);
    httpd_resp_send(req, start, end - start - 1);   /* -1: drop the trailing NUL */
    return ESP_OK;
}

static esp_err_t h_wifi(httpd_req_t *r)   { return send_asset(r, "text/html", wifi_html_start, wifi_html_end); }
static esp_err_t h_bt(httpd_req_t *r)     { return send_asset(r, "text/html", bt_html_start, bt_html_end); }
static esp_err_t h_preset_css(httpd_req_t *r) { return send_asset(r, "text/css", preset_css_start, preset_css_end); }
static esp_err_t h_portal_css(httpd_req_t *r) { return send_asset(r, "text/css", portal_css_start, portal_css_end); }
static esp_err_t h_preset_js(httpd_req_t *r)  { return send_asset(r, "application/javascript", preset_js_start, preset_js_end); }
static esp_err_t h_portal_js(httpd_req_t *r)  { return send_asset(r, "application/javascript", portal_js_start, portal_js_end); }

static esp_err_t h_done(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/html");
    httpd_resp_sendstr(r, "<html><body style='font-family:sans-serif;background:#111;"
        "color:#eee;text-align:center;padding-top:40px'><h2>All set.</h2>"
        "<p>Manage your presets from this device's page on your network once it's "
        "online.</p></body></html>");
    return ESP_OK;
}

/* GET /api/scan -> { "networks": [ {ssid,rssi,auth}, ... ] } */
static esp_err_t h_api_scan(httpd_req_t *req)
{
    wifi_scan_config_t sc = { 0 };
    esp_wifi_scan_start(&sc, true);
    uint16_t num = 0;
    esp_wifi_scan_get_ap_num(&num);
    if (num > 20) {
        num = 20;
    }
    wifi_ap_record_t *recs = calloc(num ? num : 1, sizeof(wifi_ap_record_t));
    if (recs) {
        esp_wifi_scan_get_ap_records(&num, recs);
    } else {
        num = 0;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *arr  = cJSON_AddArrayToObject(root, "networks");
    for (int i = 0; i < num; i++) {
        if (!recs[i].ssid[0]) {
            continue;
        }
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "ssid", (char *)recs[i].ssid);
        cJSON_AddNumberToObject(o, "rssi", recs[i].rssi);
        cJSON_AddNumberToObject(o, "auth", recs[i].authmode != WIFI_AUTH_OPEN ? 1 : 0);
        cJSON_AddItemToArray(arr, o);
    }
    free(recs);
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s ? s : "{}");
    cJSON_free(s);
    return ESP_OK;
}

/* POST /api/wifi  ssid=..&pass=..  -> save + connect (AP stays up). */
static esp_err_t h_api_wifi(httpd_req_t *req)
{
    char body[320];
    if (read_body(req, body, sizeof(body)) < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad form");
        return ESP_FAIL;
    }
    char ssid[SETTINGS_STR_MAX], pass[SETTINGS_STR_MAX];
    if (!form_field(body, "ssid", ssid, sizeof(ssid)) || !ssid[0]) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "no ssid");
        return ESP_FAIL;
    }
    form_field(body, "pass", pass, sizeof(pass));
    settings_set_wifi(ssid, pass);

    /* Best-effort STA join so the user sees "linked"; AP stays up (APSTA). */
    wifi_config_t sta = { 0 };
    strncpy((char *)sta.sta.ssid, ssid, sizeof(sta.sta.ssid) - 1);
    strncpy((char *)sta.sta.password, pass, sizeof(sta.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    esp_wifi_connect();

    ESP_LOGI(TAG, "wifi saved: %s", ssid);
    httpd_resp_sendstr(req, "ok");
    return ESP_OK;
}

/* GET /api/bt/scan -> ask the bridge to discover sinks, collect, return them. */
static esp_err_t h_api_bt_scan(httpd_req_t *req)
{
    if (!s_bt_lock) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no lock");
        return ESP_FAIL;
    }
    xSemaphoreTake(s_bt_lock, portMAX_DELAY);
    s_bt_n = 0;
    xSemaphoreGive(s_bt_lock);

    uart_writer_send_control(PCM_LINK_CTRL_BT_SCAN_START, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(3500));   /* let results arrive */

    cJSON *root = cJSON_CreateObject();
    cJSON *arr  = cJSON_AddArrayToObject(root, "devices");
    xSemaphoreTake(s_bt_lock, portMAX_DELAY);
    for (int i = 0; i < s_bt_n; i++) {
        char mac[18];
        snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                 s_bt[i].mac[0], s_bt[i].mac[1], s_bt[i].mac[2],
                 s_bt[i].mac[3], s_bt[i].mac[4], s_bt[i].mac[5]);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", s_bt[i].name[0] ? s_bt[i].name : mac);
        cJSON_AddStringToObject(o, "mac", mac);
        cJSON_AddNumberToObject(o, "rssi", -60);   /* bridge reports name only */
        cJSON_AddItemToArray(arr, o);
    }
    xSemaphoreGive(s_bt_lock);

    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s ? s : "{}");
    cJSON_free(s);
    return ESP_OK;
}

/* Reboot shortly after responding, into normal operation. */
static void reboot_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

/* POST /api/bt  mac=aa:bb:..  -> pair, finish setup, reboot. */
static esp_err_t h_api_bt(httpd_req_t *req)
{
    char body[96];
    if (read_body(req, body, sizeof(body)) < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad form");
        return ESP_FAIL;
    }
    char macs[20];
    unsigned m[6];
    if (!form_field(body, "mac", macs, sizeof(macs)) ||
        sscanf(macs, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad mac");
        return ESP_FAIL;
    }
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
        if (m[i] > 0xFF) {                  /* reject out-of-range octets */
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad mac");
            return ESP_FAIL;
        }
        mac[i] = (uint8_t)m[i];
    }
    uart_writer_send_control(PCM_LINK_CTRL_BT_PAIR, mac, 6);
    ESP_LOGI(TAG, "pair %s; rebooting into normal mode", macs);
    httpd_resp_sendstr(req, "ok");
    xTaskCreate(reboot_task, "reboot", 2048, NULL, 5, NULL);
    return ESP_OK;
}

/* Any other GET -> the WiFi page (captive-portal probe handling). */
static esp_err_t h_catch_all(httpd_req_t *req)
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
    cfg.max_uri_handlers = 16;
    cfg.lru_purge_enable = true;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        return;
    }
    const httpd_uri_t routes[] = {
        { .uri = "/",            .method = HTTP_GET,  .handler = h_wifi },
        { .uri = "/bluetooth",   .method = HTTP_GET,  .handler = h_bt },
        { .uri = "/done",        .method = HTTP_GET,  .handler = h_done },
        { .uri = "/preset.css",  .method = HTTP_GET,  .handler = h_preset_css },
        { .uri = "/portal.css",  .method = HTTP_GET,  .handler = h_portal_css },
        { .uri = "/preset.js",   .method = HTTP_GET,  .handler = h_preset_js },
        { .uri = "/portal.js",   .method = HTTP_GET,  .handler = h_portal_js },
        { .uri = "/api/scan",    .method = HTTP_GET,  .handler = h_api_scan },
        { .uri = "/api/wifi",    .method = HTTP_POST, .handler = h_api_wifi },
        { .uri = "/api/bt/scan", .method = HTTP_GET,  .handler = h_api_bt_scan },
        { .uri = "/api/bt",      .method = HTTP_POST, .handler = h_api_bt },
        { .uri = "/*",           .method = HTTP_GET,  .handler = h_catch_all },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
}

void portal_start(char *ap_ssid_out)
{
    esp_netif_init();
    esp_err_t e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "event loop: %s", esp_err_to_name(e));
    }
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();   /* for STA scan + join */

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wcfg);

    char name[SETTINGS_STR_MAX];
    settings_get_device_name(name);
    wifi_config_t ap = {
        .ap = { .channel = 1, .max_connection = 4, .authmode = WIFI_AUTH_OPEN },
    };
    size_t slen = strnlen(name, sizeof(ap.ap.ssid));
    memcpy(ap.ap.ssid, name, slen);
    ap.ap.ssid_len = slen;
    if (ap_ssid_out) {
        memcpy(ap_ssid_out, name, slen);
        ap_ssid_out[slen] = '\0';
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* BT scan/pair runs over the control plane: bring the forward link up and
     * collect BT_SCAN_RESULT frames from the bridge. */
    s_bt_lock = xSemaphoreCreateMutex();
    uart_writer_init_link();
    backpressure_rx_set_scan_cb(on_scan_result);
    backpressure_rx_start();

    xTaskCreate(dns_task, "portal_dns", 3072, NULL, 5, NULL);
    http_start();

    ESP_LOGI(TAG, "captive portal up: join AP '%s', open http://%s/", name, AP_IP);
}
