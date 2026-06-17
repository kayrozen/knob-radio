/*
 * audio_output.h — output routing: Bluetooth (via U4WDH) vs analog DAC (plan §5).
 *
 *   BT     : pcm_source -> COBS/UART -> U4WDH -> A2DP -> car  (S3 + U4WDH)
 *   ANALOG : pcm_source -> I2S -> PCM5100 -> line-out / AUX   (S3 alone)
 *
 * One PCM producer (pcm_source) feeds whichever path is active. The choice is
 * made at start from Kconfig today; a runtime switch (portal/NVS) is Phase 10.
 */
#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AUDIO_OUTPUT_BT,      /* COBS/UART -> U4WDH A2DP (default) */
    AUDIO_OUTPUT_ANALOG,  /* S3 I2S -> PCM5100 DAC             */
} audio_output_mode_t;

/* Start the selected output path. BT starts the UART writer + backpressure
 * listener; ANALOG brings up the DAC, unmutes it (XSMT lives on the U4WDH, so
 * it is unmuted over the control plane) and feeds it from pcm_source. */
void audio_output_start(audio_output_mode_t mode);

audio_output_mode_t audio_output_mode(void);

/* Relay now-playing text to the sink. In BT mode it goes to the U4WDH over the
 * control plane (which forwards it to the car via AVRCP); in analog mode there
 * is no sink to tell, so it is a no-op (the board screen shows it directly). */
void audio_output_send_metadata(const char *title);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_OUTPUT_H */
