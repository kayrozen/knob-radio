/*
 * backpressure_rx.c — see backpressure_rx.h.
 */
#include "backpressure_rx.h"
#include "pcm_link_proto.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <stdatomic.h>

static const char *TAG = "bp_rx";

#define RETURN_UART_NUM  UART_NUM_1   /* shared full-duplex UART (see uart_writer) */

/* Pacing multiplier on the frame period. SLOWDOWN -> longer period (>1),
 * SPEEDUP -> shorter period (<1). Small step keeps control gentle.
 * Stored as an atomic int state (avoids 64-bit atomics) and mapped to a
 * multiplier on read. */
#define RATE_SLOW   1.03
#define RATE_FAST   0.97
#define RATE_NORMAL 1.00

enum { ST_NORMAL = 0, ST_SLOW = 1, ST_FAST = 2 };
static atomic_int s_state = ST_NORMAL;

double backpressure_rate(void)
{
    switch (atomic_load(&s_state)) {
        case ST_SLOW: return RATE_SLOW;
        case ST_FAST: return RATE_FAST;
        default:      return RATE_NORMAL;
    }
}

static void bp_rx_task(void *arg)
{
    (void)arg;
    uint8_t b;
    for (;;) {
        int n = uart_read_bytes(RETURN_UART_NUM, &b, 1, pdMS_TO_TICKS(200));
        if (n != 1) {
            continue;
        }
        switch (b) {
        case PCM_LINK_BP_SLOWDOWN:
            atomic_store(&s_state, ST_SLOW);
            ESP_LOGD(TAG, "slowdown");
            break;
        case PCM_LINK_BP_SPEEDUP:
            atomic_store(&s_state, ST_FAST);
            ESP_LOGD(TAG, "speedup");
            break;
        case PCM_LINK_BP_NORMAL:
            atomic_store(&s_state, ST_NORMAL);
            break;
        default:
            break; /* noise/garbage: ignore */
        }
    }
}

void backpressure_rx_start(void)
{
    xTaskCreatePinnedToCore(bp_rx_task, "bp_rx", 2560, NULL, 6, NULL, 1);
}
