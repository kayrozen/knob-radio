/*
 * provisioning_serial.h — first-run serial provisioning over USB (plan §3.4).
 *
 * After the browser installer flashes the board it opens the S3's USB serial
 * (115200) and writes one line:
 *
 *     PROVISION:{"provision_version":1,"device_name":"...","playlist":[ ... ]}\n
 *
 * This module listens on the USB-Serial-JTAG console, parses that line, stores
 * the device name + the (up to 5) station presets, replies "OK\n" (or
 * "ERR:<reason>\n"), and reboots so the new config takes effect. Wi-Fi is left
 * to the captive portal — the installer only sends name + presets.
 */
#ifndef PROVISIONING_SERIAL_H
#define PROVISIONING_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the provisioning listener task (USB-Serial-JTAG). */
void provisioning_serial_start(void);

#ifdef __cplusplus
}
#endif

#endif /* PROVISIONING_SERIAL_H */
