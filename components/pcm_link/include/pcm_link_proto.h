/*
 * pcm_link_proto.h — Wire-protocol constants for the S3 <-> U4WDH PCM bridge.
 *
 * This header is shared verbatim by both firmware projects (S3 sender,
 * U4WDH bridge) and by the host-side unit tests. Keep it dependency-free
 * (no ESP-IDF includes) so it compiles on the host toolchain as well.
 *
 * Architecture (see docs/prototype-plan.md):
 *
 *   ESP32-S3  (WiFi + codecs -> PCM) --[forward UART, COBS]--> ESP32-U4WDH (PCM -> A2DP)
 *   ESP32-S3  <--[return UART, backpressure]------------------- ESP32-U4WDH
 */
#ifndef PCM_LINK_PROTO_H
#define PCM_LINK_PROTO_H

#include <stdint.h>

/* ----------------------------------------------------------------------- *
 *  Audio format carried over the link
 * ----------------------------------------------------------------------- */
#define PCM_LINK_SAMPLE_RATE_HZ   44100
#define PCM_LINK_BITS_PER_SAMPLE  16
#define PCM_LINK_CHANNELS         2
/* Bytes per stereo frame (one L+R sample pair). */
#define PCM_LINK_BYTES_PER_SAMPLE ((PCM_LINK_BITS_PER_SAMPLE / 8) * PCM_LINK_CHANNELS)
/* Nominal throughput: 44100 * 4 = 176400 B/s (~172 KiB/s). */

/* ----------------------------------------------------------------------- *
 *  Forward channel (S3 -> U4WDH): logical frame + COBS framing
 * ----------------------------------------------------------------------- *
 *
 *  Logical frame, serialized little-endian, BEFORE COBS:
 *
 *      offset 0      : type     (uint8)   frame_type_t (audio/control/ota/art)
 *      offset 1      : seq      (uint8)   sequence number, wraps at 256
 *      offset 2..3   : length   (uint16)  number of payload bytes
 *      offset 4..N   : payload  (length)  type-dependent (PCM, control msg, ...)
 *      offset N+1    : crc8     (uint8)   CRC-8 over type..payload (inclusive)
 *
 *  On the wire: COBS(serialized frame) followed by a single 0x00 delimiter.
 *  COBS guarantees the body contains no 0x00, so 0x00 is an unambiguous,
 *  always-resyncable frame boundary even at 3 Mbps without HW flow control.
 *
 *  The type byte multiplexes the audio stream with a bidirectional control
 *  plane (and the OTA / cover-art transfers) over the one validated link; the
 *  receiver dispatches on it. Audio dominates; everything else is occasional.
 */

/* Logical frame type (first byte of every frame). */
typedef enum {
    PCM_LINK_FRAME_AUDIO    = 0x01, /* PCM payload (S3 -> U4WDH), dominant      */
    PCM_LINK_FRAME_CONTROL  = 0x02, /* control message (bidirectional)          */
    PCM_LINK_FRAME_OTA_DATA = 0x03, /* U4WDH firmware block (S3 -> U4WDH)        */
    PCM_LINK_FRAME_ART_DATA = 0x04, /* cover-art image block (S3 -> U4WDH)       */
} pcm_link_frame_type_t;

/* PCM payload size of a full frame. 512 bytes = 128 stereo samples ~= 2.9 ms
 * of audio at 44.1 kHz. Starting point; tune in Phase B per latency/overhead.
 * Control / OTA / art payloads also fit within this bound (they are chunked). */
#define PCM_LINK_PAYLOAD_BYTES    512

/* Header (type + seq + length) + payload + trailer (crc8). */
#define PCM_LINK_FRAME_HEADER     4
#define PCM_LINK_FRAME_TRAILER    1
#define PCM_LINK_FRAME_MAX_RAW    (PCM_LINK_FRAME_HEADER + PCM_LINK_PAYLOAD_BYTES + PCM_LINK_FRAME_TRAILER)

/* COBS worst-case overhead is ceil(n/254) extra bytes, plus the 0x00
 * delimiter. Size the wire buffer with margin. */
#define PCM_LINK_COBS_OVERHEAD(n) (((n) + 253u) / 254u)
#define PCM_LINK_FRAME_MAX_WIRE   (PCM_LINK_FRAME_MAX_RAW + PCM_LINK_COBS_OVERHEAD(PCM_LINK_FRAME_MAX_RAW) + 1)

/* Frame delimiter on the wire. Never appears inside a COBS-encoded body. */
#define PCM_LINK_DELIMITER        0x00u

/* ----------------------------------------------------------------------- *
 *  UART pinout (validated from the JC3636K518 schematics)
 * ----------------------------------------------------------------------- */
/* Forward: S3 TX -> U4WDH RX  (PCM/COBS). */
#define PCM_LINK_S3_TX_GPIO       38
#define PCM_LINK_U4WDH_RX_GPIO    18
/* Return: U4WDH TX -> S3 RX   (backpressure/control). */
#define PCM_LINK_U4WDH_TX_GPIO    23
#define PCM_LINK_S3_RX_GPIO       48

/* Forward link baud. 3 Mbps gives ~17x headroom over the 176400 B/s payload,
 * leaving room for COBS overhead, framing gaps and retransmission-free slack.
 * Both directions share one full-duplex UART at this baud; the return
 * direction carries only a few control bytes per jitter-buffer poll. */
