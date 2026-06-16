/*
 * pcm_source.h — PCM producer abstraction for the S3 sender.
 *
 * This is the seam between the audio pipeline and the UART writer. For the
 * link-validation phases (A-D) it synthesizes a deterministic test signal so
 * the UART/COBS/jitter path can be characterised without WiFi. In Phase E the
 * ESP-ADF pipeline (http/hls_stream -> decoder -> resample -> PCM) replaces
 * this generator; only pcm_source_read() needs to be re-pointed at the ADF
 * output ring buffer — the writer downstream is unchanged.
 *
 * Output format is fixed by the link: 44.1 kHz, 16-bit, interleaved stereo.
 */
#ifndef PCM_SOURCE_H
#define PCM_SOURCE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void pcm_source_init(void);

/*
 * Fill `dst` with exactly `len` bytes of interleaved 16-bit stereo PCM.
 * Always returns `len` for the test generator (an ADF-backed implementation
 * may block until enough decoded PCM is available).
 */
size_t pcm_source_read(uint8_t *dst, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PCM_SOURCE_H */
