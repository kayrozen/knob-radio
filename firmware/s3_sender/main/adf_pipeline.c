/*
 * adf_pipeline.c — see adf_pipeline.h.
 *
 * Follows the standard ESP-ADF reader-pipeline pattern (cf. the player /
 * pipeline_raw_http examples): elements are linked http -> decoder -> filter ->
 * raw, and the app pulls PCM out of the raw_stream. A listener task watches for
 * the decoder's music-info report and reconfigures the resampler to the stream's
 * real sample rate / channel count, so any MP3/AAC rate lands at 44.1k/stereo.
 */
#include "adf_pipeline.h"
#include "pcm_link_proto.h"

#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "http_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "esp_decoder.h"

static const char *TAG = "adf";

static audio_pipeline_handle_t s_pipeline;
static audio_element_handle_t  s_http;
static audio_element_handle_t  s_decoder;
static audio_element_handle_t  s_filter;   /* resampler */
static audio_element_handle_t  s_raw;
static audio_event_iface_handle_t s_evt;

static uint32_t s_start_offset;   /* Range offset this playback started at    */
static uint32_t s_play_start_ms;  /* when the current URL started playing (ms) */
/* 32-bit ms (not 64-bit us): reads/writes stay atomic on the 32-bit S3, so the
 * value can't tear between the playback worker and a caller on another core;
 * unsigned subtraction still handles wrap (~49 days). */

/* Re-play roughly this much on resume: the decoder's consumed-bytes position
 * still leads what has been *heard* by the decoded-but-unplayed audio sitting
 * in the resampler/raw ringbuffers and the far jitter buffer. Rewinding a few
 * seconds also reads better than resuming mid-word. */
#define RESUME_REWIND_S  5

/* Listener: when the decoder reports the real music info, retune the resampler
 * so its source rate/channels match (output stays 44.1k/16/stereo). */
static void adf_event_task(void *arg)
{
    (void)arg;
    for (;;) {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(s_evt, &msg, portMAX_DELAY) != ESP_OK) {
            continue;
        }
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
            msg.source == (void *)s_decoder &&
            msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t info = { 0 };
            audio_element_getinfo(s_decoder, &info);
            ESP_LOGI(TAG, "stream music info: %d Hz, %d ch, %d bits",
                     info.sample_rates, info.channels, info.bits);
            rsp_filter_set_src_info(s_filter, info.sample_rates, info.channels);
        }
    }
}

/* Set the http element's source byte position (resume via HTTP Range). */
static void set_byte_pos(uint32_t byte_offset)
{
    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();
    audio_element_getinfo(s_http, &info);
    info.byte_pos = (int64_t)byte_offset;
    audio_element_setinfo(s_http, &info);
}

uint32_t adf_pipeline_byte_pos(void)
{
    if (!s_decoder) {
        return 0;
    }
    /* Use the DECODER's consumed-bytes position, not the HTTP element's: the
     * http position is how far the fetch has read ahead, which can lead the
     * played position by the whole pipeline ringbuffer depth (tens of seconds
     * at podcast bitrates). The decoder position counts source bytes actually
     * decoded — much closer to what was heard. It counts from 0 for this
     * playback, so add the Range offset the playback started at. */
    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();
    audio_element_getinfo(s_decoder, &info);
    if (info.byte_pos <= 0) {
        return s_start_offset;   /* nothing decoded yet: position unchanged */
    }
    uint64_t pos = (uint64_t)s_start_offset + (uint64_t)info.byte_pos;

    /* Rewind a few seconds using the measured average source byte rate, to
     * cover the decoded-but-unplayed tail and give a comfortable pickup. */
    uint32_t elapsed_ms = (uint32_t)(esp_timer_get_time() / 1000) - s_play_start_ms;
    if (elapsed_ms > 1000) {
        uint64_t avg_bps = (uint64_t)info.byte_pos * 1000ULL / (uint64_t)elapsed_ms;
        uint64_t margin  = avg_bps * RESUME_REWIND_S;
        pos = (pos > s_start_offset + margin) ? pos - margin : s_start_offset;
    }
    return pos > UINT32_MAX ? UINT32_MAX : (uint32_t)pos;
}

void adf_pipeline_start(const char *url, uint32_t byte_offset)
{
    audio_pipeline_cfg_t pcfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    s_pipeline = audio_pipeline_init(&pcfg);

    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;   /* follow .m3u/.pls/HLS playlists */
    s_http = http_stream_init(&http_cfg);

    audio_decoder_t auto_decode[] = {
        DEFAULT_ESP_MP3_DECODER_CONFIG(),
        DEFAULT_ESP_AAC_DECODER_CONFIG(),
        DEFAULT_ESP_M4A_DECODER_CONFIG(),
        DEFAULT_ESP_TS_DECODER_CONFIG(),
    };
    esp_decoder_cfg_t dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
    s_decoder = esp_decoder_init(&dec_cfg, auto_decode,
                                 sizeof(auto_decode) / sizeof(auto_decode[0]));

    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate  = PCM_LINK_SAMPLE_RATE_HZ;   /* updated on music-info */
    rsp_cfg.src_ch    = PCM_LINK_CHANNELS;
    rsp_cfg.dest_rate = PCM_LINK_SAMPLE_RATE_HZ;
    rsp_cfg.dest_ch   = PCM_LINK_CHANNELS;
    s_filter = rsp_filter_init(&rsp_cfg);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    s_raw = raw_stream_init(&raw_cfg);

    audio_pipeline_register(s_pipeline, s_http,    "http");
    audio_pipeline_register(s_pipeline, s_decoder, "dec");
    audio_pipeline_register(s_pipeline, s_filter,  "rsp");
    audio_pipeline_register(s_pipeline, s_raw,     "raw");

    const char *order[] = { "http", "dec", "rsp", "raw" };
    audio_pipeline_link(s_pipeline, order, 4);

    /* Event interface so we can react to the decoder's music-info report. */
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    s_evt = audio_event_iface_init(&evt_cfg);
    audio_pipeline_set_listener(s_pipeline, s_evt);

    audio_element_set_uri(s_http, url);
    if (byte_offset) {
        set_byte_pos(byte_offset);
    }
    s_start_offset  = byte_offset;
    s_play_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
    audio_pipeline_run(s_pipeline);
    ESP_LOGI(TAG, "pipeline running: %s (offset %lu)", url, (unsigned long)byte_offset);

    xTaskCreatePinnedToCore(adf_event_task, "adf_evt", 3072, NULL, 5, NULL, 1);
}

int adf_pipeline_read(void *buf, size_t len)
{
    int n = raw_stream_read(s_raw, (char *)buf, (int)len);
    return (n > 0) ? n : 0;
}

void adf_pipeline_set_url(const char *url, uint32_t byte_offset)
{
    ESP_LOGI(TAG, "switching to: %s (offset %lu)", url, (unsigned long)byte_offset);
    /* Stop and reset every element, re-point the URI, run again. The pipeline
     * objects persist, so this is fast and leaves the rest of the system
     * (and the downstream A2DP link) intact. */
    audio_pipeline_stop(s_pipeline);
    audio_pipeline_wait_for_stop(s_pipeline);
    audio_pipeline_reset_ringbuffer(s_pipeline);
    audio_pipeline_reset_elements(s_pipeline);
    audio_pipeline_change_state(s_pipeline, AEL_STATE_INIT);

    audio_element_set_uri(s_http, url);
    set_byte_pos(byte_offset);
    s_start_offset  = byte_offset;
    s_play_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
    audio_pipeline_run(s_pipeline);
}
