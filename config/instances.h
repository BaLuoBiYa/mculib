/* SPDX-License-Identifier: MIT */
#ifndef MCULIB_INSTANCES_H
#define MCULIB_INSTANCES_H

#include "subsystems/subsystems_uart.h"
#include "subsystems/subsystems_spi_master.h"
#include "subsystems/subsystems_i2c_master.h"
#include "subsystems/subsystems_can.h"
#include "subsystems/subsystems_timer.h"
#include "subsystems/subsystems_pwm.h"
#include "subsystems/subsystems_gpio.h"

/*
 * Peripheral instance limits.
 *
 * The maximum number of instances of each peripheral type. These values size
 * the static pools of adapter-private data, so they must cover every instance
 * the MCU provides, not only the ones the application happens to use.
 */

#define UART_MAX_INSTANCES 2
#define SPI_MAX_INSTANCES 3
#define I2C_MAX_INSTANCES 2
#define CAN_MAX_INSTANCES 2
#define TIMER_MAX_INSTANCES 4
#define PWM_MAX_INSTANCES 8

#endif /* MCULIB_INSTANCES_H */