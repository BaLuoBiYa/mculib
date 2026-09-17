/* SPDX-License-Identifier: MIT */
/*
 * GPIO subsystem contract.
 *
 * The interrupt trigger mode is fixed by the adapter during board
 * initialization. The adapter serializes concurrent calls on the same device.
 */

#ifndef SUBSYSTEM_GPIO_H
#define SUBSYSTEM_GPIO_H

#include <stdint.h>

/**
 * struct gpio_dev - GPIO pin instance
 * @name: debug name, not interpreted by the subsystem layer
 * @priv: adapter-private data, not interpreted by the subsystem layer
 *
 * One instance represents one bound GPIO pin.
 */
struct gpio_dev {
	const char *name;
	void *priv;
};

/**
 * typedef gpio_irq_cb_t - GPIO interrupt callback
 * @dev: GPIO device that triggered the interrupt
 * @ctx: user context passed to gpio_irq_enable()
 *
 * Context: interrupt context.
 *
 * Invoked for every interrupt until gpio_irq_disable() clears the
 * subscription. @ctx must stay valid until that disable returns.
 *
 * Must not block, sleep, wait, allocate, or call any subsystem API; execution
 * time must be bounded. May run concurrently with task context on the same
 * device.
 */
typedef void (*gpio_irq_cb_t)(struct gpio_dev *dev, void *ctx);

/**
 * gpio_set() - drive the pin high
 * @dev: GPIO device
 *
 * Context: task context.
 *
 * Return: %0 on success, negative error code on failure.
 */
int gpio_set(struct gpio_dev *dev);

/**
 * gpio_reset() - drive the pin low
 * @dev: GPIO device
 *
 * Context: task context.
 *
 * Return: %0 on success, negative error code on failure.
 */
int gpio_reset(struct gpio_dev *dev);

/**
 * gpio_toggle() - invert the pin level
 * @dev: GPIO device
 *
 * Context: task context.
 *
 * Return: %0 on success, negative error code on failure.
 */
int gpio_toggle(struct gpio_dev *dev);

/**
 * gpio_read() - read the pin level
 * @dev: GPIO device
 * @value: receives %0 for low, non-zero for high
 *
 * Context: task context.
 *
 * Return: %0 on success, negative error code on failure.
 */
int gpio_read(struct gpio_dev *dev, uint8_t *value);

/**
 * gpio_irq_enable() - enable the pin interrupt and register the callback
 * @dev: GPIO device
 * @cb: interrupt callback, must not be NULL
 * @ctx: user context passed to @cb
 *
 * Context: task context. Registration is not atomic with respect to a pending
 * interrupt.
 *
 * Registers @cb once; it stays registered until gpio_irq_disable(). Enabling
 * an already enabled interrupt is a contract breach, and gpio_irq_disable()
 * followed by a new gpio_irq_enable() is the only way to replace @cb or @ctx.
 *
 * Return: %0 on success, negative error code on failure.
 */
int gpio_irq_enable(struct gpio_dev *dev, gpio_irq_cb_t cb, void *ctx);

/**
 * gpio_irq_disable() - disable the pin interrupt and clear the callback
 * @dev: GPIO device
 *
 * Context: task context.
 *
 * After it returns no further callback will be invoked for this device and
 * @ctx may be released. Disabling an already disabled interrupt returns the
 * same value.
 *
 * Return: %0 on success, negative error code on failure.
 */
int gpio_irq_disable(struct gpio_dev *dev);

#endif /* SUBSYSTEM_GPIO_H */