# knob-radio — dual-chip PCM-over-UART bridge prototype

Software validation of the **"PCM-UART bridge"** architecture for an internet-radio
knob (Guition JC3636K518 / Waveshare-knob class hardware): an **ESP32-S3** does
all the heavy lifting (WiFi, HLS, codecs, UI) and ships raw PCM over a UART link
to an **ESP32-U4WDH** that acts as a dumb, reliable **A2DP source** to the car.

```
ESP32-S3 (WiFi + HLS + codecs -> PCM)  --[UART forward, COBS]-->  ESP32-U4WDH (PCM -> SBC -> A2DP)  -->  car
ESP32-S3  <-----------------------------[UART return, backpressure]----------------------------  ESP32-U4WDH
```

The question this prototype answers: **can this board carry a 44.1 kHz/16/stereo
PCM stream (~176 KB/s) S3 → U4WDH over their COBS-framed UART and emit it as A2DP,
stably for 1h+, with no audible dropouts and bounded clock drift?**

Full plan (phases, risks, deliverable): [`docs/prototype-plan.md`](docs/prototype-plan.md).

## Layout

| Path | What |
|---|---|
| `components/pcm_link/` | **Shared protocol** (used by both firmwares + host tests): COBS codec, logical frame (seq/length/payload/crc8), streaming reassembler, wire constants & pinout. Pure C, no ESP-IDF deps. |
| `firmware/s3_sender/` | **ESP32-S3** sender (ESP-IDF, target `esp32s3`). PCM source → frame → COBS → UART TX, plus return-channel backpressure listener. Phase E adds the ESP-ADF radio pipeline, WiFi, rotary encoder, station list and optional LVGL UI (all behind Kconfig). |
| `firmware/u4wdh_bridge/` | **ESP32-U4WDH** bridge (ESP-IDF, target `esp32`). UART RX → COBS decode → CRC/seq → jitter buffer → A2DP source, plus backpressure transmit. |
| `test/host/` | Host unit tests (no hardware): COBS round-trip & overhead, frame build/parse, CRC detection, **resync-after-corruption**, jitter buffer wrap/overrun/underrun. |
| `.github/workflows/ci.yml` | CI: runs host tests + compiles both firmwares with ESP-IDF v5.3. |

The single shared `pcm_link` component is the key design choice: the exact same
COBS/framing code that runs on both chips is also compiled and unit-tested on the
host, so the protocol's correctness claims are verifiable in CI without hardware.

## The wire protocol (COBS)

A logical frame is serialized little-endian, then COBS-encoded, then terminated
by a single `0x00`:

```
[ type(1) | seq(1) | length(2 LE) | payload(length) | crc8(1) ]  -- serialize -->  raw bytes
raw bytes  -- COBS encode -->  body with NO 0x00  -->  append 0x00 delimiter
```

COBS guarantees `0x00` never appears inside the body, so it is an unambiguous,
always-resyncable frame boundary — exactly what a 3 Mbps link without hardware
flow control needs. Measured overhead for a 512-byte payload: **~1.8%** (see the
host test output). See [`components/pcm_link/include/pcm_link_proto.h`](components/pcm_link/include/pcm_link_proto.h)
for all constants and the validated pinout (S3 GPIO38→U4WDH GPIO18 forward,
U4WDH GPIO23→S3 GPIO48 return).

### Control plane (the `type` byte)

The leading `type` byte multiplexes the audio stream with a bidirectional
control plane (and the OTA / cover-art transfers) over the one validated link —
the receiver dispatches on it (`pcm_link_frame_type_t`: `AUDIO`/`CONTROL`/
`OTA_DATA`/`ART_DATA`). A `CONTROL` frame carries `[opcode | args…]`
([`control_msg.h`](components/pcm_link/include/control_msg.h)) covering BT
pairing/status, the AVRCP relay, now-playing metadata, flow, the version
handshake and OTA orchestration (plan §1.3). Audio sequence tracking is
per-stream, so interleaved control frames never look like an audio gap. The
codec and the audio/control dispatch are host-tested; the per-opcode behaviour
lands in its later phase.

## Build & test

### Host tests (no hardware)
```sh
make -C test/host test
```

### Firmware (needs ESP-IDF v5.x)
```sh
# S3 sender — default TONE build (phases B-D, no WiFi)
cd firmware/s3_sender && idf.py set-target esp32s3 && idf.py build

# U4WDH bridge
cd firmware/u4wdh_bridge && idf.py set-target esp32 && idf.py build
```

### S3 Phase E — real internet radio (needs ESP-ADF, `ADF_PATH` exported)
```sh
cd firmware/s3_sender
idf.py menuconfig    # Preset S3 sender -> PCM audio source -> ESP-ADF; set WiFi SSID/pass
#                    # (optional) Enable LVGL UI on the round display
#                    # Audio HAL -> Audio board -> pick any ESP32-S3 board (e.g. S3-Korvo-2)
idf.py set-target esp32s3 build
```
`menuconfig` options live under **Preset S3 sender**: audio source (tone/ADF),
WiFi credentials, the LVGL UI toggle, and the encoder GPIOs. The UI pulls the
LVGL/`esp_lcd_st77916` managed components automatically when enabled.

