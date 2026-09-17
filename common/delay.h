/* SPDX-License-Identifier: MIT */
#ifndef MCULIB_COMMON_DELAY_H
#define MCULIB_COMMON_DELAY_H

#include <stdint.h>

/*
 * Delay helpers.
 *
 * Thread safety: yes, reentrant. Neither function may be called from a
 * critical section; the context constraints are documented per function.
 */

/**
 * delay_us() - delay for a number of microseconds
 * @us: delay in microseconds
 *
 * Context: task or interrupt context. Interrupt context must keep the delay
 * very short.
 *
 * Return: %0 on success, negative error code on failure.
 */
int delay_us(uint32_t us);

/**
 * delay_ms() - delay for a number of milliseconds
 * @ms: delay in milliseconds
 *
 * Context: task context only. May block, so it must not be called from
 * interrupt context.
 *
 * Return: %0 on success, negative error code on failure.
 */
int delay_ms(uint32_t ms);

#endif /* MCULIB_COMMON_DELAY_H */