/*
 * album_art.c — see album_art.h.
 *
 * A single worker task waits for a requested favicon URL, downloads it into a
 * heap buffer, and points the cover image at the bytes; LVGL's JPEG decoder
 * (LV_USE_SJPG) renders them. The newest request wins, so rapid station
 * changes coalesce. The board screen owns scaling/clipping of the image.
 */
#include "album_art.h"
#include "ui.h"

#include <string.h>
#include <stdlib.h>

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "art";

#define ART_MAX_BYTES   (256 * 1024)   /* cap on a favicon download */

static lv_img_dsc_t      s_dsc;        /* describes the current JPEG to LVGL */
static uint8_t          *s_buf;        /* bytes backing s_dsc (kept alive)   */
static char              s_pending[256];
static SemaphoreHandle_t s_lock;       /* guards s_pending                   */
static SemaphoreHandle_t s_sig;        /* "a new URL is pending"             */

/* Download `url` into a freshly malloc'd buffer. Returns size, 0 on failure. */
static size_t http_fetch(const char *url, uint8_t **out)
{
    esp_http_client_config_t cfg = {
        .url               = url,
        .timeout_ms        = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,   /* HTTPS favicons */
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        return 0;
    }

    size_t cap = 0, len = 0;
    uint8_t *buf = NULL;
    if (esp_http_client_open(c, 0) != ESP_OK) {
        esp_http_client_cleanup(c);
        return 0;
    }
    esp_http_client_fetch_headers(c);

    uint8_t tmp[1024];
    int r;
    while ((r = esp_http_client_read(c, (char *)tmp, sizeof(tmp))) > 0) {
        if (len + (size_t)r > ART_MAX_BYTES) {
            len = 0;   /* too big — treat as failure */
            break;
        }
        if (len + (size_t)r > cap) {
            cap = len + (size_t)r + 4096;
            uint8_t *nb = realloc(buf, cap);
            if (!nb) {
                len = 0;
                break;
            }
            buf = nb;
        }
        memcpy(buf + len, tmp, (size_t)r);
        len += (size_t)r;
    }
    esp_http_client_close(c);
    esp_http_client_cleanup(c);

    if (len == 0) {
        free(buf);
        return 0;
    }
    *out = buf;
    return len;
}

static void art_task(void *arg)
{
    (void)arg;
    for (;;) {
        xSemaphoreTake(s_sig, portMAX_DELAY);

        char url[sizeof(s_pending)];
        xSemaphoreTake(s_lock, portMAX_DELAY);
        strcpy(url, s_pending);
        xSemaphoreGive(s_lock);

        if (url[0] == '\0') {
            ui_set_cover(NULL);   /* no logo for this station */
            continue;
        }

        uint8_t *buf = NULL;
        size_t n = http_fetch(url, &buf);
        if (n == 0) {
            ESP_LOGW(TAG, "fetch failed: %s", url);
            continue;   /* keep showing whatever is up */
        }

        /* Detach the old image first (closes LVGL's decoder on the old bytes),
         * then it is safe to free them and publish the new buffer. */
        ui_set_cover(NULL);
        free(s_buf);
        s_buf = buf;

        s_dsc.header.always_zero = 0;
        s_dsc.header.w = 0;        /* decoder reads the real size from the JPEG */
        s_dsc.header.h = 0;
        s_dsc.header.cf = LV_IMG_CF_RAW;
        s_dsc.data_size = n;
        s_dsc.data = s_buf;
        ui_set_cover(&s_dsc);
        ESP_LOGI(TAG, "cover updated (%u bytes)", (unsigned)n);
    }
}

void album_art_start(void)
{
    if (s_lock) {
        return;
    }
    s_lock = xSemaphoreCreateMutex();
    s_sig  = xSemaphoreCreateBinary();

    /* Register LVGL's (split-)JPEG decoder once. */
    if (lvgl_port_lock(0)) {
        lv_split_jpeg_init();
        lvgl_port_unlock();
    }

    /* Core 0 (away from the audio/UART on core 1); generous stack for TLS. */
    xTaskCreatePinnedToCore(art_task, "album_art", 12288, NULL, 4, NULL, 0);
}

void album_art_load(const char *url)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    strncpy(s_pending, url ? url : "", sizeof(s_pending) - 1);
    s_pending[sizeof(s_pending) - 1] = '\0';
    xSemaphoreGive(s_lock);
    xSemaphoreGive(s_sig);
}
