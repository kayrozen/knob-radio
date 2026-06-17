/*
 * app_main.c — ESP32-S3 sender: PCM -> COBS frame -> UART forward link.
 *
 * TONE build (default): synthesizes a test PCM signal and streams it over the
 * COBS/UART link to the U4WDH bridge (validates link/jitter/drift, phases B-D).
 *
 * ADF build (Phase E): brings up WiFi + the ESP-ADF internet-radio pipeline and
 * streams real radio PCM over the same link; the rotary encoder changes station
 * and (optionally) the LVGL UI shows it. Audio/UART live on core 1, UI/input on
 * core 0, so the UI cannot starve the audio.
 */
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "pcm_source.h"
#include "audio_output.h"
#include "settings.h"

#if defined(CONFIG_PRESET_ENABLE_HAPTIC)
#include "haptic.h"
#endif

#if defined(CONFIG_PRESET_AUDIO_SOURCE_ADF)
#include "station.h"
#include "encoder.h"
#include "wifi_sta.h"
#include "adf_pipeline.h"
#if defined(CONFIG_PRESET_ENABLE_PORTAL)
#include "portal.h"
#endif
#if defined(CONFIG_PRESET_ENABLE_UI)
#include "ui.h"
#include "album_art.h"
#endif
#endif

static const char *TAG = "s3_main";

#if defined(CONFIG_PRESET_AUDIO_SOURCE_ADF)
/* Apply a new station: re-point the pipeline and refresh the UI. Shared by the
 * encoder and the touch buttons (the haptic differs per source — see below). */
static void apply_station(int index)
{
    const station_t *st = station_get(index);
    if (!st) {
        return;
    }
    adf_pipeline_set_url(st->url);   /* PCM pauses briefly; A2DP link survives */
    audio_output_send_metadata(st->name);  /* BT: relay to the car via AVRCP */
#if defined(CONFIG_PRESET_ENABLE_UI)
    ui_set_station(index, st->name);
    album_art_load(st->favicon);     /* fetch + show the station logo */
#endif
}

/* Encoder detent -> a crisp click (plan §6.2: hand is on the knob). */
static void on_encoder(int index)
{
#if defined(CONFIG_PRESET_ENABLE_HAPTIC)
    haptic_play(HAPTIC_CLICK);
#endif
    apply_station(index);
}

#if defined(CONFIG_PRESET_ENABLE_UI)
/* UI touch-button callback: advance the station like an encoder detent, with a
 * lighter tap. The encoder advances internally; the buttons hand us a delta. */
static void on_ui_nav(int delta)
{
#if defined(CONFIG_PRESET_ENABLE_HAPTIC)
    haptic_play(HAPTIC_TICK);
#endif
    apply_station(station_advance(delta));
}
#endif

static void phase_e_start(void)
{
    station_init();

    /* Bring the UI up first so its transient overlay can narrate boot. */
#if defined(CONFIG_PRESET_ENABLE_UI)
    ui_start(on_ui_nav);              /* core 0 */
#endif

    /* Prefer provisioned Wi-Fi credentials; fall back to the build-time ones. */
    char ssid[SETTINGS_STR_MAX], pass[SETTINGS_STR_MAX];
    bool have_creds = settings_get_wifi(ssid, pass);

#if defined(CONFIG_PRESET_ENABLE_PORTAL)
    if (!have_creds) {
        /* Unconfigured: run the captive portal and stay in setup mode. */
        char ap[33] = "";
        portal_start(ap);
#if defined(CONFIG_PRESET_ENABLE_UI)
        ui_show_status(UI_STATUS_WIFI, ap);   /* "join <ap> to set up" */
#endif
        return;
    }
#endif

    const char *use_ssid = have_creds ? ssid : CONFIG_PRESET_WIFI_SSID;
    const char *use_pass = have_creds ? pass : CONFIG_PRESET_WIFI_PASSWORD;
#if defined(CONFIG_PRESET_ENABLE_UI)
    ui_show_status(UI_STATUS_WIFI, use_ssid);
#endif

    if (!wifi_sta_start(use_ssid, use_pass, 0)) {
        ESP_LOGE(TAG, "WiFi did not connect (will keep retrying)");
    }
#if defined(CONFIG_PRESET_ENABLE_UI)
    ui_show_status(UI_STATUS_NONE, NULL);   /* reveal the preset screen */
    album_art_start();
#endif

    const station_t *st = station_current_station();
    adf_pipeline_start(st->url);
#if defined(CONFIG_PRESET_ENABLE_UI)
    album_art_load(st->favicon);            /* logo for the initial station */
#endif

    encoder_start(CONFIG_PRESET_ENC_GPIO_A, CONFIG_PRESET_ENC_GPIO_B,
                  on_encoder);        /* core 0 */
}
#endif

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "hello S3 — PCM -> COBS/UART forward link");

    pcm_source_init();

#if defined(CONFIG_PRESET_ENABLE_HAPTIC)
    haptic_init();
#endif

    /* Output mode: provisioned value (portal) wins over the build-time default. */
    int mode_def = 0;
#if defined(CONFIG_PRESET_OUTPUT_ANALOG)
    mode_def = 1;
#endif
    audio_output_mode_t out_mode = settings_get_output_mode(mode_def)
                                       ? AUDIO_OUTPUT_ANALOG : AUDIO_OUTPUT_BT;
    audio_output_start(out_mode);   /* BT: UART+A2DP / ANALOG: I2S->DAC */

#if defined(CONFIG_PRESET_AUDIO_SOURCE_ADF)
    phase_e_start();
#endif

#if defined(CONFIG_PRESET_ENABLE_HAPTIC)
    haptic_play(HAPTIC_READY);
#endif

    ESP_LOGI(TAG, "free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
}
