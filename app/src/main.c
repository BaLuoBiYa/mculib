/* SPDX-License-Identifier: MIT */

#include "config/backend.h"
#include "config/instances.h"

static struct uart_dev uart0 = {
	.name = "console",
	.priv = 0,
};

static const struct uart_config console_cfg = {
	.baudrate = 115200,
	.databits = 8,
	.stopbits = 1,
	.parity = UART_PARITY_NONE,
};

struct uart_dev *uart_console;

int main(void)
{
	static const char msg[] = "Hello\n";

	uart_host_mock_bind(&uart0, stdout, &console_cfg);

	for (;;) {
		uart_send(uart_console, msg, sizeof(msg) - 1, 1000);
	}
}