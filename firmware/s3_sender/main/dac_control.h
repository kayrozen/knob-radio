/*
 * dac_control.h — analog output path: S3 I2S -> PCM5100 DAC (plan §5).
 *
 * In analog mode the S3 decodes straight to its own I2S (BCK=39, WS=40,
 * DOUT=41) feeding the PCM5100, and drives the CH445P source switch (GPIO0) to
 * route the S3's I2S to the DAC. The U4WDH is unused for audio; it only owns
 * the DAC's XSMT mute line, which is coordinated over the control plane.
 */
#ifndef DAC_CONTROL_H
#define DAC_CONTROL_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bring up the I2S TX channel (44.1 kHz/16-bit/stereo) and point the CH445P
 * switch at the S3. */
esp_err_t dac_control_init(void);

/* Write interleaved 16-bit stereo PCM to the DAC, blocking until queued.
 * Returns bytes written. */
size_t dac_control_write(const uint8_t *pcm, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* DAC_CONTROL_H */
