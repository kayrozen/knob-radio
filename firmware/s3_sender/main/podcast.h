/*
 * podcast.h — podcast playback support (RSS resolve + resume position).
 *
 * A podcast preset's URL is an RSS feed. podcast_resolve() fetches it and
 * returns the latest episode's audio URL. The byte position in that audio is
 * remembered per feed (NVS), so playback resumes where it left off — e.g. after
 * the car's Bluetooth has been gone for more than ~10 s.
 */
#ifndef PODCAST_H
#define PODCAST_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fetch `feed_url` and write the latest episode's audio URL into `out` (cap).
 * Returns true on success. Blocking (HTTP); call off the audio path. */
bool podcast_resolve(const char *feed_url, char *out, size_t cap);

/* Remembered resume position (source byte offset) for a feed; 0 if none. */
uint32_t podcast_pos_get(const char *feed_url);
void     podcast_pos_set(const char *feed_url, uint32_t byte_offset);

#ifdef __cplusplus
}
#endif

#endif /* PODCAST_H */
