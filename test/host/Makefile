# Host-side unit tests. No ESP-IDF, no hardware — pure C with the system
# toolchain. Covers the shared pcm_link protocol, the U4WDH jitter buffer, and
# the (pure) auto-play scheduler.

CC       ?= cc
COMPONENT := ../../components/pcm_link
BRIDGE    := ../../firmware/u4wdh_bridge/main
S3        := ../../firmware/s3_sender/main
CFLAGS   ?= -std=c11 -O2 -Wall -Wextra -Werror -I$(COMPONENT)/include

LINK_SRCS := test_pcm_link.c \
             $(COMPONENT)/src/cobs.c \
             $(COMPONENT)/src/pcm_frame.c \
             $(COMPONENT)/src/pcm_link_rx.c \
             $(COMPONENT)/src/control_msg.c

JB_SRCS   := test_jitter_buffer.c \
             $(BRIDGE)/jitter_buffer.c

SCHED_SRCS := test_schedule.c \
             $(S3)/schedule.c

POD_SRCS  := test_podcast.c \
             $(S3)/podcast_parse.c

.PHONY: all test clean
all: test

test_pcm_link: $(LINK_SRCS)
	$(CC) $(CFLAGS) $(LINK_SRCS) -o $@

test_jitter_buffer: $(JB_SRCS)
	$(CC) $(CFLAGS) -I$(BRIDGE) $(JB_SRCS) -o $@

test_schedule: $(SCHED_SRCS)
	$(CC) $(CFLAGS) -I$(S3) $(SCHED_SRCS) -o $@

test_podcast: $(POD_SRCS)
	$(CC) $(CFLAGS) -I$(S3) $(POD_SRCS) -o $@

test: test_pcm_link test_jitter_buffer test_schedule test_podcast
	./test_pcm_link
	./test_jitter_buffer
	./test_schedule
	./test_podcast

clean:
	rm -f test_pcm_link test_jitter_buffer test_schedule test_podcast
