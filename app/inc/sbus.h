/* SPDX-License-Identifier: MIT */
/*
 * SBUS receiver driver.
 *
 * Decodes one SBUS frame per reception. The driver only uses the UART
 * subsystem contract: uart_recv_async() arms one reception, the adapter
 * reports the completed frame through a callback, and the driver re-arms the
 * next reception from task context in sbus_process().
 */

#ifndef SBUS_H
#define SBUS_H

#include <stdint.h>
#include <stdbool.h>

#include "subsystems/subsystems_uart.h"

#define SBUS_FRAME_SIZE 25
#define SBUS_CHANNEL_MIN 172
#define SBUS_CHANNEL_MID 1024
#define SBUS_CHANNEL_MAX 1811

typedef enum {
	AIL = 0,
	ELE = 1,
	THR = 2,
	RUD = 3,

	S1 = 4,
	S2 = 5,
	S3 = 6,
	S4 = 7,

	SA = 8,
	SB = 9,
	SC = 10,
	SD = 11,
	SE = 12,
	SF = 13,
	SG = 14,
	SH = 15,
} SBUS_MAPPING;

typedef enum {
	/*
	 * Three-position switch: below 682 is TOP, 682 to 1365 is MID,
	 * above that is BUT.
	 */
	TOP = 0,
	MID = 1,
	BUT = 2,
} SBUS_SWITCH_POS;

typedef union {
	struct __attribute__((packed)) {
		uint8_t start_byte; /* 0x0F */
		/* 16 channels x 11 bits = 22 bytes packed */
		uint8_t channel_data[22];
		/* bit7: CH17, bit6: CH18, bit5: frame lost, bit4: failsafe */
		uint8_t flags;
		uint8_t end_byte; /* 0x00 */
	} sbus_frame;
	uint8_t raw[25];
} sbus_frame_t;

struct sbus_data {
	float ail;
	float ele;
	float thr;
	float rud;

	float s1;
	float s2;
	float s3;
	float s4;

	uint8_t sa;
	uint8_t sb;
	uint8_t sc;
	uint8_t sd;
	uint8_t se;
	uint8_t sf;
	uint8_t sg;
	uint8_t sh;
};

/**
 * struct sbus - SBUS receiver instance
 * @msg: last decoded frame, refreshed by sbus_process()
 * @uart: UART port carrying the SBUS signal, provided by the caller
 * @rx_frame: raw receive buffer, filled directly by the adapter
 * @rx_len: bytes reported by the receive callback
 * @rx_errors: error bitmask reported by the receive callback, see UART_RX_ERR_*
 * @rx_done: set by the receive callback when a reception has finished
 * @rx_armed: task context only, a reception is currently in flight
 * @new_frame_ready: %true while @msg holds a frame the application has not read
 *
 * @rx_frame doubles as the receive buffer, so the adapter may overwrite it as
 * soon as the next reception starts. Read @msg, not @rx_frame, once
 * sbus_process() has returned. @new_frame_ready is cleared by the application
 * after it has consumed @msg.
 */
struct sbus {
	struct sbus_data msg;
	struct uart_dev *uart;
	sbus_frame_t rx_frame;
	volatile uint16_t rx_len;
	volatile uint32_t rx_errors;
	volatile bool rx_done;
	bool rx_armed;
	bool new_frame_ready;
};

/**
 * sbus_init() - initialize a receiver
 * @s: receiver instance
 * @uart: UART port carrying the SBUS signal, bound by board code
 *
 * Context: task context, before the first sbus_process() call. The first
 * reception is armed by sbus_process(), not here.
 */
void sbus_init(struct sbus *s, struct uart_dev *uart);

/**
 * sbus_process() - decode a completed reception and arm the next one
 * @s: receiver instance
 *
 * Context: task context, typically once per main loop iteration.
 *
 * Must be called often enough that the next reception is armed before the
 * transmitter starts the following frame; bytes arriving while no reception
 * is armed are lost.
 *
 * Return: %true when @s->msg was refreshed, %false otherwise.
 */
bool sbus_process(struct sbus *s);

/**
 * sbus_uart_rx_cb() - receive callback, registered through uart_recv_async()
 * @dev: UART device that completed the reception
 * @len: number of bytes received
 * @errors: error bitmask, 0 when the frame is clean, see UART_RX_ERR_*
 * @ctx: receiver instance, passed as @ctx to uart_recv_async()
 *
 * Context: interrupt context. Latches the result only; decoding and re-arming
 * are done by sbus_process() in task context.
 */
void sbus_uart_rx_cb(struct uart_dev *dev, size_t len, uint32_t errors,
		     void *ctx);

/**
 * sbus_parse() - validate a raw SBUS frame
 * @data: raw frame bytes
 * @len: number of bytes in @data
 *
 * Context: any context.
 *
 * Return: %true when @data holds a well-formed frame.
 */
bool sbus_parse(const uint8_t *data, uint16_t len);

/**
 * sbus_parse_to_msg() - decode the receiver buffer into @s->msg
 * @s: receiver instance
 *
 * Context: task context.
 *
 * Decodes @s->rx_frame, which is the buffer the adapter receives into.
 *
 * Return: %true when @s->msg was refreshed.
 */
bool sbus_parse_to_msg(struct sbus *s);

/**
 * sbus_get_one_channel() - extract one raw channel value from a frame
 * @s: receiver instance
 * @ch: channel index, %0 to %15
 *
 * Context: any context. Reads the receive buffer, so the result is only
 * valid until the next reception starts; use the decoded values in @s->msg
 * once sbus_process() has returned.
 *
 * Return: raw channel value, 11 bit wide.
 */
static inline uint16_t sbus_get_one_channel(struct sbus *s, uint8_t ch)
{
	const uint8_t *raw = s->rx_frame.raw;
	uint16_t bit_index = ch * 11;
	uint8_t byte_index = bit_index / 8;
	uint8_t bit_offset = bit_index % 8;

	uint32_t val = (uint32_t)raw[1 + byte_index] |
		       ((uint32_t)raw[1 + byte_index + 1] << 8) |
		       ((uint32_t)raw[1 + byte_index + 2] << 16);

	return (val >> bit_offset) & 0x07FF;
}

#endif /* SBUS_H */
