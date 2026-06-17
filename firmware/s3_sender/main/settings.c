/*
 * settings.c — see settings.h.
 */
#include "settings.h"

#include <string.h>
#include "nvs.h"

#define NS              "preset"
#define KEY_SSID        "wifi_ssid"
#define KEY_PASS        "wifi_pass"
#define KEY_NAME        "device_name"
#define KEY_OUTMODE     "output_mode"
#define KEY_TZ          "tz"

#define DEFAULT_NAME    "Preset Radio"
#define DEFAULT_TZ      "America/Toronto"

static bool get_str(const char *key, char *out, size_t cap)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t len = cap;
    esp_err_t err = nvs_get_str(h, key, out, &len);
    nvs_close(h);
    return err == ESP_OK && out[0] != '\0';
}

static void set_str(const char *key, const char *val)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, key, val ? val : "");
        nvs_commit(h);
        nvs_close(h);
    }
}

bool settings_get_wifi(char *ssid, char *pass)
{
    pass[0] = '\0';
    get_str(KEY_PASS, pass, SETTINGS_STR_MAX);
    return get_str(KEY_SSID, ssid, SETTINGS_STR_MAX);
}

void settings_set_wifi(const char *ssid, const char *pass)
{
    set_str(KEY_SSID, ssid);
    set_str(KEY_PASS, pass);
}

void settings_get_device_name(char *out)
{
    if (!get_str(KEY_NAME, out, SETTINGS_STR_MAX)) {
        strcpy(out, DEFAULT_NAME);
    }
}

void settings_set_device_name(const char *name)
{
    set_str(KEY_NAME, name);
}

int settings_get_output_mode(int def)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return def;
    }
    uint8_t v = (uint8_t)def;
    esp_err_t err = nvs_get_u8(h, KEY_OUTMODE, &v);
    nvs_close(h);
    return err == ESP_OK ? v : def;
}

void settings_set_output_mode(int mode)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, KEY_OUTMODE, (uint8_t)(mode ? 1 : 0));
        nvs_commit(h);
        nvs_close(h);
    }
}

void settings_get_timezone(char *out)
{
    if (!get_str(KEY_TZ, out, SETTINGS_STR_MAX)) {
        strcpy(out, DEFAULT_TZ);
    }
}

void settings_set_timezone(const char *iana)
{
    set_str(KEY_TZ, iana);
}
