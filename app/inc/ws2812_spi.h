/* SPDX-License-Identifier: MIT */
/*
 * WS2812 addressable LED driver.
 *
 * The LED data line is driven through an SPI master: every WS2812 bit is
 * encoded as several SPI bits, so one byte of color data becomes eight SPI
 * bytes and one 24-bit pixel becomes 24 SPI bytes. The driver only uses the
 * SPI master subsystem contract; the SPI device carrying the LED data line is
 * bound by board code.
 *
 * The bound SPI device must use 8 data bits, MSB-first bit order, and a clock
 * whose period is one eighth of a WS2812 bit period. At the 800 kHz WS2812 bit
 * rate that is 6.4 MHz; the timing constants below assume a 6 MHz clock.
 */

#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>

#include "subsystems/subsystems_spi_master.h"

#define WS2812_LED_NUM 1
#define WS2812_SPI_BYTES_PER_COLOR 8
#define WS2812_SPI_BYTES_PER_LED (3 * WS2812_SPI_BYTES_PER_COLOR)
/*
 * Reset pulse: the line is held low for more than 50 us to latch a frame.
 * At 6 MHz, 100 low bytes take about 133 us.
 */
#define WS2812_RESET_BYTES 100

/*
 * Encoded WS2812 bits, before the right shift applied at encode time.
 *
 * WS2812B expects T0H in 200..500 ns and T1H in 550..850 ns. At 6 MHz one SPI
 * bit is 166.7 ns, so two high bits (333 ns) encode a 0 and four high bits
 * (667 ns) encode a 1.
 */
#define WS2812_SPI_BIT_0 0xC0 /* 0x60 after shift: 2 bits high */
#define WS2812_SPI_BIT_1 0xF0 /* 0x78 after shift: 4 bits high */

#define WS2812_SPI_TIMEOUT_MS 10

/**
 * struct ws2812_pixel - color of one LED in the chain
 * @index: position in the chain, must be below WS2812_LED_NUM
 * @r: red intensity, 0-255
 * @g: green intensity, 0-255
 * @b: blue intensity, 0-255
 */
struct ws2812_pixel {
	uint16_t index;
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

/**
 * struct ws2812 - WS2812 LED chain instance
 * @spi: SPI master device driving the LED data line, bound by board code
 * @tx_buf: encoded SPI stream, image bytes followed by the reset pulse
 *
 * The stream is built by ws2812_set_pixel() and shifted out as a single
 * transfer by ws2812_show(). A freshly initialized instance holds an all-zero
 * stream, which is a black image followed by the reset pulse, so ws2812_show()
 * may be called before any pixel has been set.
 */
struct ws2812 {
	struct spi_master_dev *spi;
	uint8_t tx_buf[(WS2812_LED_NUM * WS2812_SPI_BYTES_PER_LED) +
		       WS2812_RESET_BYTES];
};

/**
 * ws2812_init() - initialize an LED chain
 * @ws: instance to initialize
 * @spi: SPI master device driving the LED data line, bound by board code
 *
 * Context: task context, before the first ws2812_show() call.
 *
 * The line parameters of @spi are fixed by board code at bind time; this
 * function only clears the encoded stream.
 */
void ws2812_init(struct ws2812 *ws, struct spi_master_dev *spi);

/**
 * ws2812_set_pixel() - encode one pixel into the transmit stream
 * @ws: instance to update
 * @pixel: color to encode
 *
 * Context: any context.
 *
 * Only updates the transmit stream; the chain is not driven until
 * ws2812_show() is called. A pixel whose @index is beyond the chain is
 * ignored.
 */
void ws2812_set_pixel(struct ws2812 *ws, const struct ws2812_pixel *pixel);

/**
 * ws2812_show() - shift the transmit stream out to the chain
 * @ws: instance to refresh
 *
 * Context: task context. Blocks until the frame has been shifted out.
 *
 * Return: %0 on success, negative SPI_MASTER_ERR_* value on failure.
 */
int ws2812_show(struct ws2812 *ws);

/**
 * ws2812_ctrl() - set the first pixel and refresh the chain
 * @ws: instance to update
 * @r: red intensity, 0-255
 * @g: green intensity, 0-255
 * @b: blue intensity, 0-255
 *
 * Context: task context.
 *
 * Convenience wrapper for a single LED: ws2812_set_pixel() followed by
 * ws2812_show().
 *
 * Return: %0 on success, negative SPI_MASTER_ERR_* value on failure.
 */
int ws2812_ctrl(struct ws2812 *ws, uint8_t r, uint8_t g, uint8_t b);

#endif /* WS2812_H */
