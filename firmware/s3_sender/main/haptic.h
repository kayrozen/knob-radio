/*
 * haptic.h — DRV2605 LRA haptic feedback (plan §6).
 *
 * The DRV2605 sits on the S3's shared I2C bus (i2c_bus.c). On the board its
 * TRIG is tied to GND and EN to 3V3, so it is driven entirely over I2C: we
 * select a ROM effect and set the GO bit. A small vocabulary maps UI events to
 * effects (sharp click on a station change, etc.).
 */
#ifndef HAPTIC_H
#define HAPTIC_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event vocabulary (plan §6.1). The encoder click is the signature feel; AVRCP
 * (steering-wheel) station changes get NO haptic — the hand isn't on the knob. */
typedef enum {
    HAPTIC_CLICK,         /* station change via the encoder           */
    HAPTIC_TICK,          /* light confirmation (touch tap)           */
    HAPTIC_DOUBLE_CLICK,  /* playlist limit (if not wrapping)         */
    HAPTIC_SUCCESS,       /* BT pairing succeeded                     */
    HAPTIC_ERROR,         /* station / playback error                 */
    HAPTIC_READY,         /* boot complete                            */
    HAPTIC_EFFECT_COUNT
} haptic_effect_t;

/* Bring up the DRV2605 (LRA mode, internal trigger). Idempotent w.r.t. the
 * shared I2C bus. Returns ESP_OK once the device acknowledges. */
esp_err_t haptic_init(void);

/* Fire one effect. No-op (returns) if haptic_init() has not succeeded. */
void haptic_play(haptic_effect_t effect);

#ifdef __cplusplus
}
#endif

#endif /* HAPTIC_H */
