/*
 * podcast_parse.h — pure RSS helper (no ESP-IDF deps).
 *
 * A podcast preset stores the RSS feed URL; playback needs the audio URL of the
 * latest episode. RSS feeds are reverse-chronological, so the first <enclosure>
 * (or <media:content>) in the document is the newest episode's audio. This
 * extracts that URL. Pure C so it is unit-tested on the host.
 */
#ifndef PODCAST_PARSE_H
#define PODCAST_PARSE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Write the latest episode's audio URL from RSS `xml` into `out` (cap bytes).
 * Returns the URL length, or 0 if none was found. `&amp;` is decoded to `&`. */
size_t podcast_extract_enclosure(const char *xml, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* PODCAST_PARSE_H */
