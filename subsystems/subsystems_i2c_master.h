/* SPDX-License-Identifier: MIT */
/*
 * i2c_master subsystem contract.
 */

#ifndef SUBSYSTEM_I2C_MASTER_H
#define SUBSYSTEM_I2C_MASTER_H

#include <stddef.h>
#include <stdint.h>

/*
 * Return values: >= 0 is the number of messages transferred, or 0 for control
 * APIs; < 0 is one of the I2C_ERR_* values below. These are not bitmask flags.
 */
#define I2C_OK 0
#define I2C_ERR_PARAM (-1)
#define I2C_ERR_TIMEOUT (-2)
#define I2C_ERR_IO (-3)
#define I2C_ERR_BUSY (-4)
#define I2C_ERR_UNSUPPORTED (-5)
#define I2C_ERR_NACK (-6)
#define I2C_ERR_ARBITRATION_LOST (-7)

/*
 * Error bits for the @errors argument of i2c_master_done_cb_t; a transfer may
 * report several of them at once.
 *
 * I2C_ERR_ERR_NACK:             slave did not acknowledge
 * I2C_ERR_ERR_ARBITRATION_LOST: bus arbitration was lost
 * I2C_ERR_ERR_BUS:              bus error, typically a misplaced start or stop
 * I2C_ERR_ERR_OVERRUN:          receive data was lost before it could be
 *                               consumed
 * I2C_ERR_ERR_ABORTED:          transfer was terminated by hardware error or
 *                               by i2c_master_abort()
 */
#define I2C_ERR_ERR_NACK (1u << 0)
#define I2C_ERR_ERR_ARBITRATION_LOST (1u << 1)
#define I2C_ERR_ERR_BUS (1u << 2)
#define I2C_ERR_ERR_OVERRUN (1u << 3)
#define I2C_ERR_ERR_ABORTED (1u << 4)

/**
 * struct i2c_master_dev - I2C master device instance
 * @name: debug name, not interpreted by the subsystem layer
 * @priv: adapter-private data, not interpreted by the subsystem layer
 *
 * One instance represents one bound I2C slave device. Multiple instances may
 * share one physical bus; the adapter serializes access at the bus level.
 */
struct i2c_master_dev {
	const char *name;
	void *priv;
};

/**
 * struct i2c_msg - I2C message
 * @tx_buf: buffer to transmit, NULL for a read-only message
 * @rx_buf: buffer to receive into, NULL for a write-only message
 * @len: number of bytes
 *
 * Exactly one of @tx_buf and @rx_buf must be non-NULL; both set is invalid. A
 * message with both NULL and @len 0 is a zero-length probe of the device
 * address.
 *
 * Messages are sent in order, with a repeated start between them and a stop
 * after the last one.
 */
struct i2c_msg {
	const void *tx_buf;
	void *rx_buf;
	size_t len;
};

/**
 * struct i2c_master_config - I2C master configuration
 * @clock_hz: bus clock frequency in Hz
 * @address: slave address, right-aligned
 * @address_bits: address width, 7 or 10
 *
 * Fixed by board code at binding time; not reconfigurable at runtime.
 */
struct i2c_master_config {
	uint32_t clock_hz;
	uint16_t address;
	uint8_t address_bits;
};

/**
 * typedef i2c_master_done_cb_t - transfer completion callback
 * @dev: I2C master device that completed the transfer
 * @num_msgs: number of messages transferred successfully
 * @errors: error bitmask, 0 when clean, see I2C_ERR_ERR_*
 * @ctx: user context passed to i2c_master_transfer_async()
 *
 * Context: interrupt context.
 *
 * Invoked once per i2c_master_transfer_async() call, after which @msgs and its
 * buffers may be released. @num_msgs is smaller than the requested count when
 * the transfer was aborted in the middle, with @errors non-zero. @ctx must stay
 * valid until the callback or i2c_master_abort() returns.
 *
 * Must not block, sleep, wait, allocate, or call any subsystem API; execution
 * time must be bounded. May run concurrently with task context on the same
 * device.
 */
typedef void (*i2c_master_done_cb_t)(struct i2c_master_dev *dev,
				     size_t num_msgs, uint32_t errors,
				     void *ctx);

/**
 * i2c_master_transfer() - synchronously transfer messages
 * @dev: I2C master device
 * @msgs: array of messages to transfer
 * @num_msgs: number of messages in @msgs
 * @timeout: timeout in milliseconds, 0 for non-blocking
 *
 * Context: task context.
 *
 * The address and address width come from the device configuration. Concurrent
 * calls on the same device are serialized by the adapter or rejected with
 * I2C_ERR_BUSY.
 *
 * Return: number of messages transferred on success, negative I2C_ERR_*
 * value on failure.
 */
int i2c_master_transfer(struct i2c_master_dev *dev, const struct i2c_msg *msgs,
			size_t num_msgs, uint32_t timeout);

/**
 * i2c_master_transfer_async() - asynchronously transfer messages
 * @dev: I2C master device
 * @msgs: array of messages to transfer, must remain valid until @cb is
 *        invoked
 * @num_msgs: number of messages in @msgs
 * @cb: completion callback, must not be NULL, invoked exactly once
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * @cb reports completion or failure; there is no timeout parameter, cancel the
 * transfer with i2c_master_abort(). @msgs and its buffers must remain valid
 * until @cb is invoked, and may be released when @cb or i2c_master_abort()
 * returns.
 *
 * Return: 0 on success, negative I2C_ERR_* value on failure.
 */
int i2c_master_transfer_async(struct i2c_master_dev *dev,
			      const struct i2c_msg *msgs, size_t num_msgs,
			      i2c_master_done_cb_t cb, void *ctx);

/**
 * i2c_master_abort() - abort the ongoing transfer
 * @dev: I2C master device
 *
 * Context: task context.
 *
 * Aborts the ongoing transfer and clears its completion callback. After it
 * returns no transfer callback will be invoked for this device and @msgs and
 * its buffers may be released. Calling this with no transfer in flight returns
 * the same value.
 *
 * It does not trigger the completion callback; a passive abort is reported
 * through it first, using the I2C_ERR_ERR_ABORTED bit.
 *
 * If a callback is running in an ISR, this function waits for it to return, or
 * the adapter serializes the operation.
 *
 * Return: 0 on success, negative I2C_ERR_* value on failure.
 */
int i2c_master_abort(struct i2c_master_dev *dev);

#endif /* SUBSYSTEM_I2C_MASTER_H */