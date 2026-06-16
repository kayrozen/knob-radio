/*
 * uart_reader.h — forward-link receive task for the U4WDH bridge.
 *
 * Owns UART1 RX (GPIO18) at 3 Mbps, runs a blocking read loop, feeds bytes to
 * the COBS reassembler, and on each valid frame pushes PCM into the jitter
 * buffer while tracking sequence gaps (lost frames -> insert silence).
 */
#ifndef UART_READER_H
#define UART_READER_H

#include "jitter_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    jitter_buffer_t *jb;

    /* Sequence tracking. */
    uint8_t  last_seq;
    bool     have_seq;
    uint32_t lost_frames;   /* gaps in seq */
    uint32_t silence_bytes; /* zero-fill inserted for lost frames */
} uart_reader_ctx_t;

/* Configure UART1 RX and start the receive task pinned to APP core (core 1). */
void uart_reader_start(uart_reader_ctx_t *ctx, jitter_buffer_t *jb);

#ifdef __cplusplus
}
#endif

#endif /* UART_READER_H */
