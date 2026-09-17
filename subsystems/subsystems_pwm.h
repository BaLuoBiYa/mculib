/* SPDX-License-Identifier: MIT */
/*
 * pwm subsystem contract.
 *
 * Provides a backend-independent PWM API for applications and board code.
 * The subsystem layer declares only the contract; all hardware access is
 * performed by the adapter layer.
 *
 * A PWM instance is a single output channel. Frequency, period, polarity, and
 * idle level are fixed by board code at binding time and are not changed at
 * runtime. Only the duty cycle is changed at runtime.
 *
 * Multiple channels driven by the same timer are represented by multiple
 * pwm_dev instances. The adapter serializes access at the timer level.
 */

#ifndef SUBSYSTEM_PWM_H
#define SUBSYSTEM_PWM_H

#include <stdint.h>

/*
 * Error codes for functions returning int.
 */
#define PWM_OK 0
#define PWM_ERR (-1)

/**
 * struct pwm_dev - PWM device instance
 * @name: debug name, not interpreted by the subsystem layer
 * @priv: adapter-private data, not interpreted by the subsystem layer
 *
 * One instance represents one bound PWM output channel. Instances are created
 * by board code and exported to applications.
 */
struct pwm_dev {
	const char *name;
	void *priv;
};

/**
 * struct pwm_config - PWM configuration
 * @frequency_hz: PWM frequency in Hz
 * @polarity: output polarity
 * @idle_duty: duty value driven when the channel is stopped
 *
 * Describes the waveform parameters of a PWM instance. The board provides
 * one of these when it binds the device. Runtime reconfiguration is not
 * provided by the subsystem layer.
 */
typedef enum {
	PWM_POLARITY_NORMAL = 0,
	PWM_POLARITY_INVERTED,
} pwm_polarity_t;

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
 * Enables the PWM output. The output starts from the current duty cycle,
 * which is zero until pwm_set_duty() is called.
 *
 * Return: PWM_OK on success, negative error code on failure.
 */
int pwm_start(struct pwm_dev *dev);

/**
 * pwm_stop() - stop PWM output
 * @dev: PWM device
 *
 * Context: task context.
 *
 * Disables the PWM output. The channel is driven to its configured idle
 * level, determined at binding time.
 *
 * Return: PWM_OK on success, negative error code on failure.
 */
int pwm_stop(struct pwm_dev *dev);

/**
 * pwm_set_duty() - set the duty cycle
 * @dev: PWM device
 * @duty: duty cycle in native counter units
 *
 * Context: task or interrupt context.
 *
 * Sets the duty cycle. @duty is expressed in the same counting unit as the
 * timer's period, not as a percentage. The valid range is 0 to the period
 * value fixed at binding time, inclusive. Passing a value outside this range
 * is a contract breach.
 *
 * This function is intended for hot paths. The adapter writes the compare
 * register directly, without locks unless the backend requires them.
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