/*
 * haptic.c — see haptic.h.
 *
 * DRV2605L driven open-loop in LRA mode with the on-chip ROM effect library
 * (TS2200 library 6 for LRA). The effect indices below are reasonable defaults
 * and easy to retune on hardware (an auto-calibration pass for the specific LRA
 * can be added later for a crisper feel).
 */
#include "haptic.h"
#include "i2c_bus.h"
#include "esp_log.h"

#include <stdbool.h>

static const char *TAG = "haptic";

#define DRV2605_ADDR        0x5A

/* Registers. */
#define REG_MODE            0x01
#define REG_LIBRARY         0x03
#define REG_WAVESEQ1        0x04
#define REG_WAVESEQ2        0x05
#define REG_GO              0x0C
#define REG_FEEDBACK        0x1A

#define MODE_INTERNAL_TRIG  0x00   /* out of standby, internal trigger        */
#define FEEDBACK_LRA        0x80   /* N_ERM_LRA bit -> LRA actuator           */
#define LIBRARY_LRA         6      /* ROM library tuned for LRA               */

/* Logical effect -> ROM effect index (TS2200 library). Tune on hardware. */
static const uint8_t s_rom[HAPTIC_EFFECT_COUNT] = {
    [HAPTIC_CLICK]        = 1,   /* Strong Click 100%   */
    [HAPTIC_TICK]         = 24,  /* Sharp Tick 1 100%   */
    [HAPTIC_DOUBLE_CLICK] = 10,  /* Double Click 100%   */
    [HAPTIC_SUCCESS]      = 16,  /* 1000 ms Alert 100%  */
    [HAPTIC_ERROR]        = 14,  /* Strong Buzz 100%    */
    [HAPTIC_READY]        = 7,   /* Soft Bump 100%      */
};

static bool s_ready;

esp_err_t haptic_init(void)
{
    esp_err_t err = i2c_bus_init();
    if (err != ESP_OK) {
        return err;
    }

    /* Out of standby, internal-trigger mode. */
    err = i2c_bus_write_reg(DRV2605_ADDR, REG_MODE, MODE_INTERNAL_TRIG);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DRV2605 not responding: %s", esp_err_to_name(err));
        return err;
    }

    /* Select LRA actuator + its ROM library. */
    uint8_t fb = 0;
    i2c_bus_read_reg(DRV2605_ADDR, REG_FEEDBACK, &fb);
    i2c_bus_write_reg(DRV2605_ADDR, REG_FEEDBACK, (uint8_t)(fb | FEEDBACK_LRA));
    i2c_bus_write_reg(DRV2605_ADDR, REG_LIBRARY, LIBRARY_LRA);

    s_ready = true;
    ESP_LOGI(TAG, "DRV2605 up (LRA, lib %d, shared I2C)", LIBRARY_LRA);
    return ESP_OK;
}

void haptic_play(haptic_effect_t effect)
{
    if (!s_ready || effect >= HAPTIC_EFFECT_COUNT) {
        return;
    }
    /* Waveform slot 0 = the effect, slot 1 = 0 (end of sequence), then GO.
     * The three register writes each take the bus lock; an interleaved touch
     * read between them is harmless (the slots persist until GO plays them). */
    i2c_bus_write_reg(DRV2605_ADDR, REG_WAVESEQ1, s_rom[effect]);
    i2c_bus_write_reg(DRV2605_ADDR, REG_WAVESEQ2, 0);
    i2c_bus_write_reg(DRV2605_ADDR, REG_GO, 1);
}
