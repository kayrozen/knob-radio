/*
 * link_tx.h — return-channel control transmitter (U4WDH side).
 *
 * The return UART (U4WDH TX -> S3 RX) now carries COBS control frames, not just
 * single backpressure bytes, so it can relay flow control, BT status, AVRCP
 * button events and scan results uniformly. This builds a CONTROL frame and
 * writes it to the shared UART; the S3 reassembles + dispatches it.
 */
#ifndef LINK_TX_H
#define LINK_TX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Send one control-plane frame on the return link. Thread-safe (the UART driver
 * serializes writes); `op` is a pcm_link_ctrl_op_t. */
void link_tx_send_control(uint8_t op, const uint8_t *args, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* LINK_TX_H */
