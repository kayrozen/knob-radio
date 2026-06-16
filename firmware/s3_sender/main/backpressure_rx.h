/*
 * backpressure_rx.h — listen to the U4WDH's flow-control commands (S3 side).
 *
 * Reads single-byte commands on the return UART (GPIO48) and exposes a rate
 * multiplier the UART writer applies to its frame pacing, nudging the producer
 * faster/slower to hold the bridge's jitter buffer near target.
 */
#ifndef BACKPRESSURE_RX_H
#define BACKPRESSURE_RX_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the return-channel listener task. */
void backpressure_rx_start(void);

/* Current pacing multiplier: 1.0 nominal, <1 hurry up, >1 ease off. */
double backpressure_rate(void);

#ifdef __cplusplus
}
#endif

#endif /* BACKPRESSURE_RX_H */
