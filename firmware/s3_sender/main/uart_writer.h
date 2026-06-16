/*
 * uart_writer.h — forward-link transmit for the S3 sender.
 *
 * Reads PCM from pcm_source in PCM_LINK_PAYLOAD_BYTES chunks, builds a logical
 * frame (seq + length + payload + crc8), COBS-encodes it with a 0x00 trailer,
 * and writes it to UART1 TX (GPIO38) via the driver's TX buffer (DMA-backed).
 * Paces itself to ~44.1 kHz, nudged by the backpressure rate multiplier.
 *
 * In the Phase-E ESP-ADF build this becomes an audio_element whose process()
 * drains the pipeline ring buffer instead of pcm_source — the framing/UART
 * code below is identical.
 */
#ifndef UART_WRITER_H
#define UART_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Configure UART1 (TX GPIO38 / RX GPIO48, full duplex) and start the writer. */
void uart_writer_start(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_WRITER_H */
