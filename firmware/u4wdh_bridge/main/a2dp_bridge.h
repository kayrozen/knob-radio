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

/* Initialize the BT stack and begin discovery/connection. The data callback
 * pulls PCM from `jb`. */
void a2dp_bridge_start(jitter_buffer_t *jb);

#ifdef __cplusplus
}
#endif

#endif /* A2DP_BRIDGE_H */
