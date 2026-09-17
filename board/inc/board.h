/* SPDX-License-Identifier: MIT */
#ifndef BOARD_BOARD_H
#define BOARD_BOARD_H

#include "subsystems/subsystems_uart.h"

/*
 * UART instance exported by the board layer. The board binds a backend
 * resource behind it; the application only sees the subsystem contract.
 */
extern struct uart_dev *const uart_console;

/**
 * board_init() - bind board resources to the exported device instances
 *
 * Attributes that cannot change at runtime are fixed here: UART line
 * parameters, GPIO interrupt trigger mode, and similar one-shot properties.
 *
 * Context: task context, called by the application before it uses any
 * exported instance.
 */
void board_init(void);

#endif /* BOARD_BOARD_H */