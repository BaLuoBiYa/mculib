/* SPDX-License-Identifier: MIT */
/*
 * timer subsystem contract.
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
 * One instance represents one bound timer, a periodic or one-shot event
 * source.
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
 * Fixed by board code at binding time; not reconfigurable at runtime.
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
 * Context: interrupt context.
 *
 * Invoked on every period in periodic mode, once per run in one-shot mode.
 * @ctx must stay valid until timer_abrot() returns.
 *
 * Must not block, sleep, wait, allocate, or call any subsystem API; execution
 * time must be bounded. May run concurrently with task context on the same
 * device.
 */
typedef void (*timer_cb_t)(struct timer_dev *dev, void *ctx);

/**
 * timer_start() - start the timer and register the expiration callback
 * @dev: timer device
 * @cb: expiration callback, must not be NULL
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * Registers @cb until timer_abrot() clears it. In one-shot mode the counter
 * stops after the callback; the registration does not.
 *
 * Calling this while the timer is running is a contract breach, and so is
 * replacing @cb or @ctx while it runs. With the counter stopped - after
 * timer_abrot() or after a one-shot completion - this call re-registers and is
 * how the timer is armed again.
 *
 * Return: TIMER_OK on success, negative error code on failure.
 */
int timer_start(struct timer_dev *dev, timer_cb_t cb, void *ctx);

/**
 * timer_abrot() - stop the timer and clear the registration
 * @dev: timer device
 *
 * Context: task context.
 *
 * After it returns no further callback will be invoked for this device and
 * @ctx may be released. Stopping an already stopped timer returns the same
 * value.
 *
 * Return: TIMER_OK on success, negative error code on failure.
 */
int timer_abrot(struct timer_dev *dev);

/**
 * timer_get_count() - read the current counter value
 * @dev: timer device
 * @count: output for the current counter value, in the timer's native counting
 *         unit
 *
 * Context: task or interrupt context.
 *
 * Return: TIMER_OK on success, negative error code on failure.
 */
int timer_get_count(struct timer_dev *dev, uint32_t *count);

#endif /* SUBSYSTEM_TIMER_H */