/* SPDX-License-Identifier: MIT */
#ifndef ADAPTER_SPI_HOST_MOCK_H
#define ADAPTER_SPI_HOST_MOCK_H

#include "subsystems/subsystems_spi_master.h"
#include <stdio.h>

/**
 * spi_host_mock_bind() - bind a stdio stream to an SPI master device instance
 * @dev: SPI master device to bind
 * @stream: stdio stream that receives the transmitted bytes
 * @cfg: line parameters, copied into the binding
 *
 * Context: task context, typically during board initialization. Must be
 * called before the device is used. The adapter keeps a static slot per
 * binding; bindings beyond the available slots are silently ignored.
 *
 * The stream logs the MOSI line: every transmitted byte is appended to it. A
 * stdio stream cannot act as an SPI slave, so transfers that ask for received
 * data are rejected with SPI_MASTER_ERR_UNSUPPORTED. When @cfg provides a chip
 * select callback, the adapter invokes it around the transfer as a real
 * backend would.
 */
void spi_host_mock_bind(struct spi_master_dev *dev, FILE *stream,
			const struct spi_master_master_config *cfg);

#endif /* ADAPTER_SPI_HOST_MOCK_H */
