/*
 * pcm_frame.c — see pcm_frame.h.
 */
#include "pcm_frame.h"
#include "cobs.h"
#include <string.h>

uint8_t pcm_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

size_t pcm_frame_build_typed(uint8_t type, uint8_t seq, const uint8_t *payload,
                             uint16_t payload_len, uint8_t *wire)
{
    if (wire == NULL || payload_len > PCM_LINK_PAYLOAD_BYTES) {
        return 0;
    }
    if (payload_len > 0 && payload == NULL) {
        return 0;
    }

    /* Flat layout: type | seq | length_lo | length_hi | payload | crc8 */
    uint8_t raw[PCM_LINK_FRAME_MAX_RAW];
    size_t n = 0;
    raw[n++] = type;
    raw[n++] = seq;
    raw[n++] = (uint8_t)(payload_len & 0xFF);
    raw[n++] = (uint8_t)(payload_len >> 8);
    if (payload_len) {
        memcpy(&raw[n], payload, payload_len);
        n += payload_len;
    }
    raw[n] = pcm_crc8(raw, n); /* CRC over type..payload */
    n++;

    size_t enc = cobs_encode(raw, n, wire);
    wire[enc] = PCM_LINK_DELIMITER;
    return enc + 1;
}

size_t pcm_frame_build(uint8_t seq, const uint8_t *payload, uint16_t payload_len,
                       uint8_t *wire)
{
    return pcm_frame_build_typed(PCM_LINK_FRAME_AUDIO, seq, payload, payload_len,
                                 wire);
}

pcm_frame_status_t pcm_frame_parse(const uint8_t *cobs_body, size_t cobs_len,
                                   pcm_frame_t *out)
{
    uint8_t raw[PCM_LINK_FRAME_MAX_RAW];

    size_t n = cobs_decode(cobs_body, cobs_len, raw, sizeof(raw));
    if (n == 0) {
        return PCM_FRAME_ERR_COBS;
    }
    if (n < PCM_LINK_FRAME_HEADER + PCM_LINK_FRAME_TRAILER) {
        return PCM_FRAME_ERR_SHORT;
    }

    uint8_t  type   = raw[0];
    uint8_t  seq    = raw[1];
    uint16_t length = (uint16_t)raw[2] | ((uint16_t)raw[3] << 8);

    /* header(4) + payload(length) + crc(1) must equal the decoded size. */
    if ((size_t)PCM_LINK_FRAME_HEADER + length + PCM_LINK_FRAME_TRAILER != n ||
        length > PCM_LINK_PAYLOAD_BYTES) {
        return PCM_FRAME_ERR_LENGTH;
    }

    uint8_t want = pcm_crc8(raw, PCM_LINK_FRAME_HEADER + length);
    uint8_t got  = raw[PCM_LINK_FRAME_HEADER + length];
    if (want != got) {
        return PCM_FRAME_ERR_CRC;
    }

    out->type   = type;
    out->seq    = seq;
    out->length = length;
    out->crc8   = got;
    if (length) {
        memcpy(out->payload, &raw[PCM_LINK_FRAME_HEADER], length);
    }
    return PCM_FRAME_OK;
}
