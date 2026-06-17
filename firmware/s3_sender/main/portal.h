/*
 * portal.h — captive portal for first-run provisioning (plan §3).
 *
 * Single-phase portal (WiFi lives on the S3, BT on the U4WDH, so the S3 can run
 * its SoftAP freely). Brings up an open access point, a DNS responder that
 * points every lookup at the portal, and an HTTP server with one config page:
 * Wi-Fi credentials, device name, output mode, and the 5 station presets. On
 * save it persists everything and reboots into normal operation.
 */
#ifndef PORTAL_H
#define PORTAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the SoftAP + DNS + HTTP config server. `ap_ssid_out` (>= 33 bytes, may
 * be NULL) receives the AP SSID to show on screen. Returns after the servers
 * are up; the device stays in setup mode until the user saves (then reboots). */
void portal_start(char *ap_ssid_out);

#ifdef __cplusplus
}
#endif

#endif /* PORTAL_H */
