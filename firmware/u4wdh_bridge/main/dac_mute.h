/*
 * dac_mute.h — PCM5100 XSMT (soft-mute) control on the U4WDH (plan §5).
 *
 * On this board the DAC's XSMT line is wired to the U4WDH (GPIO32), not the S3.
 * In analog mode the S3 owns the audio (its own I2S -> DAC) but must ask the
 * U4WDH to un-mute the DAC; that request arrives as a PCM_LINK_CTRL_DAC_MUTE
 * control frame over the forward link. The DAC powers up muted.
 *
 * XSMT polarity: LOW = muted, HIGH = un-muted (PCM5100 soft mute, active low).
 */
#ifndef DAC_MUTE_H
#define DAC_MUTE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configure the XSMT GPIO as an output and start muted. */
void dac_mute_init(void);

/* muted = true -> XSMT low (silent); false -> XSMT high (audio passes). */
void dac_mute_set(bool muted);

#ifdef __cplusplus
}
#endif

#endif /* DAC_MUTE_H */
