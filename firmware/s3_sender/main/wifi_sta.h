/*
 * wifi_sta.h — minimal WiFi station bring-up with auto-reconnect.
 *
 * Standard esp_wifi (not ADF periph_wifi) to keep the API surface stable. On
 * disconnect it retries indefinitely, which covers the Phase F "reprise après
 * perte réseau" requirement (tunnel drop -> reconnect).
 */
#ifndef WIFI_STA_H
#define WIFI_STA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bring up WiFi STA and block until first connection (or timeout_ms, 0 = wait
 * forever). Returns true once an IP is acquired. */
bool wifi_sta_start(const char *ssid, const char *password, int timeout_ms);

/* True while an IP is currently held. */
bool wifi_sta_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_STA_H */
