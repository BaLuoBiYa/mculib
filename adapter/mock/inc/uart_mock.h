/* SPDX-License-Identifier: MIT */
#ifndef ADAPTER_UART_HOST_MOCK_H
#define ADAPTER_UART_HOST_MOCK_H

#include "subsystems/subsystems_uart.h"
#include <stdio.h>

/**
 * uart_host_mock_bind() - bind a stdio stream to a UART device instance
 * @dev: UART device to bind
 * @stream: stdio stream used as the UART line
 * @cfg: line parameters, fixed for the lifetime of the binding
 *
 * Context: task context, typically during board initialization. Must be
 * called before the device is used. The adapter keeps a static slot per
 * binding; bindings beyond the available slots are silently ignored.
 */
void uart_host_mock_bind(struct uart_dev *dev, FILE *stream,
			 const struct uart_config *cfg);

#endif /* ADAPTER_UART_HOST_MOCK_H */