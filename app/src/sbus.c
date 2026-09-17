/* SPDX-License-Identifier: MIT */

#include "app/inc/sbus.h"

#include <string.h>

bool sbus_parse(const uint8_t *data, uint16_t len)
{
	if (len != SBUS_FRAME_SIZE || data[0] != 0x0F ||
	    data[SBUS_FRAME_SIZE - 1] != 0x00) {
		return false;
	}

	return true;
}

void sbus_init(struct sbus *s, struct uart_dev *uart)
{
	*s = (struct sbus){ .uart = uart };
}

int sbus_start(struct sbus *s)
{
	int rc = uart_recv_async(s->uart, s->rx_frame.raw,
				 sizeof(s->rx_frame.raw), sbus_uart_rx_cb, s);

	s->started = (rc == 0);

	return rc;
}

void sbus_uart_rx_cb(struct uart_dev *dev, size_t len, uint32_t errors,
		     void *ctx)
{
	struct sbus *s = ctx;

	(void)dev;

	/*
	 * The adapter reuses rx_frame for every frame, so the frame has to be
	 * copied out here. A frame that arrives before the previous one was
	 * consumed is dropped and counted, not written over a staged frame.
	 */
	if (s->rx_done) {
		s->rx_dropped++;
		return;
	}

	memcpy(s->rx_stage.raw, s->rx_frame.raw, sizeof(s->rx_stage.raw));

	s->rx_len = (uint16_t)len;
	s->rx_errors = errors;
	s->rx_done = true;
}

bool sbus_process(struct sbus *s)
{
	bool refreshed = false;

	if (!s->rx_done) {
		return false;
	}

	s->rx_done = false;

	if (s->rx_errors == 0 && s->rx_len == SBUS_FRAME_SIZE) {
		refreshed = sbus_parse_to_msg(s);

		if (refreshed) {
			s->new_frame_ready = true;
		}
	}

	return refreshed;
}

static inline float sbus_ch_to_float(uint16_t raw)
{
	if (raw <= SBUS_CHANNEL_MIN) {
		return -1.0f;
	}
	if (raw >= SBUS_CHANNEL_MAX) {
		return 1.0f;
	}
	if (raw < SBUS_CHANNEL_MID) {
		return -(float)(SBUS_CHANNEL_MID - raw) /
		       (float)(SBUS_CHANNEL_MID - SBUS_CHANNEL_MIN);
	}

	return (float)(raw - SBUS_CHANNEL_MID) /
	       (float)(SBUS_CHANNEL_MAX - SBUS_CHANNEL_MID);
}

static inline SBUS_SWITCH_POS sbus_ch_to_switch(uint16_t raw)
{
	/*
	 * Three-position switch: below 682 is TOP, 682 to 1365 is MID,
	 * above that is BUT.
	 */
	if (raw < 682) {
		return TOP;
	}
	if (raw > 1365) {
		return BUT;
	}
	return MID;
}

bool sbus_parse_to_msg(struct sbus *s)
{
	if (!sbus_parse(s->rx_stage.raw, SBUS_FRAME_SIZE)) {
		return false;
	}

	/* Sticks */
	s->msg.ail = sbus_ch_to_float(sbus_get_one_channel(s, AIL));
	s->msg.ele = sbus_ch_to_float(sbus_get_one_channel(s, ELE));
	s->msg.thr = sbus_ch_to_float(sbus_get_one_channel(s, THR));
	s->msg.rud = sbus_ch_to_float(sbus_get_one_channel(s, RUD));

	/* Knobs */
	s->msg.s1 = sbus_ch_to_float(sbus_get_one_channel(s, S1));
	s->msg.s2 = sbus_ch_to_float(sbus_get_one_channel(s, S2));
	s->msg.s3 = sbus_ch_to_float(sbus_get_one_channel(s, S3));
	s->msg.s4 = sbus_ch_to_float(sbus_get_one_channel(s, S4));

	/* Switches */
	s->msg.sa = sbus_ch_to_switch(sbus_get_one_channel(s, SA));
	s->msg.sb = sbus_ch_to_switch(sbus_get_one_channel(s, SB));
	s->msg.sc = sbus_ch_to_switch(sbus_get_one_channel(s, SC));
	s->msg.sd = sbus_ch_to_switch(sbus_get_one_channel(s, SD));
	s->msg.se = sbus_ch_to_switch(sbus_get_one_channel(s, SE));
	s->msg.sf = sbus_ch_to_switch(sbus_get_one_channel(s, SF));
	s->msg.sg = sbus_ch_to_switch(sbus_get_one_channel(s, SG));
	s->msg.sh = sbus_ch_to_switch(sbus_get_one_channel(s, SH));

	return true;
}
