/*
 * podcast_parse.c — see podcast_parse.h.
 */
#include "podcast_parse.h"
#include <string.h>

/* Copy `src`[0..len) into `out` (cap), decoding &amp; -> &. NUL-terminates. */
static size_t copy_decoded(const char *src, size_t len, char *out, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; i < len && o + 1 < cap; i++) {
        if (src[i] == '&' && i + 4 < len && strncmp(&src[i], "&amp;", 5) == 0) {
            out[o++] = '&';
            i += 4;
        } else {
            out[o++] = src[i];
        }
    }
    out[o] = '\0';
    return o;
}

/* Within the tag starting at `tag` (up to its '>'), find url="..." / url='...'. */
static size_t url_in_tag(const char *tag, char *out, size_t cap)
{
    const char *end = strchr(tag, '>');
    if (!end) {
        return 0;
    }
    const char *u = tag;
    while ((u = strstr(u, "url=")) != NULL && u < end) {
        const char *v = u + 4;
        char q = (v < end) ? *v : '\0';
        if (q == '"' || q == '\'') {
            v++;
            if (v < end) {
                const char *vend = memchr(v, q, (size_t)(end - v));
                if (vend) {
                    return copy_decoded(v, (size_t)(vend - v), out, cap);
                }
            }
        }
        u += 4;
    }
    return 0;
}

size_t podcast_extract_enclosure(const char *xml, char *out, size_t cap)
{
    if (!xml || !out || cap == 0) {
        return 0;
    }
    out[0] = '\0';

    /* Prefer <enclosure>; fall back to <media:content>. Take the first (newest)
     * with an audio-ish url. */
    const char *tags[] = { "<enclosure", "<media:content" };
    for (size_t t = 0; t < sizeof(tags) / sizeof(tags[0]); t++) {
        const char *e = strstr(xml, tags[t]);
        if (e) {
            size_t n = url_in_tag(e, out, cap);
            if (n > 0) {
                return n;
            }
        }
    }
    return 0;
}
