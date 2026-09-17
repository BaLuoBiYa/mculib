/* SPDX-License-Identifier: MIT */
#ifndef MCULIB_COMMON_TIME_H
#define MCULIB_COMMON_TIME_H

#include <stdint.h>

/*
 * System time.
 *
 * Monotonically increasing and unaffected by wall-clock adjustments. Used
 * for timeouts, elapsed time measurement, and log timestamps.
 *
 * Thread safety: yes, reentrant.
 *
 * Overflow: time_now_ms() returns a 32-bit millisecond counter that wraps
 * after about 49.7 days. Compare two values with (int32_t)(a - b) to get the
 * correct ordering across a wrap.
 */

/**
 * time_now_ms() - current monotonic time in milliseconds
 *
 * Context: task and interrupt context.
 *
 * Return: milliseconds since an arbitrary epoch; wraps about every 49.7
 * days.
 */
uint32_t time_now_ms(void);

/**
 * time_now_ticks() - current monotonic time in hardware ticks
 *
 * Context: task and interrupt context.
 *
 * Return: ticks since an arbitrary epoch. The tick source and its period are
 * defined by the backend.
 */
uint64_t time_now_ticks(void);

#endif /* MCULIB_COMMON_TIME_H */