/*
 * power_sleep.h — deep-sleep when the car (Bluetooth) is gone.
 *
 * In a car, the listener stops the engine and walks away: the U4WDH loses its
 * A2DP link and reports BT_DISCONNECTED. After a grace period with no car, the
 * S3 — the big power draw (WiFi + PSRAM + display + ADF) — drops into deep
 * sleep to preserve the battery. It cannot wake on the return UART (GPIO48 is
 * not an RTC pin and deep sleep has no UART wake), so instead it wakes on an
 * RTC timer every few seconds, asks the U4WDH whether the car is back
 * (BT_STATUS_REQ), and either resumes (full reboot) or sleeps again.
 *
 * The podcast position is already committed on disconnect (app_main's 10 s
 * window), so a normal reboot resumes where you left off — deep sleep adds no
 * new state to preserve beyond an RTC marker.
 *
 * All thresholds are Kconfig (PRESET_SLEEP_AFTER_S / PRESET_SLEEP_POLL_S).
 */
#ifndef POWER_SLEEP_H
#define POWER_SLEEP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call first thing in app_main (after NVS). If this boot is an RTC-timer wake
 * from our car-off deep sleep, do a minimal link bring-up, ask the U4WDH for BT
 * status, and either esp_restart() into a full boot (car is back) or deep-sleep
 * again (still gone) — in that case this never returns. On a normal cold boot it
 * returns immediately and does nothing. */
void power_sleep_boot_hook(void);

/* Call once during the full boot (Phase E). Requests the current BT status (a
 * car already connected at boot won't re-emit it). Does NOT start the countdown:
 * a device that has never seen a car (e.g. sitting at home for LAN-editor setup)
 * stays awake and reachable. */
void power_sleep_init(void);

/* Feed BT connection state (from the return-channel BT_STATUS handler): the
 * first CONNECTED marks the device "in service"; thereafter DISCONNECTED arms
 * the deep-sleep countdown (engine off) and CONNECTED cancels it. */
void power_sleep_on_bt(uint8_t bt_state);

/* Reset the inactivity countdown on user activity (encoder / touch / LAN edit),
 * so the device does not sleep out from under someone using it. */
void power_sleep_kick(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_SLEEP_H */
