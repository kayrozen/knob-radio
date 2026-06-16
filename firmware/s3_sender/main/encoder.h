/*
 * encoder.h — EC1 rotary encoder -> station change.
 *
 * Quadrature-decoded with the PCNT peripheral. Each detent advances the station
 * selection and fires a callback (used to re-point the ADF pipeline and refresh
 * the UI).
 */
#ifndef ENCODER_H
#define ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Called from the encoder task on each net detent change, with the new station
 * index. Keep it short; do heavy work elsewhere. */
typedef void (*encoder_cb_t)(int new_station_index);

void encoder_start(int gpio_a, int gpio_b, encoder_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_H */
