/*
 * jitter_buffer.c — see jitter_buffer.h.
 */
#include "jitter_buffer.h"
#include "pcm_link_proto.h"
#include <string.h>

void jb_init(jitter_buffer_t *jb, uint8_t *backing, size_t capacity)
{
    jb->data         = backing;
    jb->capacity     = capacity;
    jb->head         = 0;
    jb->tail         = 0;
    jb->underruns    = 0;
    jb->overruns     = 0;
    jb->pushed_bytes = 0;
    jb->pulled_bytes = 0;
}

size_t jb_filled(const jitter_buffer_t *jb)
{
    size_t head = jb->head;
    size_t tail = jb->tail;
    if (head >= tail) {
        return head - tail;
    }
    return jb->capacity - (tail - head);
}

size_t jb_free(const jitter_buffer_t *jb)
{
    /* Keep one byte unused to distinguish full from empty. */
    return jb->capacity - 1 - jb_filled(jb);
}

uint32_t jb_filled_ms(const jitter_buffer_t *jb)
{
    size_t bytes = jb_filled(jb);
    size_t frames = bytes / PCM_LINK_BYTES_PER_SAMPLE;   /* stereo sample pairs */
    return (uint32_t)((uint64_t)frames * 1000u / PCM_LINK_SAMPLE_RATE_HZ);
}

bool jb_push(jitter_buffer_t *jb, const uint8_t *src, size_t len)
{
    if (len > jb_free(jb)) {
        jb->overruns++;
        return false;
    }
    size_t head = jb->head;
    size_t first = jb->capacity - head;
    if (first > len) {
        first = len;
    }
    memcpy(&jb->data[head], src, first);
    if (len > first) {
        memcpy(&jb->data[0], src + first, len - first);
    }
    jb->head = (head + len) % jb->capacity;
    jb->pushed_bytes += (uint32_t)len;
    return true;
}

size_t jb_pull(jitter_buffer_t *jb, uint8_t *dst, size_t len)
{
    size_t avail = jb_filled(jb);
    size_t take  = (avail < len) ? avail : len;

    size_t tail = jb->tail;
    size_t first = jb->capacity - tail;
    if (first > take) {
        first = take;
    }
    memcpy(dst, &jb->data[tail], first);
    if (take > first) {
        memcpy(dst + first, &jb->data[0], take - first);
    }
    jb->tail = (tail + take) % jb->capacity;
    jb->pulled_bytes += (uint32_t)take;

    if (take < len) {
        /* Underrun: pad with silence so the A2DP callback never stalls. */
        memset(dst + take, 0, len - take);
        jb->underruns++;
    }
    return take;
}
