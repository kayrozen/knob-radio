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
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "pcm_source.h"
#include "audio_output.h"
#include "backpressure_rx.h"
#include "pcm_link_proto.h"
#include "settings.h"

#if defined(CONFIG_PRESET_ENABLE_PROVISION)
#include "provisioning_serial.h"
#endif

#if defined(CONFIG_PRESET_ENABLE_HAPTIC)
#include "haptic.h"
#endif

#if defined(CONFIG_PRESET_AUDIO_SOURCE_ADF)
#include "station.h"
#include "encoder.h"
#include "wifi_sta.h"
#include "adf_pipeline.h"
#include "schedule.h"
#include "time_sync.h"
#include "podcast.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#if defined(CONFIG_PRESET_ENABLE_PORTAL)
#include "portal.h"
#endif
#if defined(CONFIG_PRESET_ENABLE_LAN_EDITOR)
#include "lan_editor.h"
#endif
#if defined(CONFIG_PRESET_ENABLE_DEEPSLEEP)
#include "power_sleep.h"
#endif
#if defined(CONFIG_PRESET_ENABLE_UI)
#include "ui.h"
#include "album_art.h"
#endif
#endif

static const char *TAG = "s3_main";

#if defined(CONFIG_PRESET_AUDIO_SOURCE_ADF)
static int  s_playing_idx = -1;   /* preset currently feeding the pipeline */
static char s_playing_episode[STATION_URL_MAX];  /* resolved podcast episode URL */
static uint32_t s_disc_pos;       /* podcast byte position at BT disconnect */
static bool s_long_gap;           /* BT was gone > 10 s -> resume on connect */
static esp_timer_handle_t s_disc_timer;

/* Playback worker: resolving a podcast feed (blocking HTTPS, up to 8 s) and
 * writing the resume position to NVS (blocking flash) must NOT run on the
 * encoder/UI/return-channel/esp_timer tasks — they would freeze the controls or
 * stall the system timer. Those tasks post a request here and return; this one
 * worker does the blocking work and is the sole caller into the pipeline, which
 * also serialises pipeline access. */
typedef enum { PLAY_APPLY, PLAY_SAVE_POS, PLAY_SCHEDULE } play_op_t;
typedef struct { play_op_t op; int index; bool save_outgoing; } play_req_t;
static QueueHandle_t s_play_q;

static void schedule_apply_blocking(void);   /* defined below; runs on worker */

/* Resolve a preset to its play URL + resume offset. A podcast's URL is an RSS
 * feed: fetch it for the latest episode and resume at its saved byte position;
 * a live station plays its URL from the start. Returns false if unresolvable. */
static bool station_playspec(int index, char *url, size_t cap, uint32_t *offset)
{
    const station_t *st = station_get(index);
    if (!st) {
        return false;
    }
    if (st->is_podcast) {
        if (!podcast_resolve(st->url, url, cap)) {
            return false;
        }
        /* Resume only if it is still the same episode (else start the new one). */
        *offset = podcast_pos_get(st->url, url);
    } else {
        strncpy(url, st->url, cap - 1);
        url[cap - 1] = '\0';
        *offset = 0;
    }
    return true;
}

/* The blocking body of a station change (runs on the playback worker). */
static void apply_station_blocking(int index, bool save_outgoing)
{
    const station_t *st = station_get(index);
    if (!st) {
        return;
    }
    if (save_outgoing && s_playing_idx >= 0 && s_playing_episode[0]) {
        const station_t *prev = station_get(s_playing_idx);
        if (prev && prev->is_podcast) {
            podcast_pos_set(prev->url, s_playing_episode, adf_pipeline_byte_pos());
        }
    }
    char url[STATION_URL_MAX];
    uint32_t offset = 0;
    if (!station_playspec(index, url, sizeof(url), &offset)) {
        /* Couldn't resolve (e.g. podcast feed unreachable): leave the pipeline
         * and the UI/metadata on the current station so they stay in sync. */
        ESP_LOGW(TAG, "could not resolve preset %d; staying put", index);
        return;
    }
    adf_pipeline_set_url(url, offset);       /* PCM pauses briefly; A2DP survives */
    s_playing_idx = index;
    if (st->is_podcast) {
        strncpy(s_playing_episode, url, sizeof(s_playing_episode) - 1);
        s_playing_episode[sizeof(s_playing_episode) - 1] = '\0';
    } else {
        s_playing_episode[0] = '\0';
    }
    audio_output_send_metadata(st->name);    /* BT: relay to the car via AVRCP */
#if defined(CONFIG_PRESET_ENABLE_UI)
    ui_set_station(index, st->name);
    album_art_load(st->favicon);             /* fetch + show the station logo */
#endif
}

/* The blocking body of the 10 s-gap position commit (runs on the worker). */
static void save_disc_pos_blocking(void)
{
    if (s_playing_idx >= 0 && s_playing_episode[0]) {
        const station_t *p = station_get(s_playing_idx);
        if (p && p->is_podcast) {
            podcast_pos_set(p->url, s_playing_episode, s_disc_pos);
            s_long_gap = true;
        }
    }
}

