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
| `firmware/s3_sender/` | **ESP32-S3** sender (ESP-IDF, target `esp32s3`). PCM source → frame → COBS → UART TX, plus return-channel backpressure listener. |
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
[ seq(1) | length(2 LE) | payload(length) | crc8(1) ]  -- serialize -->  raw bytes
raw bytes  -- COBS encode -->  body with NO 0x00  -->  append 0x00 delimiter
```

COBS guarantees `0x00` never appears inside the body, so it is an unambiguous,
always-resyncable frame boundary — exactly what a 3 Mbps link without hardware
flow control needs. Measured overhead for a 512-byte payload: **~1.6%** (see the
host test output). See [`components/pcm_link/include/pcm_link_proto.h`](components/pcm_link/include/pcm_link_proto.h)
for all constants and the validated pinout (S3 GPIO38→U4WDH GPIO18 forward,
U4WDH GPIO23→S3 GPIO48 return).

## Build & test

### Host tests (no hardware)
```sh
make -C test/host test
```

### Firmware (needs ESP-IDF v5.x)
```sh
# S3 sender
cd firmware/s3_sender && idf.py set-target esp32s3 && idf.py build

# U4WDH bridge
cd firmware/u4wdh_bridge && idf.py set-target esp32 && idf.py build
```

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
- **Phase E (real network + UI)** — *integration point, not built here.* Swap
  `pcm_source` for the ESP-ADF pipeline (WiFi → http/hls_stream → decoder →
  resample → PCM) and add the LVGL UI on the rotary encoder. The `uart_writer`
  framing path is unchanged — see the header comments in
  `firmware/s3_sender/main/pcm_source.h` and `uart_writer.h`.
- **Phase F (real car)** — field test; see the plan.

## What this prototype does NOT do

Provisioning/OTA/telemetry, a full station playlist, haptics/mic/SD, branding,
the analog PCM5100 DAC (output is Bluetooth), or any hardware re-validation
(already confirmed from the schematics). See plan §9.
