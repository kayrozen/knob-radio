/*
 * pcm_source.c — PCM producer (see pcm_source.h).
 *
 * Two implementations selected by Kconfig:
 *   - TONE (default): deterministic L/R test tone, no WiFi. Phases B-D.
 *   - ADF: pulls real internet-radio PCM from the ESP-ADF pipeline. Phase E.
 *
 * Either way the output contract is identical (44.1k/16/stereo, always returns
 * `len`), so the downstream framing/UART writer never changes.
 */
#include "pcm_source.h"
#include "pcm_link_proto.h"
#include "sdkconfig.h"

#include <string.h>

#if defined(CONFIG_PRESET_AUDIO_SOURCE_ADF)

#include "adf_pipeline.h"

void pcm_source_init(void) { /* pipeline is started from app_main */ }

size_t pcm_source_read(uint8_t *dst, size_t len)
{
    size_t got = 0;
    while (got < len) {
        int n = adf_pipeline_read(dst + got, len - got);
        if (n <= 0) {
            break;   /* (re)buffering or mid-switch */
        }
        got += (size_t)n;
    }
    if (got < len) {
        /* Silence-fill the shortfall: keeps the link cadence and the U4WDH's
         * A2DP connection alive across rebuffering and station switches. */
        memset(dst + got, 0, len - got);
    }
    return len;
}

#else  /* CONFIG_PRESET_AUDIO_SOURCE_TONE (default) */

#include <math.h>

#define LEFT_HZ   440.0   /* A4 */
#define RIGHT_HZ  554.37  /* C#5 -> audibly distinct from L */
#define AMPLITUDE 12000   /* < INT16_MAX, leaves headroom */

static double s_phase_l;
static double s_phase_r;

void pcm_source_init(void)
{
    s_phase_l = 0.0;
    s_phase_r = 0.0;
}

size_t pcm_source_read(uint8_t *dst, size_t len)
{
    const double dl = 2.0 * M_PI * LEFT_HZ  / PCM_LINK_SAMPLE_RATE_HZ;
    const double dr = 2.0 * M_PI * RIGHT_HZ / PCM_LINK_SAMPLE_RATE_HZ;

    int16_t *out = (int16_t *)dst;
    size_t samples = len / sizeof(int16_t);

    for (size_t i = 0; i + 1 < samples; i += 2) {
        out[i]     = (int16_t)(AMPLITUDE * sin(s_phase_l));
        out[i + 1] = (int16_t)(AMPLITUDE * sin(s_phase_r));
        s_phase_l += dl;
        s_phase_r += dr;
        if (s_phase_l > 2.0 * M_PI) { s_phase_l -= 2.0 * M_PI; }
        if (s_phase_r > 2.0 * M_PI) { s_phase_r -= 2.0 * M_PI; }
    }
    return len;
}

#endif
