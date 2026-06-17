/*
 * a2dp_bridge.h — A2DP source on the U4WDH (PCM jitter buffer -> SBC -> BT).
 *
 * Brings up the classic-BT controller + Bluedroid stack, discovers the target
 * sink by name, connects, and feeds the A2DP data callback from the jitter
 * buffer. The whole audio path on this chip is: UART/COBS -> jitter buffer ->
 * this callback -> SBC encoder -> Bluetooth. No audio decoder runs here (the
 * U4WDH has no PSRAM), which is the architectural premise under test.
 */
#ifndef A2DP_BRIDGE_H
#define A2DP_BRIDGE_H

#include "jitter_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Target sink name to look for during discovery. Override at build time, e.g.
 *   idf.py -DA2DP_TARGET_NAME='"My Car"' build
 * If empty, the bridge connects to the first audio sink it finds. */
#ifndef A2DP_TARGET_NAME
#define A2DP_TARGET_NAME ""
#endif

/*
 * Bring up the classic-BT controller + Bluedroid + A2DP source and register
 * callbacks, but do NOT start discovery yet. Call this first so the BT stack
 * claims its (large) share of internal DRAM before the caller sizes the
 * jitter buffer from what remains.
 */
void a2dp_bridge_init(void);

/* Point the A2DP data callback at the jitter buffer. Must be called (with a
 * valid, initialized buffer) before discovery so streaming has somewhere to
 * pull from. */
void a2dp_bridge_set_buffer(jitter_buffer_t *jb);

/* Begin discovery/connection to the target sink. */
void a2dp_bridge_start_discovery(void);

/* Store the now-playing title relayed from the S3 (PCM_LINK_CTRL_METADATA).
 * The AVRCP target relays the car's transport buttons today; exposing this as
 * the TITLE element attribute to the car is the remaining metadata-to-car step. */
void a2dp_bridge_set_metadata(const char *title);

/* Orchestrated pairing (plan §3.3), driven by control frames from the S3:
 *  - start_scan: enter discovery and report each sink via BT_SCAN_RESULT
 *    (instead of auto-connecting).
 *  - pair: connect to the chosen sink (6-byte MAC). */
void a2dp_bridge_start_scan(void);
void a2dp_bridge_pair(const uint8_t mac[6]);

#ifdef __cplusplus
}
#endif

#endif /* A2DP_BRIDGE_H */
