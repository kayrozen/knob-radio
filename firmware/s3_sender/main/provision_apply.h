/*
 * provision_apply.h — apply a provisioning config JSON to NVS.
 *
 * The browser installer (over USB serial) and the LAN preset editor (over HTTP,
 * via mDNS <name>.local) both send the same config object:
 *
 *   { provision_version, device_name, timezone, playlist:[ { name, url, kind,
 *     codec, bitrate, schedule:{ mode, days[], start, end } } ] }
 *
 * This parses it, persists the device name, timezone and the 5-preset playlist,
 * and reports whether it was usable. Pure of any transport — the caller owns the
 * I/O and the reboot. cJSON + station + settings only; always compiled.
 */
#ifndef PROVISION_APPLY_H
#define PROVISION_APPLY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Parse one config JSON body and persist name + timezone + playlist. Returns
 * true on a usable config (at least one station with a URL). */
bool provision_apply_json(const char *json);

#ifdef __cplusplus
}
#endif

#endif /* PROVISION_APPLY_H */
