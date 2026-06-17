/*
 * i2c_bus.h — the S3's shared I2C master bus (plan §1.2).
 *
 * The capacitive touch (CST816S) and the haptic driver (DRV2605) sit on the
 * SAME I2C bus (SDA=GPIO11, SCL=GPIO12). A single bus manager owns it and
 * serializes access behind a mutex, so the two device drivers never collide.
 *
 * Built on the ESP-IDF v5 `i2c_master` API: callers either attach a device to
 * the shared bus handle (esp_lcd touch IO, or i2c_master_dev for the haptic)
 * or use the locked convenience transfers below for simple register access.
 */
#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stddef.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "board_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared-bus pinout (board_pins.h, plan §1.2). External 5.1 kOhm pull-ups
 * (R5/R6) are on the board, so the internal pull-ups stay off. */
#define I2C_BUS_SDA_GPIO   BOARD_I2C_SDA
#define I2C_BUS_SCL_GPIO   BOARD_I2C_SCL
#define I2C_BUS_FREQ_HZ    400000

/* Initialize the shared bus once (idempotent). Safe to call from each driver's
 * init; the first call creates the bus + mutex, later calls are no-ops. */
esp_err_t i2c_bus_init(void);

/* The underlying master-bus handle, for drivers that add their own device
 * (e.g. esp_lcd_new_panel_io_i2c, i2c_master_bus_add_device). NULL until init. */
i2c_master_bus_handle_t i2c_bus_handle(void);

/* Serialize a multi-step transaction against the shared bus. Every raw access
 * (and the haptic's read-modify-write sequences) must run between these. */
void i2c_bus_lock(void);
void i2c_bus_unlock(void);

/*
 * Locked convenience transfers for simple register-based devices (the haptic).
 * Each adds the device ephemerally, performs the transfer under the lock, and
 * removes it. `reg`-prefixed helpers write the register pointer first.
 */
esp_err_t i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len);
esp_err_t i2c_bus_write_reg(uint8_t addr, uint8_t reg, uint8_t val);
esp_err_t i2c_bus_read_reg(uint8_t addr, uint8_t reg, uint8_t *val);

#ifdef __cplusplus
}
#endif

#endif /* I2C_BUS_H */
