/*
 * control_msg.c — see control_msg.h.
 */
#include "control_msg.h"
#include <string.h>

size_t pcm_link_ctrl_build_frame(uint8_t seq, uint8_t op,
                                 const uint8_t *args, uint16_t arg_len,
                                 uint8_t *wire)
{
    if (wire == NULL || arg_len > PCM_LINK_CTRL_MAX_ARGS) {
        return 0;
    }
    if (arg_len > 0 && args == NULL) {
        return 0;
    }

    /* Control payload: opcode byte followed by the args. */
    uint8_t payload[PCM_LINK_PAYLOAD_BYTES];
    payload[0] = op;
    if (arg_len) {
        memcpy(&payload[1], args, arg_len);
    }

    return pcm_frame_build_typed(PCM_LINK_FRAME_CONTROL, seq, payload,
                                 (uint16_t)(arg_len + 1), wire);
}

size_t pcm_link_ctrl_build_u8_frame(uint8_t seq, uint8_t op, uint8_t arg,
                                    uint8_t *wire)
{
    return pcm_link_ctrl_build_frame(seq, op, &arg, 1, wire);
}

int pcm_link_ctrl_parse(const pcm_frame_t *frame, pcm_link_ctrl_msg_t *out)
{
    if (frame == NULL || out == NULL) {
        return 0;
    }
    if (frame->type != PCM_LINK_FRAME_CONTROL || frame->length < 1) {
        return 0;   /* not a control frame, or missing the opcode byte */
    }

    out->op      = frame->payload[0];
    out->arg_len = (uint16_t)(frame->length - 1);
    out->args    = out->arg_len ? &frame->payload[1] : NULL;
    return 1;
}