ESP-ADF registers its whole component tree, including the board-specific
`audio_board` whose default (LyraT v4.3) is an ESP32 pin map that won't compile
for the S3. We never drive a codec/board — output is PCM over UART — but the
pin table still has to build, so under **Audio HAL → Audio board** pick any
ESP32-S3 board (the CI build uses `ESP32-S3-Korvo-2`).

The two chips have **separate USB-C ports** — flash each via its own port:
```sh
idf.py -p <port-for-this-chip> flash monitor
```

To target a specific car/sink by name on the bridge:
```sh
cd firmware/u4wdh_bridge && idf.py build -DA2DP_TARGET_NAME='"My Car"'
```
(empty name = connect to the first audio sink found.)

## Status vs. the plan's phases

- **Phase A (flash + "hello")** — both projects build in CI and print a banner on boot.
- **Phases B–D (link, jitter, drift)** — implemented: COBS framing + CRC/seq loss
  handling, the SRAM jitter buffer with silence-fill on underrun, and the
  return-channel backpressure loop that nudges the S3's pacing. The S3 currently
  feeds a deterministic **test tone** (`pcm_source.c`) so the link can be
  characterised without WiFi.
- **Phase E (real network + UI)** — implemented behind Kconfig. The ADF build
  brings up WiFi + an ESP-ADF pipeline (`http_stream` → auto decoder → resample
  → `raw_stream`) feeding the same COBS/UART path; the rotary encoder switches
  stations (`adf_pipeline.c`, `encoder.c`, `station.c`), and switching only
  pauses PCM — the S3 emits silence so the U4WDH's A2DP link survives. An
  optional LVGL UI (`ui.c`) on the round display runs on core 0, away from the
  audio/UART work on core 1: a dark-themed preset screen (outer ring, per-station
  dots, cover tile, station name/type, a prev/play/next control bar and a
  wifi/bt/battery status row) whose name/type/active-dot track the encoder. The
  panel (`display_st77916.c`, QSPI + PWM backlight) and the CST816S touch
  (`touch_cst816.c`) share the S3's mutex-guarded I2C bus (`i2c_bus.c`) with the
  Phase-6 haptic; the control-bar buttons route through the same station-change
  path as the encoder. Pins are the schematic-confirmed values in `board_pins.h`.
  The Phase-E CI job (`.github/workflows/phase-e.yml`) compiles the full ESP-ADF
  pipeline **and** the LVGL UI + touch (`ui.c`/`touch_cst816.c` + the
  `lvgl`/`esp_lcd_st77916`/`esp_lcd_touch_cst816s` managed components); only the
  on-hardware panel/touch bring-up is out of CI scope.
- **Phase F (real car)** — field test; WiFi auto-reconnect (`wifi_sta.c`) covers
  the tunnel-drop/recovery case. See the plan.

## Product build-out (on top of the prototype)

Work toward the full product (dual-chip, plan in the issue) layered on the
validated prototype, behind Kconfig and compiled in CI:

- **Control plane** — the COBS frame gained a `type` byte (`AUDIO`/`CONTROL`/
  `OTA_DATA`/`ART_DATA`) and a CONTROL message codec (`control_msg.c`), so the
  one link multiplexes audio with control. Host-tested.
- **Display + touch** — `display_st77916.c` (QSPI + PWM backlight dimming) and
  `touch_cst816.c` (CST816S) over the shared, mutex-guarded I2C bus
  (`i2c_bus.c`); pins in `board_pins.h` (from the schematic).
- **Haptic** — `haptic.c` (DRV2605 LRA) on the same I2C bus: a click on encoder
  detents, a lighter tap on touch, plus boot/error effects.
- **Analog output** — `audio_output.c` routes either Bluetooth (UART→U4WDH→A2DP)
  or **analog** (`dac_control.c`: S3 I2S → PCM5100, CH445P source switch). The
  DAC's XSMT mute lives on the U4WDH, so analog mode un-mutes it over the
  control plane (`dac_mute.c` + `PCM_LINK_CTRL_DAC_MUTE`).
- **Playlist + metadata** — the station list is an NVS-backed playlist with a
  remembered index (`station.c`); a station change relays the now-playing title
  to the U4WDH over the control plane (`PCM_LINK_CTRL_METADATA`).
- **UI screens** — preset screen, a settings screen (brightness→NVS, output
  mode, info) and transient boot/Wi-Fi/pairing/error overlays (`ui.c`).
- **Album art** — `album_art.c` downloads the station favicon over HTTPS and
  LVGL decodes the JPEG into the cover tile; works in BT and analog modes.

## What this prototype does NOT do (yet)

Provisioning/captive portal, OTA, telemetry, a full station playlist, mic/SD,
branding, image signing — see the product plan's phase list. No hardware
re-validation is needed (pins confirmed from the schematics).
