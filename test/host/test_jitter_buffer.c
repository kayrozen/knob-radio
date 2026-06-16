/*
 * test_jitter_buffer.c — host-side tests for the U4WDH jitter buffer.
 *
 * Exercises wrap-around, overrun rejection, underrun silence-fill, and the
 * fill/ms accounting the backpressure logic relies on.
 */
#include "jitter_buffer.h"
#include "pcm_link_proto.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int g_failures = 0;
static int g_checks   = 0;

#define CHECK(cond, ...)                                      \
    do {                                                      \
        g_checks++;                                           \
        if (!(cond)) {                                        \
            g_failures++;                                     \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);     \
            printf(__VA_ARGS__);                              \
            printf("\n");                                     \
        }                                                     \
    } while (0)

static void test_basic(void)
{
    printf("test_basic\n");
    uint8_t backing[64];
    jitter_buffer_t jb;
    jb_init(&jb, backing, sizeof(backing));

    CHECK(jb_filled(&jb) == 0, "fresh buffer not empty");
    CHECK(jb_free(&jb) == sizeof(backing) - 1, "free size wrong: %zu", jb_free(&jb));

    uint8_t in[20];
    for (int i = 0; i < 20; i++) { in[i] = (uint8_t)i; }
    CHECK(jb_push(&jb, in, 20), "push failed");
    CHECK(jb_filled(&jb) == 20, "filled=%zu", jb_filled(&jb));

    uint8_t out[20];
    size_t real = jb_pull(&jb, out, 20);
    CHECK(real == 20, "pulled real=%zu", real);
    CHECK(memcmp(in, out, 20) == 0, "data mismatch");
    CHECK(jb_filled(&jb) == 0, "not drained");
}

static void test_wrap(void)
{
    printf("test_wrap\n");
    uint8_t backing[64];
    jitter_buffer_t jb;
    jb_init(&jb, backing, sizeof(backing));

    uint8_t in[40], out[40];
    for (int i = 0; i < 40; i++) { in[i] = (uint8_t)(i + 1); }

    /* Push/pull repeatedly so head/tail wrap past the end. */
    for (int round = 0; round < 10; round++) {
        CHECK(jb_push(&jb, in, 40), "round %d push", round);
        size_t r = jb_pull(&jb, out, 40);
        CHECK(r == 40, "round %d pulled %zu", round, r);
        CHECK(memcmp(in, out, 40) == 0, "round %d wrap corruption", round);
    }
    CHECK(jb.underruns == 0, "spurious underruns %u", jb.underruns);
    CHECK(jb.overruns == 0, "spurious overruns %u", jb.overruns);
}

static void test_overrun(void)
{
    printf("test_overrun\n");
    uint8_t backing[32];
    jitter_buffer_t jb;
    jb_init(&jb, backing, sizeof(backing));

    uint8_t in[40] = {0};
    CHECK(!jb_push(&jb, in, 40), "oversized push should fail");
    CHECK(jb.overruns == 1, "overrun not counted");
    CHECK(jb_filled(&jb) == 0, "failed push left data");
}

static void test_underrun_silence(void)
{
    printf("test_underrun_silence\n");
    uint8_t backing[64];
    jitter_buffer_t jb;
    jb_init(&jb, backing, sizeof(backing));

    uint8_t in[10];
    for (int i = 0; i < 10; i++) { in[i] = 0xAB; }
    jb_push(&jb, in, 10);

    uint8_t out[20];
    memset(out, 0xFF, sizeof(out));
    size_t real = jb_pull(&jb, out, 20);   /* ask for more than available */
    CHECK(real == 10, "real audio bytes=%zu", real);
    CHECK(jb.underruns == 1, "underrun not counted");
    for (int i = 0; i < 10; i++) { CHECK(out[i] == 0xAB, "audio byte %d", i); }
    for (int i = 10; i < 20; i++) { CHECK(out[i] == 0x00, "silence byte %d", i); }
}

static void test_fill_ms(void)
{
    printf("test_fill_ms\n");
    /* One second of audio worth of bytes -> ~1000 ms (minus the 1 spare). */
    size_t cap = PCM_LINK_SAMPLE_RATE_HZ * PCM_LINK_BYTES_PER_SAMPLE; /* 1s */
    static uint8_t backing[44100 * 4];
    jitter_buffer_t jb;
    jb_init(&jb, backing, cap);

    size_t half = (PCM_LINK_SAMPLE_RATE_HZ / 2) * PCM_LINK_BYTES_PER_SAMPLE;
    static uint8_t chunk[44100 * 4];
    memset(chunk, 0, sizeof(chunk));
    jb_push(&jb, chunk, half);
    uint32_t ms = jb_filled_ms(&jb);
    CHECK(ms >= 495 && ms <= 505, "half-second fill reported %lu ms",
          (unsigned long)ms);
}

int main(void)
{
    printf("=== jitter_buffer host tests ===\n");
    test_basic();
    test_wrap();
    test_overrun();
    test_underrun_silence();
    test_fill_ms();
    printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
