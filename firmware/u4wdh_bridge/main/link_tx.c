/*
 * link_tx.c — see link_tx.h.
 */
#include "link_tx.h"
#include "pcm_link_proto.h"
#include "control_msg.h"

#include "driver/uart.h"

#define RETURN_UART_NUM   UART_NUM_1   /* shared full-duplex UART */

void link_tx_send_control(uint8_t op, const uint8_t *args, uint16_t len)
{
    static uint8_t seq;
    uint8_t wire[PCM_LINK_FRAME_MAX_WIRE];
    size_t w = pcm_link_ctrl_build_frame(seq++, op, args, len, wire);
    if (w) {
        uart_write_bytes(RETURN_UART_NUM, wire, w);
    }
}
