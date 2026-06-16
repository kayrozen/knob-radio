/*
 * test_pcm_link.c — host-side unit tests for the shared PCM link protocol.
 *
 * Compiles the component sources directly with the host compiler (no ESP-IDF,
 * no hardware) and exercises the COBS codec, frame build/parse, CRC detection,
 * and the streaming reassembler including resync-after-corruption. This is the
 * verifiable core of the prototype's Phase B claims.
 *
 * Build & run:  make -C test/host
 */
#include "cobs.h"
#include "pcm_frame.h"
#include "pcm_link_rx.h"
#include "pcm_link_proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int g_failures = 0;
static int g_checks   = 0;

#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            g_failures++;                                                  \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);                  \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)

/* --------------------------------------------------------------------- */
/* COBS                                                                   */
/* --------------------------------------------------------------------- */

static void cobs_roundtrip(const uint8_t *src, size_t len, const char *name)
{
    uint8_t enc[2048];
    uint8_t dec[2048];

    size_t e = cobs_encode(src, len, enc);

    /* Invariant: encoded body contains no 0x00 (so 0x00 can delimit). */
    int has_zero = 0;
    for (size_t i = 0; i < e; i++) {
        if (enc[i] == 0) { has_zero = 1; }
    }
    CHECK(!has_zero, "[%s] encoded body contains 0x00", name);

    /* Overhead bound: at most ceil(len/254) extra bytes. */
    CHECK(e <= len + (len + 253) / 254 + 1, "[%s] overhead exceeds bound", name);

    size_t d = cobs_decode(enc, e, dec, sizeof(dec));
    CHECK(d == len, "[%s] decoded length %zu != %zu", name, d, len);
    CHECK(memcmp(dec, src, len) == 0, "[%s] roundtrip mismatch", name);
}

static void test_cobs(void)
{
    printf("test_cobs\n");

    uint8_t a[] = {0x11, 0x22, 0x33};
    cobs_roundtrip(a, sizeof(a), "no-zeros");

    uint8_t b[] = {0x00};
    cobs_roundtrip(b, sizeof(b), "single-zero");

    uint8_t c[] = {0x00, 0x00, 0x00};
    cobs_roundtrip(c, sizeof(c), "all-zeros");

    uint8_t d[] = {0x01, 0x00, 0x02, 0x00, 0x00, 0x03};
    cobs_roundtrip(d, sizeof(d), "mixed");

    /* A run of >254 non-zero bytes forces an intermediate code byte. */
    uint8_t big[600];
    for (size_t i = 0; i < sizeof(big); i++) {
        big[i] = (uint8_t)(1 + (i % 255)); /* never 0 */
    }
    cobs_roundtrip(big, sizeof(big), "long-run-no-zero");

    /* Pseudo-random PCM-like data with embedded zeros. */
    uint8_t pcm[PCM_LINK_PAYLOAD_BYTES];
    uint32_t s = 12345;
    for (size_t i = 0; i < sizeof(pcm); i++) {
        s = s * 1103515245u + 12345u;
        pcm[i] = (uint8_t)(s >> 16);
    }
    cobs_roundtrip(pcm, sizeof(pcm), "pcm-like");

    /* Malformed input: code byte points past end -> decode returns 0. */
    uint8_t bad[] = {0x05, 0x01, 0x02};
    uint8_t out[16];
    CHECK(cobs_decode(bad, sizeof(bad), out, sizeof(out)) == 0,
          "malformed COBS not rejected");

    /* A leading 0x00 code byte is illegal (0x00 is the delimiter, never a
     * code). The streaming reassembler never delivers a body with 0x00, but
     * decode rejects it defensively. */
    uint8_t bad2[] = {0x00, 0x01};
    CHECK(cobs_decode(bad2, sizeof(bad2), out, sizeof(out)) == 0,
          "leading 0x00 code not rejected");
}

/* --------------------------------------------------------------------- */
/* Frame build / parse                                                    */
/* --------------------------------------------------------------------- */

static void test_frame(void)
{
    printf("test_frame\n");

    uint8_t payload[PCM_LINK_PAYLOAD_BYTES];
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(i * 7 + 3);
    }

    uint8_t wire[PCM_LINK_FRAME_MAX_WIRE];
    size_t w = pcm_frame_build(42, payload, sizeof(payload), wire);
    CHECK(w > 0 && w <= PCM_LINK_FRAME_MAX_WIRE, "build size out of range: %zu", w);
    CHECK(wire[w - 1] == PCM_LINK_DELIMITER, "frame not delimiter-terminated");

    /* No 0x00 anywhere except the final delimiter. */
    for (size_t i = 0; i + 1 < w; i++) {
        CHECK(wire[i] != 0x00, "0x00 inside frame body at %zu", i);
    }

    pcm_frame_t f;
    pcm_frame_status_t st = pcm_frame_parse(wire, w - 1, &f);
    CHECK(st == PCM_FRAME_OK, "parse status %d", st);
    CHECK(f.seq == 42, "seq mismatch %u", f.seq);
    CHECK(f.length == sizeof(payload), "length mismatch %u", f.length);
    CHECK(memcmp(f.payload, payload, sizeof(payload)) == 0, "payload mismatch");

    /* Zero-length frame (keep-alive / silence marker). */
    w = pcm_frame_build(7, NULL, 0, wire);
    st = pcm_frame_parse(wire, w - 1, &f);
    CHECK(st == PCM_FRAME_OK && f.length == 0 && f.seq == 7, "empty frame failed");

    /* Single-bit corruption must be caught by CRC. */
    w = pcm_frame_build(99, payload, 64, wire);
    wire[3] ^= 0x01; /* flip a payload bit (post-COBS, but still detectable) */
    st = pcm_frame_parse(wire, w - 1, &f);
    CHECK(st == PCM_FRAME_ERR_CRC || st == PCM_FRAME_ERR_COBS ||
          st == PCM_FRAME_ERR_LENGTH,
          "corruption not detected (status %d)", st);
}

