/*
 * pcm_frame.h — logical audio frame: build (S3 side) and parse (U4WDH side).
 *
 * The logical frame is the in-memory representation described in the plan.
 * It is serialized to a flat little-endian buffer, then COBS-encoded, then
 * terminated by 0x00. This module handles serialize + CRC; cobs.c handles
 * the byte stuffing.
 */
#ifndef PCM_LINK_PCM_FRAME_H
#define PCM_LINK_PCM_FRAME_H

#include <stddef.h>
#include <stdint.h>
#include "pcm_link_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* In-memory logical frame. NEVER transmitted as-is (see plan §6.2). */
typedef struct {
    uint8_t  type;                            /* pcm_link_frame_type_t */
    uint8_t  seq;
    uint16_t length;                          /* valid bytes in payload */
    uint8_t  payload[PCM_LINK_PAYLOAD_BYTES];
    uint8_t  crc8;                            /* filled by build, checked by parse */
} pcm_frame_t;

/* CRC-8 (poly 0x07, init 0x00) over `len` bytes. Exposed for tests. */
uint8_t pcm_crc8(const uint8_t *data, size_t len);

/*
 * Serialize a typed logical frame into the wire buffer: lays out
 * type/seq/length/payload, computes the CRC-8, COBS-encodes the whole thing,
 * and appends the 0x00 delimiter.
 *
 * `type` is a pcm_link_frame_type_t.
 * `wire` must hold at least PCM_LINK_FRAME_MAX_WIRE bytes.
 * `payload_len` must be <= PCM_LINK_PAYLOAD_BYTES.
 *
 * Returns the number of bytes written to `wire` (including the delimiter),
 * or 0 on bad arguments.
 */
size_t pcm_frame_build_typed(uint8_t type, uint8_t seq, const uint8_t *payload,
                             uint16_t payload_len, uint8_t *wire);

/* Convenience wrapper: build a PCM_LINK_FRAME_AUDIO frame (the hot path). */
size_t pcm_frame_build(uint8_t seq, const uint8_t *payload, uint16_t payload_len,
                       uint8_t *wire);

/* Result of parsing one received frame. */
typedef enum {
    PCM_FRAME_OK = 0,
    PCM_FRAME_ERR_COBS,    /* COBS decode failed (malformed body) */
    PCM_FRAME_ERR_SHORT,   /* fewer bytes than a header+trailer    */
    PCM_FRAME_ERR_LENGTH,  /* declared length inconsistent          */
    PCM_FRAME_ERR_CRC,     /* CRC mismatch — corrupted               */
} pcm_frame_status_t;

/*
 * Parse one COBS body (the bytes BEFORE the 0x00 delimiter) into `out`.
 * On PCM_FRAME_OK, out->seq/length/payload are populated and the CRC verified.
 */
pcm_frame_status_t pcm_frame_parse(const uint8_t *cobs_body, size_t cobs_len,
                                   pcm_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif /* PCM_LINK_PCM_FRAME_H */
