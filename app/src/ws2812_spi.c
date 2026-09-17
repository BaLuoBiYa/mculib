/* SPDX-License-Identifier: MIT */

#include "app/inc/ws2812_spi.h"

/*
 * Encode one 8-bit color value into 8 SPI bytes.
 *
 * Bits are emitted MSB first, and each encoded pattern is shifted right by one
 * so that the top bit of every SPI byte is low. That guard bit keeps the line
 * low across the byte boundary and prevents a stray high pulse between two
 * encoded bits.
 */
static void ws2812_encode_color(uint8_t *dst, uint8_t value)
{
	int i;

	for (i = 0; i < 8; i++) {
		uint8_t pattern = WS2812_SPI_BIT_0;

		if ((value >> i) & 0x01) {
			pattern = WS2812_SPI_BIT_1;
		}

		dst[7 - i] = (uint8_t)(pattern >> 1);
	}
}

void ws2812_init(struct ws2812 *ws, struct spi_master_dev *spi)
{
	*ws = (struct ws2812){ .spi = spi };
}

void ws2812_set_pixel(struct ws2812 *ws, const struct ws2812_pixel *pixel)
{
	uint8_t *px;

	if (pixel->index >= WS2812_LED_NUM) {
		return;
	}

	px = ws->tx_buf + ((size_t)pixel->index * WS2812_SPI_BYTES_PER_LED);

	ws2812_encode_color(px, pixel->g);
	ws2812_encode_color(px + WS2812_SPI_BYTES_PER_COLOR, pixel->r);
	ws2812_encode_color(px + (2 * WS2812_SPI_BYTES_PER_COLOR), pixel->b);
}

int ws2812_show(struct ws2812 *ws)
{
	int rc = spi_master_transfer(ws->spi, ws->tx_buf, NULL,
				     sizeof(ws->tx_buf), WS2812_SPI_TIMEOUT_MS);

	return rc < 0 ? rc : 0;
}

int ws2812_ctrl(struct ws2812 *ws, uint8_t r, uint8_t g, uint8_t b)
{
	const struct ws2812_pixel pixel = {
		.index = 0,
		.r = r,
		.g = g,
		.b = b,
	};

	ws2812_set_pixel(ws, &pixel);

	return ws2812_show(ws);
}
