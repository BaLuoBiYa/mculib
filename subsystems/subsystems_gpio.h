/* SPDX-License-Identifier: MIT */
/*
 * GPIO subsystem contract.
 *
 * Provides a backend-independent GPIO API for applications and board code.
 * The subsystem layer declares only the contract; all hardware access is
 * performed by the adapter layer.
 *
 * Device model: one gpio_dev instance represents one bound GPIO pin.
 * Thread safety: the adapter serializes concurrent calls on the same
 * gpio_dev.
 * Return values: %0 on success, negative error code on failure.
 *
 * The interrupt trigger mode is fixed by the adapter during board
 * initialization and cannot be changed at runtime.
 */

#ifndef SUBSYSTEM_GPIO_H
#define SUBSYSTEM_GPIO_H

#include <stdint.h>

/**
 * struct gpio_dev - GPIO pin instance
 * @name: debug name, not interpreted by the subsystem layer
 * @priv: adapter-private data, not interpreted by the subsystem layer
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
 * Context: interrupt context (executed directly inside the ISR).
 *
 * The callback must not block, sleep, wait, allocate memory, or call any
 * subsystem API. Execution time must be bounded.
 *
 * The callback may run concurrently with task-context operations on the same
 * device. The adapter must guarantee API atomicity, or the callback must be
 * restricted to setting a flag without touching the device.
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
 * gpio_irq_enable() - enable the pin interrupt
 * @dev: GPIO device
 * @cb: interrupt callback, see typedef gpio_irq_cb_t for its restrictions
 * @ctx: user context passed to @cb
 *
 * Context: task context. Callback registration is not atomic with respect to
 * a pending interrupt.
 *
 * Return: %0 on success, negative error code on failure.
 */
int gpio_irq_enable(struct gpio_dev *dev, gpio_irq_cb_t cb, void *ctx);

/**
 * gpio_irq_disable() - disable the pin interrupt
 * @dev: GPIO device
 *
 * Context: task context.
 *
 * Return: %0 on success, negative error code on failure.
 */
int gpio_irq_disable(struct gpio_dev *dev);

#endif /* SUBSYSTEM_GPIO_H */