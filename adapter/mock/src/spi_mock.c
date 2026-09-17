/* SPDX-License-Identifier: MIT */

#include "config/backend.h"
#include "config/instances.h"

#if defined(SPI_BACKEND_HOST_MOCK)

#include "adapter/mock/inc/spi_mock.h"

struct spi_host_priv {
	FILE *stream;
	struct spi_master_master_config cfg;
};

/*
 * Static pool of adapter-private data, one slot per binding. Slots are never
 * reclaimed: bound devices are expected to live for the whole program.
 */
static struct spi_host_priv priv_pool[SPI_MAX_INSTANCES];
static int priv_used;

void spi_host_mock_bind(struct spi_master_dev *dev, FILE *stream,
			const struct spi_master_master_config *cfg)
{
	if (priv_used >= SPI_MAX_INSTANCES) {
		return;
	}

	priv_pool[priv_used].stream = stream;
	priv_pool[priv_used].cfg = *cfg;
	dev->priv = &priv_pool[priv_used];
	priv_used++;
}

/*
 * A stdio stream can only observe the transmitted bytes, so it is used as the
 * MOSI log. It cannot produce receive data, so a transfer that asks for one is
 * rejected instead of silently returning uninitialized bytes.
 */
int spi_master_transfer(struct spi_master_dev *dev, const void *tx_buf,
			void *rx_buf, size_t len, uint32_t timeout)
{
	struct spi_host_priv *p = dev->priv;
	size_t n;

	/* A stream transfer completes immediately, the timeout is ignored. */
	(void)timeout;

	if (rx_buf != NULL) {
		return SPI_MASTER_ERR_UNSUPPORTED;
	}

	if (p->cfg.cs_ctrl != NULL) {
		p->cfg.cs_ctrl(p->cfg.cs_ctx, 1);
	}

	n = fwrite(tx_buf, 1, len, p->stream);
	fflush(p->stream);

	if (p->cfg.cs_ctrl != NULL) {
		p->cfg.cs_ctrl(p->cfg.cs_ctx, 0);
	}

	return (int)n;
}

int spi_master_transfer_async(struct spi_master_dev *dev, const void *tx_buf,
			      void *rx_buf, size_t len, spi_master_done_cb_t cb,
			      void *ctx)
{
	int rc = spi_master_transfer(dev, tx_buf, rx_buf, len, 0);

	if (rc < 0) {
		return rc;
	}

	/*
	 * The stream completes immediately, so the callback runs in the caller
	 * context. A real backend reports completion from the ISR instead.
	 */
	cb(dev, len, 0, ctx);

	return SPI_MASTER_OK;
}

int spi_master_abort(struct spi_master_dev *dev)
{
	(void)dev;

	return SPI_MASTER_OK;
}

#endif /* SPI_BACKEND_HOST_MOCK */
