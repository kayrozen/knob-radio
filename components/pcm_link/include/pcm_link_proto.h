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
 *      offset 0      : seq      (uint8)   sequence number, wraps at 256
 *      offset 1..2   : length   (uint16)  number of PCM payload bytes
 *      offset 3..N   : payload  (length)  interleaved 16-bit stereo PCM
 *      offset N+1    : crc8     (uint8)   CRC-8 over seq..payload (inclusive)
 *
 *  On the wire: COBS(serialized frame) followed by a single 0x00 delimiter.
 *  COBS guarantees the body contains no 0x00, so 0x00 is an unambiguous,
 *  always-resyncable frame boundary even at 3 Mbps without HW flow control.
 */

/* PCM payload size of a full frame. 512 bytes = 128 stereo samples ~= 2.9 ms
 * of audio at 44.1 kHz. Starting point; tune in Phase B per latency/overhead. */
#define PCM_LINK_PAYLOAD_BYTES    512

/* Header (seq + length) + payload + trailer (crc8). */
#define PCM_LINK_FRAME_HEADER     3
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
 *  Jitter buffer targets (U4WDH side). PCM SRAM only — no PSRAM on U4WDH.
 * ----------------------------------------------------------------------- */
#define PCM_LINK_JITTER_TARGET_MS 220   /* aim point between under/overrun  */
#define PCM_LINK_JITTER_LOW_MS    120   /* below -> request SPEEDUP          */
#define PCM_LINK_JITTER_HIGH_MS   320   /* above -> request SLOWDOWN          */
#define PCM_LINK_JITTER_MAX_MS    420   /* ring capacity ceiling             */

#endif /* PCM_LINK_PROTO_H */
