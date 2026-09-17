/* SPDX-License-Identifier: MIT */

#include "config/backend.h"
#include "config/instances.h"

#if defined(UART_BACKEND_HOST_MOCK)

#include "adapter/mock/inc/uart_mock.h"

struct uart_host_priv {
	FILE *stream;
	/* Receive subscription, registered once and cleared by uart_abort(). */
	void *rx_buf;
	size_t rx_max_len;
	uart_rx_cb_t rx_cb;
	void *rx_ctx;
	int rx_active;
};

/*
 * Static pool of adapter-private data, one slot per binding. Slots are never
 * reclaimed: bound devices are expected to live for the whole program.
 */
static struct uart_host_priv priv_pool[UART_MAX_INSTANCES];
static int priv_used;

void uart_host_mock_bind(struct uart_dev *dev, FILE *stream,
			 const struct uart_config *cfg)
{
	/* A stdio stream has no registers to program. */
	(void)cfg;

	if (priv_used >= UART_MAX_INSTANCES) {
		return;
	}

	priv_pool[priv_used].stream = stream;
	dev->priv = &priv_pool[priv_used];
	priv_used++;
}

int uart_send(struct uart_dev *dev, const void *buf, size_t len,
	      uint32_t timeout)
{
	struct uart_host_priv *p = dev->priv;
	size_t n;

	(void)timeout;

	n = fwrite(buf, 1, len, p->stream);
	fflush(p->stream);
	return (int)n;
}

int uart_recv(struct uart_dev *dev, void *buf, size_t len, uint32_t timeout)
{
	(void)dev;
	(void)buf;
	(void)len;
	(void)timeout;

	return UART_ERR_IO;
}

int uart_recv_async(struct uart_dev *dev, void *buf, size_t max_len,
		    uart_rx_cb_t cb, void *ctx)
{
	struct uart_host_priv *p = dev->priv;

	/*
	 * The contract allows one subscription per device: registering again
	 * while one is active is a caller breach, and rejecting it here keeps
	 * the mistake visible instead of silently replacing the subscription.
	 */
	if (p->rx_active) {
		return UART_ERR_BUSY;
	}

	p->rx_buf = buf;
	p->rx_max_len = max_len;
	p->rx_cb = cb;
	p->rx_ctx = ctx;
	p->rx_active = 1;

	/*
	 * A stdio stream is finite, so one call replays its frames and returns,
	 * with the subscription left registered. A real backend returns
	 * immediately and fires the callback from the idle interrupt for every
	 * later frame. Host streams carry no line errors, every frame is
	 * reported clean.
	 */
	for (;;) {
		size_t n = fread(p->rx_buf, 1, p->rx_max_len, p->stream);

		if (n == 0) {
			break;
		}

		p->rx_cb(dev, n, 0, p->rx_ctx);
	}

	return UART_OK;
}

int uart_abort(struct uart_dev *dev)
{
	struct uart_host_priv *p = dev->priv;

	/* Clear the subscription: no later interrupt may reach rx_cb. */
	p->rx_buf = NULL;
	p->rx_max_len = 0;
	p->rx_cb = NULL;
	p->rx_ctx = NULL;
	p->rx_active = 0;

	return UART_OK;
}

#endif /* UART_BACKEND_HOST_MOCK */