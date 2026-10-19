/* SPDX-License-Identifier: MIT */
/*
 * pwm subsystem contract.
 *
 * Multiple channels driven by the same timer are separate instances; the
 * adapter serializes access at the timer level.
 */

#ifndef SUBSYSTEM_PWM_H
#define SUBSYSTEM_PWM_H

#include <stdint.h>

/*
 * Return values: PWM_OK on success, negative error code on failure.
 */
#define PWM_OK 0
#define PWM_ERR (-1)

/**
 * struct pwm_dev - PWM device instance
 * @name: debug name, not interpreted by the subsystem layer
 * @priv: adapter-private data, not interpreted by the subsystem layer
 *
 * One instance represents one bound PWM output channel.
 */
struct pwm_dev {
	const char *name;
	void *priv;
};

/**
 * enum pwm_polarity_t - PWM output polarity
 */
typedef enum {
	PWM_POLARITY_NORMAL = 0,
	PWM_POLARITY_INVERTED,
} pwm_polarity_t;

/**
 * struct pwm_config - PWM configuration
 * @frequency_hz: PWM frequency in Hz
 * @polarity: output polarity, see enum pwm_polarity_t
 * @idle_duty: duty value driven when the channel is stopped
 *
 * Fixed by board code at binding time; the duty cycle is the only runtime
 * setting.
 */
struct pwm_config {
	uint32_t frequency_hz;
	pwm_polarity_t polarity;
	uint32_t idle_duty;
};

/**
 * pwm_start() - start PWM output
 * @dev: PWM device
 *
 * Context: task context.
 *
 * Starts the output from the current duty cycle, which is zero until
 * pwm_set_duty() is called.
 *
 * Return: PWM_OK on success, negative error code on failure.
 */
int pwm_start(struct pwm_dev *dev);

/**
 * pwm_abrot() - stop PWM output
 * @dev: PWM device
 *
 * Context: task context.
 *
 * Stops the output; the channel is driven to its idle level, fixed at binding
 * time.
 *
 * Return: PWM_OK on success, negative error code on failure.
 */
int pwm_abrot(struct pwm_dev *dev);

/**
 * pwm_set_duty() - set the duty cycle
 * @dev: PWM device
 * @duty: duty cycle in native counter units, not a percentage
 *
 * Context: task or interrupt context.
 *
 * The valid range is 0 to the period value fixed at binding time, inclusive;
 * a value outside it is a contract breach.
 *
 * Return: PWM_OK on success, negative error code on failure.
 */
int pwm_set_duty(struct pwm_dev *dev, uint32_t duty);

/**
 * pwm_get_duty() - read the current duty cycle
 * @dev: PWM device
 * @duty: output for the current duty cycle in native counter units
 *
 * Context: task or interrupt context.
 *
 * Return: PWM_OK on success, negative error code on failure.
 */
int pwm_get_duty(struct pwm_dev *dev, uint32_t *duty);

/**
 * pwm_get_max_duty() - read max duty cycle
 * @dev: PWM device
 * @duty: output for the max duty cycle in native counter units
 *
 * Context: task or interrupt context.
 *
 * Return: PWM_OK on success, negative error code on failure.
 */
int pwm_get_max_duty(struct pwm_dev *dev, uint32_t *max_duty);

#endif /* SUBSYSTEM_PWM_H */