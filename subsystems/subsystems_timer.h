/* SPDX-License-Identifier: MIT */
/*
 * timer subsystem contract.
 *
 * Provides a backend-independent timer API for applications and board code.
 * The subsystem layer declares only the contract; all hardware access is
 * performed by the adapter layer.
 *
 * A timer instance is a periodic or one-shot event source. Period, prescaler,
 * and one-shot/periodic mode are fixed by board code at binding time and are
 * not changed at runtime.
 */

#ifndef SUBSYSTEM_TIMER_H
#define SUBSYSTEM_TIMER_H

#include <stdint.h>

/*
 * Error codes for functions returning int.
 */
#define TIMER_OK 0
#define TIMER_ERR (-1)

/**
 * struct timer_dev - timer device instance
 * @name: debug name, not interpreted by the subsystem layer
 * @priv: adapter-private data, not interpreted by the subsystem layer
 *
 * One instance represents one bound timer. Instances are created by board
 * code and exported to applications.
 */
struct timer_dev {
	const char *name;
	void *priv;
};

/**
 * struct timer_config - timer configuration
 * @period_us: timer period in microseconds
 * @one_shot: non-zero for one-shot mode, zero for periodic mode
 *
 * Describes the timing parameters of a timer instance. The board provides
 * one of these when it binds the device. Runtime reconfiguration is not
 * provided by the subsystem layer; it is done through board-level
 * re-binding.
 */
struct timer_config {
	uint32_t period_us;
	int one_shot;
};

/**
 * typedef timer_cb_t - timer expiration callback
 * @dev: timer device that expired
 * @ctx: user context passed to timer_start()
 *
 * Context: interrupt context (executed directly inside the ISR).
 *
 * The callback must not block, sleep, wait, allocate memory, or call any
 * subsystem API. Execution time must be bounded.
 *
 * The callback may run concurrently with task-context operations on the same
 * device. The adapter must guarantee API atomicity, or the callback must be
 * restricted to setting a flag without touching the device.
 */
typedef void (*timer_cb_t)(struct timer_dev *dev, void *ctx);

/**
 * timer_start() - start the timer and register the expiration callback
 * @dev: timer device
 * @cb: expiration callback
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * Starts the timer. In periodic mode, @cb is invoked on every period. In
 * one-shot mode, @cb is invoked once and the timer stops.
 *
 * The callback is registered by this call. Calling timer_start() on an
 * already running timer is a contract breach.
 *
 * Return: TIMER_OK on success, negative error code on failure.
 */
int timer_start(struct timer_dev *dev, timer_cb_t cb, void *ctx);

/**
 * timer_stop() - stop the timer and unregister the callback
 * @dev: timer device
 *
 * Context: task context.
 *
 * Stops the timer. After this function returns, no further callback will be
 * invoked for this device.
 *
 * Return: TIMER_OK on success, negative error code on failure.
 */
int timer_stop(struct timer_dev *dev);

/**
 * timer_get_count() - read the current counter value
 * @dev: timer device
 * @count: output for the current counter value
 *
 * Context: task or interrupt context.
 *
 * Reads the current counter value. The unit is the timer's native counting
 * unit, determined by the prescaler fixed at binding time. Useful for
 * measuring elapsed time within a period.
 *
 * Return: TIMER_OK on success, negative error code on failure.
 */
int timer_get_count(struct timer_dev *dev, uint32_t *count);

#endif /* SUBSYSTEM_TIMER_H */