#define PCM_LINK_FORWARD_BAUD     3000000

/* ----------------------------------------------------------------------- *
 *  Return channel (U4WDH -> S3): backpressure / flow control
 * ----------------------------------------------------------------------- *
 *  Single-byte commands. The U4WDH reports its jitter-buffer fill state so
 *  the S3 can throttle or hurry the PCM producer and tame clock drift over
 *  long sessions (three independent ~44.1 kHz clocks). */
typedef enum {
    PCM_LINK_BP_SLOWDOWN = 0x10, /* buffer too full  -> producer ease off  */
    PCM_LINK_BP_NORMAL   = 0x20, /* buffer nominal    -> hold rate          */
    PCM_LINK_BP_SPEEDUP  = 0x30, /* buffer too empty -> producer hurry up   */
} pcm_link_bp_cmd_t;

/* ----------------------------------------------------------------------- *
 *  Control plane (PCM_LINK_FRAME_CONTROL payloads)
 * ----------------------------------------------------------------------- *
 *  A control message is carried in a CONTROL frame's payload as:
 *
 *      offset 0   : opcode (uint8)   pcm_link_ctrl_op_t
 *      offset 1.. : args   (n bytes) opcode-dependent
 *
 *  The set mirrors the product plan §1.3. Phase 1 defines the wire opcodes
 *  and a generic codec; the per-opcode behaviour (BT pairing, OTA, AVRCP)
 *  lands in its later phase. Directionality in the comments is the common
 *  case, not a hard restriction — the link is full-duplex.
 */
typedef enum {
    /* Bluetooth control / status (S3 <-> U4WDH). */
    PCM_LINK_CTRL_BT_SCAN_START  = 0x01, /* S3 -> U4: begin sink discovery   */
    PCM_LINK_CTRL_BT_SCAN_RESULT = 0x02, /* U4 -> S3: one sink (mac+name)     */
    PCM_LINK_CTRL_BT_PAIR        = 0x03, /* S3 -> U4: pair/connect <mac[6]>   */
    PCM_LINK_CTRL_BT_STATUS      = 0x04, /* U4 -> S3: <pcm_link_bt_state_t>   */
    PCM_LINK_CTRL_BT_STATUS_REQ  = 0x05, /* S3 -> U4: report BT status now    */
    /* Playback / metadata (AVRCP relay). */
    PCM_LINK_CTRL_AVRCP_EVENT    = 0x10, /* U4 -> S3: steering-wheel <cmd>    */
    PCM_LINK_CTRL_PLAYBACK_STATE = 0x11, /* S3 -> U4: play/pause/stop <state> */
    PCM_LINK_CTRL_METADATA       = 0x12, /* S3 -> U4: now-playing title (str) */
    PCM_LINK_CTRL_DAC_MUTE       = 0x13, /* S3 -> U4: DAC XSMT <1=mute/0=on>  */
    /* Flow / version / OTA orchestration. */
    PCM_LINK_CTRL_FLOW           = 0x20, /* either: <pcm_link_bp_cmd_t> echo  */
    PCM_LINK_CTRL_VERSION        = 0x21, /* either: <proto_version u8>        */
    PCM_LINK_CTRL_OTA_BEGIN      = 0x22, /* S3 -> U4: <total_len u32 LE>      */
    PCM_LINK_CTRL_OTA_END        = 0x23, /* S3 -> U4: commit pushed image     */
    PCM_LINK_CTRL_OTA_ABORT      = 0x24, /* either: cancel in-flight OTA      */
} pcm_link_ctrl_op_t;

/* AVRCP commands relayed from the car's steering-wheel controls. */
typedef enum {
    PCM_LINK_AVRCP_PLAY  = 0x01,
    PCM_LINK_AVRCP_PAUSE = 0x02,
    PCM_LINK_AVRCP_NEXT  = 0x03,
    PCM_LINK_AVRCP_PREV  = 0x04,
} pcm_link_avrcp_cmd_t;

/* Bluetooth connection state reported by the U4WDH. */
typedef enum {
    PCM_LINK_BT_DISCONNECTED = 0x00,
    PCM_LINK_BT_CONNECTING   = 0x01,
    PCM_LINK_BT_CONNECTED    = 0x02,
} pcm_link_bt_state_t;

/* Current UART control-plane protocol version (bumped on wire changes; the
 * two chips exchange it via PCM_LINK_CTRL_VERSION at boot, plan §4.5). */
#define PCM_LINK_PROTO_VERSION    1

/* ----------------------------------------------------------------------- *
 *  Jitter buffer sizing (U4WDH side). PCM SRAM only — no PSRAM on U4WDH.
 * ----------------------------------------------------------------------- *
 *  MAX is the *nominal* capacity the bridge tries to allocate. Because the
 *  classic-BT stack consumes most of the ESP32's internal DRAM, the bridge
 *  falls back to a smaller buffer (down to MIN) if MAX cannot be allocated
 *  while leaving headroom for BT — and the backpressure thresholds are then
 *  derived as fractions of whatever capacity was actually obtained. */
#define PCM_LINK_JITTER_TARGET_MS 220   /* nominal aim point                 */
#define PCM_LINK_JITTER_MAX_MS    420   /* nominal ring capacity ceiling     */
#define PCM_LINK_JITTER_MIN_MS    100   /* absolute floor for the fallback   */

#endif /* PCM_LINK_PROTO_H */
