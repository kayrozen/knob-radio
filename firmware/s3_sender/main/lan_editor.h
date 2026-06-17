/*
 * lan_editor.h — mDNS <name>.local LAN preset editor (plan §3.5).
 *
 * Once the device is online in normal operation, advertise it on the LAN as
 * <device-name>.local and serve the installer's preset editor over HTTP, so the
 * five presets can be re-picked from a browser on the same network and saved
 * straight to the device. Distinct from the captive portal (portal.c), which
 * runs only when there are no Wi-Fi credentials yet.
 */
#ifndef LAN_EDITOR_H
#define LAN_EDITOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start mDNS (<device-name>.local) + the preset-editor HTTP server. Call after
 * the station list is loaded and Wi-Fi (STA) is connected. */
void lan_editor_start(void);

#ifdef __cplusplus
}
#endif

#endif /* LAN_EDITOR_H */
