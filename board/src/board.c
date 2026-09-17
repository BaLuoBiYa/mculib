/* SPDX-License-Identifier: MIT */

#include "adapter/mock/inc/uart_mock.h"
#include "board/inc/board.h"
#include <stdio.h>

/* Console UART instance; the board owns its static lifetime. */
static struct uart_dev uart0 = {
	.name = "console",
	.priv = 0,
};

/* Line parameters are board-level constants, fixed at bind time. */
static const struct uart_config console_cfg = {
	.baudrate = 115200,
	.databits = 8,
	.stopbits = 1,
	.parity = UART_PARITY_NONE,
};

struct uart_dev *const uart_console = &uart0;

void board_init(void)
{
	uart_host_mock_bind(&uart0, stdout, &console_cfg);
}