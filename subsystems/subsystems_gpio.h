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
 * gpio_abrot() - abort all callbacks
 * @dev: GPIO device
 *
 * Context: task context.
 *
 * Return: %0 on success, negative error code on failure.
 */
int gpio_abrot(struct gpio_dev *dev);

#endif /* SUBSYSTEM_GPIO_H */