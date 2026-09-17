/* SPDX-License-Identifier: MIT */

#include "config/backend.h"
#include "config/instances.h"

#if defined(UART_BACKEND_HOST_MOCK)

#include "adapter/mock/uart_mock.h"

struct uart_host_priv {
	FILE *stream;
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

#endif /* UART_BACKEND_HOST_MOCK */