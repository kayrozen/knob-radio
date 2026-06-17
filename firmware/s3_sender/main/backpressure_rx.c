/*
 * backpressure_rx.c — see backpressure_rx.h.
 */
#include "backpressure_rx.h"
#include "pcm_link_proto.h"
#include "pcm_link_rx.h"
#include "control_msg.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <stdatomic.h>

static const char *TAG = "ret_rx";

#define RETURN_UART_NUM  UART_NUM_1   /* shared full-duplex UART (see uart_writer) */
#define READ_CHUNK       256

/* Pacing multiplier on the frame period. SLOWDOWN -> longer period (>1),
 * SPEEDUP -> shorter period (<1). Stored as an atomic state, mapped on read. */
#define RATE_SLOW   1.03
#define RATE_FAST   0.97
#define RATE_NORMAL 1.00

enum { ST_NORMAL = 0, ST_SLOW = 1, ST_FAST = 2 };
static atomic_int s_state = ST_NORMAL;

static return_avrcp_cb_t     s_avrcp_cb;
static return_bt_status_cb_t s_bt_cb;

void backpressure_rx_set_handlers(return_avrcp_cb_t avrcp,
                                  return_bt_status_cb_t bt_status)
{
    s_avrcp_cb = avrcp;
    s_bt_cb    = bt_status;
}

double backpressure_rate(void)
{
    switch (atomic_load(&s_state)) {
        case ST_SLOW: return RATE_SLOW;
        case ST_FAST: return RATE_FAST;
        default:      return RATE_NORMAL;
    }
}

static void apply_flow(uint8_t cmd)
{
    switch (cmd) {
    case PCM_LINK_BP_SLOWDOWN: atomic_store(&s_state, ST_SLOW);   break;
    case PCM_LINK_BP_SPEEDUP:  atomic_store(&s_state, ST_FAST);   break;
    case PCM_LINK_BP_NORMAL:   atomic_store(&s_state, ST_NORMAL); break;
    default: break;
    }
}

/* Reassembled control frame from the bridge. */
static void on_frame(const pcm_frame_t *frame, void *user)
{
    (void)user;
    pcm_link_ctrl_msg_t msg;
    if (!pcm_link_ctrl_parse(frame, &msg)) {
        return;
    }
    switch (msg.op) {
    case PCM_LINK_CTRL_FLOW:
        if (msg.arg_len >= 1) {
            apply_flow(msg.args[0]);
        }
        break;
    case PCM_LINK_CTRL_AVRCP_EVENT:
        if (msg.arg_len >= 1 && s_avrcp_cb) {
            s_avrcp_cb(msg.args[0]);
        }
        break;
    case PCM_LINK_CTRL_BT_STATUS:
        if (msg.arg_len >= 1 && s_bt_cb) {
            s_bt_cb(msg.args[0]);
        }
        break;
    case PCM_LINK_CTRL_BT_SCAN_RESULT:
        /* mac[6] + name; the portal will surface these (Phase 10 hook). */
        ESP_LOGI(TAG, "scan result (%u bytes)", (unsigned)msg.arg_len);
        break;
    default:
        break;
    }
}

static void return_rx_task(void *arg)
{
    (void)arg;
    static uint8_t scratch[PCM_LINK_FRAME_MAX_WIRE];
    pcm_link_rx_t rx;
    pcm_link_rx_init(&rx, scratch, sizeof(scratch), on_frame, NULL);

    uint8_t chunk[READ_CHUNK];
    for (;;) {
        int n = uart_read_bytes(RETURN_UART_NUM, chunk, sizeof(chunk),
                                pdMS_TO_TICKS(200));
        if (n > 0) {
            pcm_link_rx_feed(&rx, chunk, (size_t)n);
        }
    }
}

void backpressure_rx_start(void)
{
    xTaskCreatePinnedToCore(return_rx_task, "ret_rx", 3072, NULL, 6, NULL, 1);
}
