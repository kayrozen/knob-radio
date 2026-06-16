/*
 * board_pins.h — Guition JC3636K518 ESP32-S3 (R8) GPIO map.
 *
 * SOURCE OF TRUTH: transcribed from the board schematic (ESP32-S3 sheet). These
 * are the confirmed net->GPIO assignments; modules reference them rather than
 * hardcoding pins. The forward/return UART pins live in pcm_link_proto.h (S3
 * TX=38, RX=48) and are confirmed by the same schematic.
 *
 * Shared I2C note (plan §1.2): the capacitive touch (CST816S) and the haptic
 * (DRV2605) sit on ONE I2C bus, SDA=GPIO11 / SCL=GPIO12, with external 5.1 kΩ
 * pull-ups (R5/R6) — so internal pull-ups stay off. The haptic's TRIG is tied
 * to GND and EN to 3V3, i.e. it is controlled purely over I2C.
 */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/* --- Round display: ST77916 over QSPI --- */
#define BOARD_LCD_QSPI_SCL   13
#define BOARD_LCD_QSPI_CS    14
#define BOARD_LCD_QSPI_D0    15
#define BOARD_LCD_QSPI_D1    16
#define BOARD_LCD_QSPI_D2    17
#define BOARD_LCD_QSPI_D3    18
#define BOARD_LCD_RST        21
#define BOARD_LCD_BLK        47   /* backlight enable -> MOSFET Q1 gate (PWM) */
#define BOARD_LCD_TE         (-1) /* LCD_TE net not wired to the S3; unused   */

/* --- Shared I2C bus (touch + haptic) --- */
#define BOARD_I2C_SDA        11
#define BOARD_I2C_SCL        12

/* --- Capacitive touch: CST816S (on the shared I2C bus) --- */
#define BOARD_TP_INT         9
#define BOARD_TP_RST         10

/* --- Rotary encoder EC1 (primary control) --- */
#define BOARD_ENC_A          8
#define BOARD_ENC_B          7

/* --- Analog output: I2S -> PCM5100 DAC, via the CH445P source switch --- */
#define BOARD_I2S_DAC_BCK    39
#define BOARD_I2S_DAC_LRCK   40
#define BOARD_I2S_DAC_DIN    41
#define BOARD_I2S_SWITCH_IN  0    /* CH445P select: low=S3 I2S, high=U4WDH    */
/* Note: the DAC's XSMT (soft-mute) line is driven by the U4WDH (its GPIO32),
 * not the S3 — analog mode must coordinate unmute over the control plane. */

/* --- Misc peripherals (later phases / out of v1 scope) --- */
#define BOARD_BATT_ADC       1    /* GPIO1 / ADC1 — battery divider           */
#define BOARD_PDM_MIC_SCK    45
#define BOARD_PDM_MIC_DATA   46
#define BOARD_SDMMC_CLK      4
#define BOARD_SDMMC_CMD      3
#define BOARD_SDMMC_D0       5
#define BOARD_SDMMC_D1       6
#define BOARD_SDMMC_D2       42
#define BOARD_SDMMC_D3       2

#endif /* BOARD_PINS_H */