static void playback_worker(void *arg)
{
    (void)arg;
    play_req_t req;
    for (;;) {
        if (xQueueReceive(s_play_q, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (req.op == PLAY_APPLY) {
            apply_station_blocking(req.index, req.save_outgoing);
        } else if (req.op == PLAY_SAVE_POS) {
            save_disc_pos_blocking();
        } else {
            schedule_apply_blocking();
        }
    }
}

/* Post work to the playback worker (non-blocking). Before the worker exists
 * (early boot) the request runs inline — safe, since the only early callers are
 * on the init task. A full queue just drops the request (fast knob spins
 * coalesce to the latest landing). */
static void playback_post(play_op_t op, int index, bool save_outgoing)
{
    if (!s_play_q) {
        if (op == PLAY_APPLY) {
            apply_station_blocking(index, save_outgoing);
        } else if (op == PLAY_SAVE_POS) {
            save_disc_pos_blocking();
        } else {
            schedule_apply_blocking();
        }
        return;
    }
    play_req_t req = { .op = op, .index = index, .save_outgoing = save_outgoing };
    xQueueSend(s_play_q, &req, 0);
}

/* Apply a new station (resolve, re-point the pipeline, relay metadata, refresh
 * the UI). When `save_outgoing` is set and the station we are leaving is a
 * podcast, its current position is saved first, so coming back resumes where you
 * were. (The disconnect-resume re-play passes false, so it does not overwrite
 * the position saved at the drop.) Non-blocking: the work runs on the worker. */
static void apply_station(int index, bool save_outgoing)
{
    playback_post(PLAY_APPLY, index, save_outgoing);
}

/* Fired when Bluetooth has been gone for > 10 s: persist the podcast position
 * captured at disconnect so playback resumes there on reconnect. Runs in the
 * esp_timer task, so it only posts to the worker (no blocking NVS here). */
static void disc_timer_cb(void *arg)
{
    (void)arg;
    playback_post(PLAY_SAVE_POS, 0, false);
}

/* The blocking body of the scheduler: pick the active preset and switch to it.
 * Runs on the playback worker because station_set_current() writes NVS — which
 * must not happen in the SNTP notification (LWIP task) or return-channel task
 * that trigger the scheduler. */
static void schedule_apply_blocking(void)
{
    int weekday, minute;
    if (!time_now_local(&weekday, &minute)) {
        return;
    }
    size_t n = station_count();
    if (n > STATION_MAX) {
        n = STATION_MAX;                  /* never index scheds[] out of bounds */
    }
    station_sched_t scheds[STATION_MAX];
    for (size_t i = 0; i < n; i++) {
        const station_t *s = station_get((int)i);
        scheds[i] = s ? s->sched : (station_sched_t){ 0 };
    }
    int idx = schedule_pick(scheds, n, weekday, minute);
    if (idx >= 0 && idx != station_current()) {
        ESP_LOGI(TAG, "schedule active -> preset %d", idx);
        station_set_current(idx);
        apply_station_blocking(idx, true);   /* already on the worker */
    }
}

/* Auto-play scheduler trigger (non-blocking): the decision + NVS write run on
 * the playback worker. Triggered when the clock first syncs and whenever
 * Bluetooth (re)connects, so the device lands on the right preset on its own. */
static void scheduler_apply(void)
{
    playback_post(PLAY_SCHEDULE, 0, false);
}

/* Encoder detent -> a crisp click (plan §6.2: hand is on the knob). */
static void on_encoder(int index)
{
#if defined(CONFIG_PRESET_ENABLE_HAPTIC)
    haptic_play(HAPTIC_CLICK);
#endif
#if defined(CONFIG_PRESET_ENABLE_DEEPSLEEP)
    power_sleep_kick();   /* hand on the knob: don't sleep out from under them */
#endif
    apply_station(index, true);
}

#if defined(CONFIG_PRESET_ENABLE_UI)
/* UI touch-button callback: advance the station like an encoder detent, with a
 * lighter tap. The encoder advances internally; the buttons hand us a delta. */
static void on_ui_nav(int delta)
{
#if defined(CONFIG_PRESET_ENABLE_HAPTIC)
    haptic_play(HAPTIC_TICK);
#endif
#if defined(CONFIG_PRESET_ENABLE_DEEPSLEEP)
    power_sleep_kick();
#endif
    apply_station(station_advance(delta), true);
}
#endif

/* AVRCP (steering-wheel) button relayed from the car. Per plan §6.2 there is NO
 * haptic — the hand is on the wheel, not the knob. */
static void on_avrcp(uint8_t cmd)
{
    switch (cmd) {
    case PCM_LINK_AVRCP_NEXT: apply_station(station_advance(1), true);  break;
    case PCM_LINK_AVRCP_PREV: apply_station(station_advance(-1), true); break;
    default: break;   /* play/pause: transport control is a later step */
    }
}

/* Bluetooth connection state from the bridge: drive the schedule, the podcast
 * resume-after-10s rule, and the UI overlay. */
static void on_bt_status(uint8_t state)
{
#if defined(CONFIG_PRESET_ENABLE_DEEPSLEEP)
    power_sleep_on_bt(state);   /* drive the no-car deep-sleep countdown */
#endif
    if (state == PCM_LINK_BT_CONNECTED) {
        if (s_disc_timer) {
            esp_timer_stop(s_disc_timer);   /* cancel the pending 10 s window */
        }
        /* If BT was gone > 10 s, first jump the podcast back to where it dropped
         * (the pipeline kept running 'live' meanwhile) — without re-saving, so
         * the position captured at the drop stands. */
        if (s_long_gap) {
            const station_t *cur = station_get(station_current());
            if (cur && cur->is_podcast) {
                apply_station(station_current(), false);
            }
            s_long_gap = false;
        }
        scheduler_apply();                  /* then land on the scheduled preset */
#if defined(CONFIG_PRESET_ENABLE_UI)
        ui_show_status(UI_STATUS_NONE, NULL);
#endif
    } else if (state == PCM_LINK_BT_CONNECTING) {
#if defined(CONFIG_PRESET_ENABLE_UI)
        ui_show_status(UI_STATUS_PAIRING, NULL);
#endif
    } else if (state == PCM_LINK_BT_DISCONNECTED) {
        /* Capture the podcast position now; commit it only if BT stays gone. */
        if (s_playing_idx >= 0) {
            const station_t *p = station_get(s_playing_idx);
            if (p && p->is_podcast) {
                s_disc_pos = adf_pipeline_byte_pos();
                if (!s_disc_timer) {
                    const esp_timer_create_args_t a = { .callback = disc_timer_cb,
                                                        .name = "bt_gap" };
                    if (esp_timer_create(&a, &s_disc_timer) != ESP_OK) {
                        s_disc_timer = NULL;
                    }
                }
                if (s_disc_timer) {
                    esp_timer_stop(s_disc_timer);
                    esp_timer_start_once(s_disc_timer, 10 * 1000000ULL);   /* 10 s */
                }
            }
        }
    }
}

static void phase_e_start(void)
{
    station_init();

    /* Playback worker: owns the blocking podcast-resolve + NVS writes + pipeline
     * changes, fed by the input/timer/return tasks (core 0, off the audio core).
     * 8 KB stack covers the HTTPS/TLS fetch in podcast_resolve. */
    s_play_q = xQueueCreate(8, sizeof(play_req_t));
    xTaskCreatePinnedToCore(playback_worker, "playback", 8192, NULL, 5, NULL, 0);

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

    /* Online with a car expected: start the no-car deep-sleep countdown (does
     * not run in portal/setup mode — only on the normal credentialed path). */
#if defined(CONFIG_PRESET_ENABLE_DEEPSLEEP)
    power_sleep_init();
#endif

    /* Wall-clock time -> when it first syncs, jump to the scheduled preset. */
    time_sync_start(scheduler_apply);

    /* Online: advertise <name>.local and serve the preset editor on the LAN. */
#if defined(CONFIG_PRESET_ENABLE_LAN_EDITOR)
    lan_editor_start();
#endif

    /* Initial play: resolve the current preset (podcast -> latest episode at its
     * saved position) and start the pipeline on it. */
    int cur = station_current();
    const station_t *st = station_get(cur);
    char play_url[STATION_URL_MAX];
    uint32_t play_off = 0;
    if (st && station_playspec(cur, play_url, sizeof(play_url), &play_off)) {
        adf_pipeline_start(play_url, play_off);
        s_playing_idx = cur;
        if (st->is_podcast) {
            strncpy(s_playing_episode, play_url, sizeof(s_playing_episode) - 1);
            s_playing_episode[sizeof(s_playing_episode) - 1] = '\0';
        }
    } else {
        adf_pipeline_start(st ? st->url : "", 0);   /* fallback */
    }
#if defined(CONFIG_PRESET_ENABLE_UI)
    if (st) {
        album_art_load(st->favicon);        /* logo for the initial station */
    }
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

    /* If this boot is an RTC-timer wake from a car-off deep sleep, poll the
     * U4WDH and either resume (full reboot) or sleep again — before any of the
     * heavy/visible init below (so a poll is silent and cheap). */
#if defined(CONFIG_PRESET_AUDIO_SOURCE_ADF) && defined(CONFIG_PRESET_ENABLE_DEEPSLEEP)
    power_sleep_boot_hook();
#endif

#if defined(CONFIG_PRESET_ENABLE_PROVISION)
    /* Listen for the browser installer's PROVISION line before anything else. */
    provisioning_serial_start();
#endif

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
#if defined(CONFIG_PRESET_AUDIO_SOURCE_ADF)
    /* Route the bridge's AVRCP + BT-status frames before the listener starts. */
    backpressure_rx_set_handlers(on_avrcp, on_bt_status);
#endif
    audio_output_start(out_mode);   /* BT: UART+A2DP / ANALOG: I2S->DAC */

#if defined(CONFIG_PRESET_AUDIO_SOURCE_ADF)
    phase_e_start();
#endif

#if defined(CONFIG_PRESET_ENABLE_HAPTIC)
    haptic_play(HAPTIC_READY);
#endif

    ESP_LOGI(TAG, "free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
}
