/*
 * jitter_buffer.h — PCM jitter buffer for the U4WDH bridge.
 *
 * A single-producer / single-consumer ring of raw interleaved 16-bit stereo
 * PCM. The UART/COBS task pushes decoded payloads; the A2DP source callback
 * pulls fixed reads at the SBC encoder's pace. Lives entirely in SRAM (the
 * U4WDH has no PSRAM), so capacity is the scarce resource the prototype must
 * prove out (plan risk: "A2DP source tient dans la SRAM").
 *
 * Lock-free for the common path: producer owns the head, consumer owns the
 * tail, both published via volatile indices. A short critical section guards
 * the fill-level read used by the backpressure logic.
 */
#ifndef JITTER_BUFFER_H
#define JITTER_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t      *data;
    size_t        capacity;     /* bytes, power-of-two not required */
    volatile size_t head;       /* write position (producer) */
    volatile size_t tail;       /* read position (consumer)  */

    /* Diagnostics for the deliverable's metrics. */
    uint32_t      underruns;    /* consumer found too few bytes */
    uint32_t      overruns;     /* producer dropped a payload   */
    uint32_t      pushed_bytes;
    uint32_t      pulled_bytes;
} jitter_buffer_t;

/* Initialize over a caller-provided backing buffer of `capacity` bytes. */
void jb_init(jitter_buffer_t *jb, uint8_t *backing, size_t capacity);

/* Bytes currently available to read. */
size_t jb_filled(const jitter_buffer_t *jb);

/* Free space available to write. */
size_t jb_free(const jitter_buffer_t *jb);

/* Fill level as a fraction of capacity, scaled to milliseconds of audio at
 * the link sample rate (helper for backpressure thresholds). */
uint32_t jb_filled_ms(const jitter_buffer_t *jb);

/*
 * Push `len` bytes. Returns true if the whole payload was stored, false if it
 * would overrun (in which case nothing is written and overruns++).
 */
bool jb_push(jitter_buffer_t *jb, const uint8_t *src, size_t len);

/*
 * Pull exactly `len` bytes into `dst`. If fewer are available, fills the
 * shortfall with silence (zeros), counts an underrun, and still returns `len`
 * so the A2DP callback always gets a full buffer. Returns bytes of real audio
 * supplied (len - silence).
 */
size_t jb_pull(jitter_buffer_t *jb, uint8_t *dst, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* JITTER_BUFFER_H */
