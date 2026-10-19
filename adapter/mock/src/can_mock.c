/* SPDX-License-Identifier: MIT */

#include "config/backend.h"
#include "config/instances.h"

#if defined(CAN_BACKEND_HOST_MOCK)

#include "adapter/mock/inc/can_mock.h"
#include "common/byteorder.h"

/* One frame record on the stream, see can_host_mock_bind(). */
#define CAN_MOCK_RECORD_LEN 13

struct can_host_priv {
	FILE *stream;
	/* Receive subscription, registered once and cleared by can_abort(). */
	int rx_active;
};

static struct can_host_priv priv_pool[CAN_MAX_INSTANCES];
static int priv_used;

void can_host_mock_bind(struct can_dev *dev, FILE *stream,
			const struct can_config *cfg)
{
	/* No bit timing and no acceptance filter on a host stream. */
	(void)cfg;

	if (priv_used >= CAN_MAX_INSTANCES) {
		return;
	}

	priv_pool[priv_used].stream = stream;
	dev->priv = &priv_pool[priv_used];
	priv_used++;
}

static void can_mock_encode(uint8_t rec[CAN_MOCK_RECORD_LEN],
			    const struct can_msg *msg)
{
	int i;

	put_u32le(rec, msg->id);
	rec[4] = msg->dlc;
	for (i = 0; i < 8; i++) {
		rec[5 + i] = msg->data[i];
	}
}

static void can_mock_decode(const uint8_t rec[CAN_MOCK_RECORD_LEN],
			    struct can_msg *msg)
{
	int i;

	msg->id = get_u32le(rec);
	msg->dlc = rec[4];
	for (i = 0; i < 8; i++) {
		msg->data[i] = rec[5 + i];
	}
}

int can_send(struct can_dev *dev, const struct can_msg *msg, uint32_t timeout)
{
	struct can_host_priv *p = dev->priv;
	uint8_t rec[CAN_MOCK_RECORD_LEN];

	/* A stream transfer completes immediately, the timeout is ignored. */
	(void)timeout;

	can_mock_encode(rec, msg);

	if (fwrite(rec, 1, sizeof(rec), p->stream) != sizeof(rec)) {
		/* The record did not reach the stream. */
		return CAN_ERR_BUSY;
	}

	fflush(p->stream);

	return CAN_OK;
}

int can_send_async(struct can_dev *dev, const struct can_msg *msg,
		   can_tx_done_cb_t cb, void *ctx)
{
	int rc = can_send(dev, msg, 0);

	/* A stream transfer completes in the caller context. */
	cb(dev, rc, 0, ctx);

	return CAN_OK;
}

int can_recv_async(struct can_dev *dev, can_rx_cb_t cb, void *ctx)
{
	struct can_host_priv *p = dev->priv;
	uint8_t rec[CAN_MOCK_RECORD_LEN];

	/* One subscription per device, see can_recv_async(). */
	if (p->rx_active) {
		return CAN_ERR_BUSY;
	}

	p->rx_active = 1;

	/*
	 * A stdio stream is finite, so one call replays its records and
	 * returns, with the subscription left registered. A real backend keeps
	 * receiving until can_abort(), so its callback runs in the ISR. Host
	 * streams carry no bus errors, every frame is reported clean.
	 */
	while (fread(rec, 1, sizeof(rec), p->stream) == sizeof(rec)) {
		struct can_msg msg;

		can_mock_decode(rec, &msg);
		cb(dev, &msg, 0, ctx);
	}

	return CAN_OK;
}

int can_set_filter(struct can_dev *dev, const struct can_filter *filters,
		   size_t num_filters)
{
	/* The host mock has no acceptance filter, every frame is delivered. */
	(void)dev;
	(void)filters;
	(void)num_filters;

	return CAN_OK;
}

int can_abort(struct can_dev *dev)
{
	struct can_host_priv *p = dev->priv;

	/* Clear the subscription: no later frame may reach the callback. */
	p->rx_active = 0;

	return CAN_OK;
}

#endif /* CAN_BACKEND_HOST_MOCK */
