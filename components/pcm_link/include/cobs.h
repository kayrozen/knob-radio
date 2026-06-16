/*
 * cobs.h — Consistent Overhead Byte Stuffing (RFC-style, byte-stuffed framing).
 *
 * COBS removes every 0x00 from a byte buffer so that 0x00 can be used as an
 * unambiguous frame delimiter. The encoded body NEVER contains 0x00; the
 * caller appends a single 0x00 after the encoded body to mark end-of-frame.
 *
 * Overhead is bounded: at most ceil(len/254) bytes plus the 1-byte delimiter.
 *
 * These functions are pure (no allocation, no globals) and host-portable.
 */
#ifndef PCM_LINK_COBS_H
#define PCM_LINK_COBS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum encoded size (excluding the trailing 0x00 delimiter) for `len`
 * input bytes. Use to size the destination buffer. */
#define COBS_ENCODE_MAX(len) ((len) + (((len) + 253u) / 254u) + 1u)

/*
 * Encode `len` bytes from `src` into `dst`.
 *
 * `dst` must hold at least COBS_ENCODE_MAX(len) bytes. The trailing 0x00
 * delimiter is NOT written — the caller appends it.
 *
 * Returns the number of bytes written to `dst` (the encoded length, without
 * the delimiter). Returns 0 only when `len` is 0.
 */
size_t cobs_encode(const uint8_t *src, size_t len, uint8_t *dst);

/*
 * Decode `len` COBS bytes from `src` (the bytes BEFORE the 0x00 delimiter,
 * delimiter excluded) into `dst`.
 *
 * `dst_cap` is the capacity of `dst`. The decoded output is never larger than
 * the input, so dst_cap >= len is always safe.
 *
 * Returns the number of decoded bytes on success, or 0 if the input is
 * malformed (a code byte points past the end of the buffer) or would overflow
 * `dst`.
 */
size_t cobs_decode(const uint8_t *src, size_t len, uint8_t *dst, size_t dst_cap);

#ifdef __cplusplus
}
#endif

#endif /* PCM_LINK_COBS_H */
