/*
 * test_podcast.c — host tests for the pure RSS enclosure extractor.
 */
#include "podcast_parse.h"

#include <stdio.h>
#include <string.h>

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

int main(void)
{
    printf("=== podcast host tests ===\n");
    char out[256];

    /* Newest episode first; attributes in any order. */
    const char *rss =
        "<rss><channel><title>Show</title>"
        "<item><title>Ep 9</title>"
        "<enclosure length=\"123\" type=\"audio/mpeg\" url=\"https://cdn.example.com/ep9.mp3\"/>"
        "</item>"
        "<item><title>Ep 8</title>"
        "<enclosure url=\"https://cdn.example.com/ep8.mp3\" type=\"audio/mpeg\"/>"
        "</item></channel></rss>";
    size_t n = podcast_extract_enclosure(rss, out, sizeof(out));
    CHECK(n > 0, "should find an enclosure");
    CHECK(strcmp(out, "https://cdn.example.com/ep9.mp3") == 0,
          "newest episode url, got '%s'", out);

    /* &amp; decoding. */
    const char *amp =
        "<item><enclosure url=\"https://x.com/a.mp3?u=1&amp;t=2\"/></item>";
    podcast_extract_enclosure(amp, out, sizeof(out));
    CHECK(strcmp(out, "https://x.com/a.mp3?u=1&t=2") == 0,
          "&amp; decoded, got '%s'", out);

    /* media:content fallback (single quotes). */
    const char *media =
        "<item><media:content url='http://y.com/b.aac' type='audio/aac'/></item>";
    podcast_extract_enclosure(media, out, sizeof(out));
    CHECK(strcmp(out, "http://y.com/b.aac") == 0,
          "media:content url, got '%s'", out);

    /* No enclosure -> empty. */
    const char *none = "<rss><channel><item><title>x</title></item></channel></rss>";
    CHECK(podcast_extract_enclosure(none, out, sizeof(out)) == 0,
          "no enclosure -> 0");
    CHECK(out[0] == '\0', "out cleared when none found");

    printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
