/*
 * backpressure.h — return-channel flow control (U4WDH -> S3).
 *
 * Polls the jitter-buffer fill level and emits a single-byte command on the
 * return UART so the S3 can throttle or hurry its PCM producer, taming the
 * drift between the three independent ~44.1 kHz clocks over long sessions.
 */
#ifndef BACKPRESSURE_H
#define BACKPRESSURE_H

#include "jitter_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start the backpressure task. Writes commands on the same UART configured by
 * uart_reader (full-duplex UART1). */
void backpressure_start(jitter_buffer_t *jb);

#ifdef __cplusplus
}
#endif

#endif /* BACKPRESSURE_H */
