/*
 * settings.h — persisted device configuration (plan §9).
 *
 * Small NVS-backed accessors for the values the captive portal writes and the
 * app reads at boot: Wi-Fi credentials, device name, and output mode. The
 * 5-station playlist lives in station.c; brightness in ui.c. All share the
 * "preset" NVS namespace.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_STR_MAX  64   /* buffer size callers must provide */

/* Wi-Fi: fills ssid/pass (each >= SETTINGS_STR_MAX) and returns true if an
 * SSID has been provisioned; false if the device is unconfigured. */
bool settings_get_wifi(char *ssid, char *pass);
void settings_set_wifi(const char *ssid, const char *pass);

/* Device name (mDNS / SoftAP SSID / BT name). Fills out (>= SETTINGS_STR_MAX),
 * defaulting to "Preset Radio". */
void settings_get_device_name(char *out);
void settings_set_device_name(const char *name);

/* Output mode: 0 = Bluetooth, 1 = analog. Returns `def` if unset. */
int  settings_get_output_mode(int def);
void settings_set_output_mode(int mode);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H */
