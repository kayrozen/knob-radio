/*
 * pcm_source.c — test PCM generator (see pcm_source.h).
 *
 * Generates a continuous L/R tone pair (different frequencies per channel so a
 * scope/ear can confirm stereo integrity end-to-end). Deterministic and
 * codec-free; replaced by the ESP-ADF pipeline output in Phase E.
 */
#include "pcm_source.h"
#include "pcm_link_proto.h"

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
    size_t samples = len / sizeof(int16_t);      /* total int16 slots */

    for (size_t i = 0; i + 1 < samples; i += 2) {
        out[i]     = (int16_t)(AMPLITUDE * sin(s_phase_l)); /* L */
        out[i + 1] = (int16_t)(AMPLITUDE * sin(s_phase_r)); /* R */
        s_phase_l += dl;
        s_phase_r += dr;
        if (s_phase_l > 2.0 * M_PI) { s_phase_l -= 2.0 * M_PI; }
        if (s_phase_r > 2.0 * M_PI) { s_phase_r -= 2.0 * M_PI; }
    }
    return len;
}
