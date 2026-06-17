/*
 * adf_pipeline.h — ESP-ADF internet-radio pipeline producing PCM (Phase E).
 *
 *   http_stream(reader) -> esp_decoder(auto MP3/AAC/...) -> rsp_filter(->44.1k
 *   /16/stereo) -> raw_stream(reader)
 *
 * The app drains PCM with adf_pipeline_read() and hands it to the existing
 * COBS/UART writer — only the source changes versus the tone build; the framing
 * and link code are identical. Switching stations re-points the http_stream URI
 * and restarts decode; during the gap adf_pipeline_read() returns 0 so the
 * caller can emit silence and keep the U4WDH's A2DP link alive.
 */
#ifndef ADF_PIPELINE_H
#define ADF_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Build + start the pipeline on `url`, beginning at `byte_offset` source bytes
 * (an HTTP Range request — used to resume a podcast). WiFi must be up. */
void adf_pipeline_start(const char *url, uint32_t byte_offset);

/* Read up to `len` bytes of 44.1k/16/stereo PCM. Returns bytes read; 0 if the
 * pipeline is (re)buffering or mid-switch (caller should emit silence). */
int adf_pipeline_read(void *buf, size_t len);

/* Switch to a new stream URL, optionally resuming at `byte_offset`, without
 * tearing down the pipeline objects. */
void adf_pipeline_set_url(const char *url, uint32_t byte_offset);

/* Source byte position currently read from the stream (for resume). */
uint32_t adf_pipeline_byte_pos(void);

#ifdef __cplusplus
}
#endif

#endif /* ADF_PIPELINE_H */
