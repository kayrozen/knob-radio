/*
 * pcm_link_rx.c — see pcm_link_rx.h.
 */
#include "pcm_link_rx.h"

void pcm_link_rx_init(pcm_link_rx_t *rx, uint8_t *scratch, size_t cap,
                      pcm_link_frame_cb cb, void *user)
{
    rx->buf        = scratch;
    rx->cap        = cap;
    rx->len        = 0;
    rx->overflowed = 0;
    rx->cb         = cb;
    rx->user       = user;
    rx->frames_ok  = 0;
    rx->err_cobs   = 0;
    rx->err_crc    = 0;
    rx->err_length = 0;
    rx->overflows  = 0;
}

static void deliver(pcm_link_rx_t *rx)
{
    if (rx->len == 0) {
        return; /* empty frame (two delimiters in a row) — ignore */
    }

    pcm_frame_t frame;
    pcm_frame_status_t st = pcm_frame_parse(rx->buf, rx->len, &frame);
    switch (st) {
        case PCM_FRAME_OK:
            rx->frames_ok++;
            if (rx->cb) {
                rx->cb(&frame, rx->user);
            }
            break;
        case PCM_FRAME_ERR_CRC:
            rx->err_crc++;
            break;
        case PCM_FRAME_ERR_LENGTH:
        case PCM_FRAME_ERR_SHORT:
            rx->err_length++;
            break;
        case PCM_FRAME_ERR_COBS:
        default:
            rx->err_cobs++;
            break;
    }
}

void pcm_link_rx_feed(pcm_link_rx_t *rx, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];

        if (b == 0x00) {
            /* Frame boundary. */
            if (!rx->overflowed) {
                deliver(rx);
            }
            rx->len = 0;
            rx->overflowed = 0;
            continue;
        }

        if (rx->overflowed) {
            continue; /* discard until the next delimiter re-aligns us */
        }

        if (rx->len >= rx->cap) {
            rx->overflowed = 1;
            rx->overflows++;
            continue;
        }
        rx->buf[rx->len++] = b;
    }
}
