/*
 * cobs.c — see cobs.h.
 *
 * Standard COBS. The encoder reserves a "code" slot, copies non-zero bytes,
 * and whenever it meets a 0x00 (or a run of 254 non-zero bytes) it writes the
 * run length into the reserved slot and opens a new one. The decoder walks the
 * code bytes in reverse.
 */
#include "cobs.h"

size_t cobs_encode(const uint8_t *src, size_t len, uint8_t *dst)
{
    if (len == 0) {
        return 0;
    }

    size_t read_index  = 0;
    size_t code_index  = 0;  /* slot reserved for the current run length */
    size_t write_index = 1;  /* first data byte goes after the code slot */
    uint8_t code = 1;        /* run length so far (+1 for the code byte itself) */

    while (read_index < len) {
        if (src[read_index] == 0) {
            /* End of run: commit the code, open a fresh slot. */
            dst[code_index] = code;
            code = 1;
            code_index = write_index++;
            read_index++;
        } else {
            dst[write_index++] = src[read_index++];
            code++;
            if (code == 0xFF) {
                /* Maximal run: commit and open a new slot. */
                dst[code_index] = code;
                code = 1;
                code_index = write_index++;
            }
        }
    }

    dst[code_index] = code;
    return write_index;
}

size_t cobs_decode(const uint8_t *src, size_t len, uint8_t *dst, size_t dst_cap)
{
    size_t read_index  = 0;
    size_t write_index = 0;

    while (read_index < len) {
        uint8_t code = src[read_index];

        if (code == 0) {
            /* A 0x00 inside the body is illegal — it is the delimiter only. */
            return 0;
        }
        if (read_index + code > len) {
            /* Run claims more bytes than remain: malformed frame. */
            return 0;
        }

        read_index++;
        for (uint8_t i = 1; i < code; i++) {
            if (write_index >= dst_cap) {
                return 0; /* would overflow caller buffer */
            }
            dst[write_index++] = src[read_index++];
        }

        /* A non-maximal run that is not the final run implies a 0x00 byte. */
        if (code != 0xFF && read_index < len) {
            if (write_index >= dst_cap) {
                return 0;
            }
            dst[write_index++] = 0;
        }
    }

    return write_index;
}
