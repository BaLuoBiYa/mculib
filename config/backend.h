/* SPDX-License-Identifier: MIT */
#ifndef CONFIG_BACKEND_H
#define CONFIG_BACKEND_H

/*
 * Backend selection.
 *
 * Select exactly one backend per subsystem or hardware-backed tool and keep
 * the others commented out. The selected backend must match the one hard-wired
 * in the board file, otherwise the mismatch shows up as undefined symbols at
 * link time.
 */

/* UART */

/* #define UART_BACKEND_STM32_HAL */
#define UART_BACKEND_HOST_MOCK

/* SPI */

/* #define SPI_BACKEND_STM32_HAL */
#define SPI_BACKEND_HOST_MOCK

/* CAN */

/* #define CAN_BACKEND_STM32_HAL */
#define CAN_BACKEND_HOST_MOCK

/* TIME (hardware-backed tools) */

/* #define TIME_BACKEND_STM32_HAL */
#define TIME_BACKEND_HOST_MOCK

#if defined(UART_BACKEND_HOST_MOCK)
#include <adapter/mock/inc/uart_mock.h>
#endif
#if defined(SPI_BACKEND_HOST_MOCK)
#include <adapter/mock/inc/spi_mock.h>
#endif
#if defined(CAN_BACKEND_HOST_MOCK)
#include <adapter/mock/inc/can_mock.h>
#endif
// #if defined(TIME_BACKEND_HOST_MOCK)
// #include <adapter/mock/inc/time_mock.h>
// #endif

#endif /* CONFIG_BACKEND_H */