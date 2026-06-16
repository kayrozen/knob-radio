/*
 * control_msg.h — codec for the UART control plane (PCM_LINK_FRAME_CONTROL).
 *
 * A control message is `[opcode][args...]` carried in a CONTROL frame's
 * payload. This module builds a complete CONTROL frame (ready for the UART)
 * and parses a received frame back into an opcode + args view. It is the
 * foundation the later phases (BT pairing, AVRCP relay, OTA push, version
 * handshake) build their messages on — see plan §1.3.
 *
 * Pure C, no ESP-IDF deps, host-testable alongside the rest of pcm_link.
 */
#ifndef PCM_LINK_CONTROL_MSG_H
#define PCM_LINK_CONTROL_MSG_H

#include <stddef.h>
#include <stdint.h>
#include "pcm_link_proto.h"
#include "pcm_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Max arg bytes a control message can carry (payload minus the opcode byte). */
#define PCM_LINK_CTRL_MAX_ARGS  (PCM_LINK_PAYLOAD_BYTES - 1)

/* Parsed view of a received control message. `args` points into the source
 * frame's payload, so it stays valid only as long as that frame does. */
typedef struct {
    uint8_t        op;        /* pcm_link_ctrl_op_t */
    uint16_t       arg_len;   /* number of arg bytes */
    const uint8_t *args;      /* arg bytes (NULL when arg_len == 0) */
} pcm_link_ctrl_msg_t;

/*
 * Build a complete CONTROL frame (COBS-framed, delimiter-terminated) carrying
 * `op` + `arg_len` arg bytes into `wire` (>= PCM_LINK_FRAME_MAX_WIRE).
 * Returns the wire byte count, or 0 on bad arguments (arg_len too large,
 * NULL wire, or NULL args with arg_len > 0).
 */
size_t pcm_link_ctrl_build_frame(uint8_t seq, uint8_t op,
                                 const uint8_t *args, uint16_t arg_len,
                                 uint8_t *wire);

/* Convenience: a control frame whose only argument is a single byte (the many
 * <u8> opcodes — VERSION, BT_STATUS, AVRCP_EVENT, FLOW, ...). */
size_t pcm_link_ctrl_build_u8_frame(uint8_t seq, uint8_t op, uint8_t arg,
                                    uint8_t *wire);

/*
 * Interpret a parsed frame as a control message. Returns 1 and fills `out`
 * when `frame->type == PCM_LINK_FRAME_CONTROL` and it carries at least the
 * opcode byte; returns 0 otherwise (not a control frame / empty).
 */
int pcm_link_ctrl_parse(const pcm_frame_t *frame, pcm_link_ctrl_msg_t *out);

#ifdef __cplusplus
}
#endif

#endif /* PCM_LINK_CONTROL_MSG_H */
