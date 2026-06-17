/*
 * backpressure_rx.h — return-channel listener (S3 side).
 *
 * The return UART (GPIO48) now carries COBS CONTROL frames, not raw bytes: this
 * reassembles them and dispatches by opcode. FLOW drives the pacing multiplier
 * the UART writer applies; AVRCP button events and BT status are handed to
 * registered callbacks (the app maps them to station nav / UI).
 */
#ifndef BACKPRESSURE_RX_H
#define BACKPRESSURE_RX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Steering-wheel (AVRCP) button relayed from the car: a pcm_link_avrcp_cmd_t. */
typedef void (*return_avrcp_cb_t)(uint8_t cmd);
/* Bluetooth connection state from the bridge: a pcm_link_bt_state_t. */
typedef void (*return_bt_status_cb_t)(uint8_t state);
/* One discovered Bluetooth sink (mac[6] + NUL-terminated name) during a scan. */
typedef void (*return_scan_cb_t)(const uint8_t *mac, const char *name);

/* Register handlers for non-flow control messages (either may be NULL). */
void backpressure_rx_set_handlers(return_avrcp_cb_t avrcp,
                                  return_bt_status_cb_t bt_status);

/* Register a handler for BT_SCAN_RESULT frames (the captive portal's BT scan). */
void backpressure_rx_set_scan_cb(return_scan_cb_t scan);

/* Start the return-channel listener task. */
void backpressure_rx_start(void);

/* Current pacing multiplier: 1.0 nominal, <1 hurry up, >1 ease off. */
double backpressure_rate(void);

#ifdef __cplusplus
}
#endif

#endif /* BACKPRESSURE_RX_H */
