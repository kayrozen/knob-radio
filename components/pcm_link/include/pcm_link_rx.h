/*
 * pcm_link_rx.h — streaming reassembly of the forward UART byte stream.
 *
 * Bytes arrive from UART RX DMA in arbitrary chunks. This accumulator buffers
 * them until it sees the 0x00 delimiter, then hands the complete COBS body to
 * a callback. After any corruption the next 0x00 instantly re-aligns framing
 * (that is the whole point of COBS), so a too-long run is simply dropped and
 * we keep scanning.
 *
 * No allocation; the caller supplies the scratch buffer.
 */
#ifndef PCM_LINK_RX_H
#define PCM_LINK_RX_H

#include <stddef.h>
#include <stdint.h>
#include "pcm_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pcm_link_frame_cb)(const pcm_frame_t *frame, void *user);

typedef struct {
    uint8_t *buf;          /* scratch, >= PCM_LINK_FRAME_MAX_WIRE bytes  */
    size_t   cap;          /* capacity of buf                            */
    size_t   len;          /* bytes currently accumulated                */
    int      overflowed;   /* current run exceeded cap -> drop to next 0 */

    pcm_link_frame_cb cb;
    void             *user;

    /* Diagnostics (Phase B/C metrics). */
    uint32_t frames_ok;
    uint32_t err_cobs;
    uint32_t err_crc;
    uint32_t err_length;
    uint32_t overflows;
} pcm_link_rx_t;

void pcm_link_rx_init(pcm_link_rx_t *rx, uint8_t *scratch, size_t cap,
                      pcm_link_frame_cb cb, void *user);

/* Feed a chunk of received bytes. Completed frames are delivered via cb. */
void pcm_link_rx_feed(pcm_link_rx_t *rx, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PCM_LINK_RX_H */
