/* SPDX-License-Identifier: MIT */
#ifndef CONFIG_BACKEND_H
#define CONFIG_BACKEND_H

/*
 * Backend selection.
 *
 * Select exactly one backend per subsystem and keep the others commented out.
 * The selected backend must match the one hard-wired in the board file,
 * otherwise the mismatch shows up as undefined symbols at link time.
 */

/* UART */

/* #define UART_BACKEND_STM32_HAL */
#define UART_BACKEND_HOST_MOCK

/* SPI */

/* #define SPI_BACKEND_STM32_HAL */
#define SPI_BACKEND_HOST_MOCK

#endif /* CONFIG_BACKEND_H */