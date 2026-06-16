/*
 * uart_reader.c — see uart_reader.h.
 */
#include "uart_reader.h"
#include "pcm_link_proto.h"
#include "pcm_link_rx.h"
#include "control_msg.h"
#include "dac_mute.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "uart_rx";

#define FORWARD_UART_NUM    UART_NUM_1
#define RX_RING_BYTES       (16 * 1024)   /* generous UART driver ring */
#define READ_CHUNK          1024

/* Reassembler scratch + a silence block for lost-frame fill. */
static uint8_t s_rx_scratch[PCM_LINK_FRAME_MAX_WIRE];
static uint8_t s_silence[PCM_LINK_PAYLOAD_BYTES];

/* Dispatch a received control-plane frame. Phase 1 only decodes and logs the
 * opcode; the per-opcode behaviour (BT pairing, AVRCP relay, OTA, version
 * handshake) lands in its later phase. */
static void on_control(uart_reader_ctx_t *ctx, const pcm_frame_t *frame)
{
    ctx->ctrl_frames++;

    pcm_link_ctrl_msg_t msg;
    if (!pcm_link_ctrl_parse(frame, &msg)) {
        return;
    }

    switch (msg.op) {
    case PCM_LINK_CTRL_DAC_MUTE:
        /* analog mode: the S3 owns the DAC and asks us to (un)mute XSMT. */
        if (msg.arg_len >= 1) {
            dac_mute_set(msg.args[0] != 0);
        }
        break;
    default:
        ESP_LOGI(TAG, "control op=0x%02x args=%u", msg.op, (unsigned)msg.arg_len);
        break;
    }
}

/* Called by the reassembler for each CRC-valid frame; dispatch on type. */
static void on_frame(const pcm_frame_t *frame, void *user)
{
    uart_reader_ctx_t *ctx = (uart_reader_ctx_t *)user;

    if (frame->type != PCM_LINK_FRAME_AUDIO) {
        if (frame->type == PCM_LINK_FRAME_CONTROL) {
            on_control(ctx, frame);
        }
        /* OTA_DATA / ART_DATA: handled in their later phases. */
        return;
    }

    /* Detect lost AUDIO frames via sequence continuity and insert equivalent
     * silence so audio timing does not slip. Sequence tracking is per audio
     * stream, so interleaved control frames never look like an audio gap. */
    if (ctx->have_seq) {
        uint8_t expected = (uint8_t)(ctx->last_seq + 1);
        uint8_t gap = (uint8_t)(frame->seq - expected);
        for (uint8_t g = 0; g < gap; g++) {
            jb_push(ctx->jb, s_silence, sizeof(s_silence));
            ctx->silence_bytes += sizeof(s_silence);
            ctx->lost_frames++;
        }
    }
    ctx->have_seq = true;
    ctx->last_seq = frame->seq;

    if (frame->length) {
        jb_push(ctx->jb, frame->payload, frame->length);
    }
}

static void uart_rx_task(void *arg)
{
    uart_reader_ctx_t *ctx = (uart_reader_ctx_t *)arg;

    pcm_link_rx_t rx;
    pcm_link_rx_init(&rx, s_rx_scratch, sizeof(s_rx_scratch), on_frame, ctx);

    uint8_t *chunk = malloc(READ_CHUNK);
    configASSERT(chunk);

    TickType_t last_log = xTaskGetTickCount();

    for (;;) {
        int n = uart_read_bytes(FORWARD_UART_NUM, chunk, READ_CHUNK,
                                pdMS_TO_TICKS(20));
        if (n > 0) {
            pcm_link_rx_feed(&rx, chunk, (size_t)n);
        }

        /* Periodic metrics (the deliverable wants integrity + loss figures). */
        if (xTaskGetTickCount() - last_log > pdMS_TO_TICKS(5000)) {
            last_log = xTaskGetTickCount();
            ESP_LOGI(TAG,
                     "ok=%lu crc=%lu cobs=%lu len=%lu ovf=%lu | lost=%lu "
                     "ctrl=%lu jb=%lums under=%lu over=%lu",
                     (unsigned long)rx.frames_ok, (unsigned long)rx.err_crc,
                     (unsigned long)rx.err_cobs, (unsigned long)rx.err_length,
                     (unsigned long)rx.overflows, (unsigned long)ctx->lost_frames,
                     (unsigned long)ctx->ctrl_frames,
                     (unsigned long)jb_filled_ms(ctx->jb),
                     (unsigned long)ctx->jb->underruns,
                     (unsigned long)ctx->jb->overruns);
        }
    }
}

void uart_reader_start(uart_reader_ctx_t *ctx, jitter_buffer_t *jb)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->jb = jb;
    memset(s_silence, 0, sizeof(s_silence));

    const uart_config_t cfg = {
        .baud_rate = PCM_LINK_FORWARD_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(FORWARD_UART_NUM, RX_RING_BYTES, 0, 0,
                                        NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(FORWARD_UART_NUM, &cfg));
    /* Full-duplex on one UART: RX = forward PCM (GPIO18), TX = return
     * backpressure (GPIO23). Both run at the forward baud; the return
     * direction is only a few control bytes. */
    ESP_ERROR_CHECK(uart_set_pin(FORWARD_UART_NUM, PCM_LINK_U4WDH_TX_GPIO,
                                 PCM_LINK_U4WDH_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART1 RX GPIO%d / TX GPIO%d @ %d baud",
             PCM_LINK_U4WDH_RX_GPIO, PCM_LINK_U4WDH_TX_GPIO,
             PCM_LINK_FORWARD_BAUD);

    /* Pin to APP core (core 1); A2DP/BT lives on core 0. */
    xTaskCreatePinnedToCore(uart_rx_task, "uart_rx", 4096, ctx, 12, NULL, 1);
}