/* --------------------------------------------------------------------- */
/* Streaming reassembly + resync                                          */
/* --------------------------------------------------------------------- */

typedef struct {
    int      count;
    uint8_t  last_seq;
    int      seq_ok;
} rx_collect_t;

static void on_frame(const pcm_frame_t *frame, void *user)
{
    rx_collect_t *c = (rx_collect_t *)user;
    c->count++;
    c->last_seq = frame->seq;
}

static void test_stream_resync(void)
{
    printf("test_stream_resync\n");

    uint8_t scratch[PCM_LINK_FRAME_MAX_WIRE];
    rx_collect_t collected = {0};
    pcm_link_rx_t rx;
    pcm_link_rx_init(&rx, scratch, sizeof(scratch), on_frame, &collected);

    uint8_t payload[128];
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)i;
    }

    /* Build a stream: [garbage][frame0][frame1][frame2], fed byte-by-byte
     * and also in odd-sized chunks, to mimic UART DMA fragmentation. */
    uint8_t stream[4096];
    size_t n = 0;

    /* Leading garbage with no delimiter, then a stray delimiter to resync. */
    for (int i = 0; i < 37; i++) {
        stream[n++] = (uint8_t)(0x80 | i);
    }
    stream[n++] = PCM_LINK_DELIMITER; /* flush the garbage run */

    for (int s = 0; s < 3; s++) {
        n += pcm_frame_build((uint8_t)s, payload, sizeof(payload), &stream[n]);
    }

    /* Feed in 1, 3, 7, 13-byte chunks cycling. */
    size_t i = 0;
    int sizes[] = {1, 3, 7, 13};
    int si = 0;
    while (i < n) {
        size_t chunk = sizes[si++ % 4];
        if (i + chunk > n) { chunk = n - i; }
        pcm_link_rx_feed(&rx, &stream[i], chunk);
        i += chunk;
    }

    CHECK(collected.count == 3, "expected 3 frames, got %d", collected.count);
    CHECK(rx.frames_ok == 3, "frames_ok=%u", rx.frames_ok);
    CHECK(rx.err_crc == 0, "unexpected crc errors=%u", rx.err_crc);
    CHECK(collected.last_seq == 2, "last seq=%u", collected.last_seq);

    /* Now corrupt the middle of a valid stream and confirm the next 0x00
     * re-aligns framing (one frame lost, the rest recovered). */
    collected.count = 0;
    pcm_link_rx_init(&rx, scratch, sizeof(scratch), on_frame, &collected);

    n = 0;
    for (int s = 0; s < 5; s++) {
        size_t before = n;
        n += pcm_frame_build((uint8_t)s, payload, sizeof(payload), &stream[n]);
        if (s == 2) {
            /* Corrupt a byte inside frame 2 (not the delimiter). */
            stream[before + 2] ^= 0xFF;
        }
    }
    pcm_link_rx_feed(&rx, stream, n);

    CHECK(collected.count >= 4, "resync lost too many frames: %d ok",
          collected.count);
    CHECK((rx.err_crc + rx.err_cobs + rx.err_length) >= 1,
          "corruption silently accepted");
    printf("  resync: %u ok, %u crc, %u cobs, %u len, %u overflow\n",
           rx.frames_ok, rx.err_crc, rx.err_cobs, rx.err_length, rx.overflows);
}

/* --------------------------------------------------------------------- */
/* Overhead measurement (informational, feeds the deliverable's metrics)  */
/* --------------------------------------------------------------------- */

static void test_overhead_report(void)
{
    printf("test_overhead_report\n");
    uint8_t payload[PCM_LINK_PAYLOAD_BYTES];
    uint32_t s = 99;
    for (size_t i = 0; i < sizeof(payload); i++) {
        s = s * 1103515245u + 12345u;
        payload[i] = (uint8_t)(s >> 16);
    }
    uint8_t wire[PCM_LINK_FRAME_MAX_WIRE];
    size_t w = pcm_frame_build(1, payload, sizeof(payload), wire);

    size_t raw = PCM_LINK_FRAME_HEADER + PCM_LINK_PAYLOAD_BYTES + PCM_LINK_FRAME_TRAILER;
    double overhead_pct = 100.0 * ((double)w - (double)PCM_LINK_PAYLOAD_BYTES) /
                          (double)PCM_LINK_PAYLOAD_BYTES;
    printf("  payload=%d raw_frame=%zu wire=%zu (incl delim) overhead=%.2f%%\n",
           PCM_LINK_PAYLOAD_BYTES, raw, w, overhead_pct);
    CHECK(w <= PCM_LINK_FRAME_MAX_WIRE, "wire exceeds declared max");
}

int main(void)
{
    printf("=== pcm_link host tests ===\n");
    test_cobs();
    test_frame();
    test_stream_resync();
    test_overhead_report();

    printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
