/* SPDX-License-Identifier: MIT */

#include "subsystems/subsystems_uart.h"
#include "board/inc/board.h"

int main(void)
{
	static const char msg[] = "Hello\n";

	board_init();

	for (;;) {
		uart_send(uart_console, msg, sizeof(msg) - 1, 1000);
	}
}