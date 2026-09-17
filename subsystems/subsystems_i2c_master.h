/* SPDX-License-Identifier: MIT */
/*
 * i2c_master subsystem contract.
 *
 * Provides a backend-independent I2C master API for applications and board
 * code. The subsystem layer declares only the contract; all hardware access
 * is performed by the adapter layer.
 */

#ifndef SUBSYSTEM_I2C_MASTER_H
#define SUBSYSTEM_I2C_MASTER_H

#include <stddef.h>
#include <stdint.h>

/*
 * Error codes for functions returning int.
 *
 *   >= 0  success: number of messages transferred, or 0 for control APIs
 *   < 0   failure: one of the I2C_ERR_* values below
 *
 * These values are not used as bitmask flags. Per-transfer errors are
 * reported separately through I2C_ERR_* bits below.
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
 * Error bits reported in the @errors argument of i2c_master_done_cb_t.
 *
 * These are bitmask flags, not return values. A single transfer may report
 * multiple error bits at the same time. Callers that do not distinguish
 * between error types may simply test @errors != 0.
 *
 * I2C_ERR_ERR_NACK:             slave did not acknowledge
 * I2C_ERR_ERR_ARBITRATION_LOST: bus arbitration was lost
 * I2C_ERR_ERR_BUS:              bus error, typically a misplaced start or
 *                               stop condition
 * I2C_ERR_ERR_OVERRUN:          receive data was lost because the adapter or
 *                               hardware could not consume it in time
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
 * One instance represents one bound I2C slave device. Instances are created
 * by board code and exported to applications. Multiple instances may share
 * the same physical I2C bus; the adapter serializes access at the bus level.
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
 * Exactly one of @tx_buf and @rx_buf must be non-NULL. A message with both
 * set is invalid, because I2C is half-duplex. A message with both NULL and
 * @len == 0 is a valid zero-length message, used to probe a device address.
 *
 * A transfer consists of one or more messages. Messages are sent in order,
 * with a repeated start condition between them and a stop condition after
 * the last one. This is the standard way to implement register access:
 * write the register address, then read the data, in a single transfer.
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
 * Describes the bus parameters and the slave address of a device. The board
 * provides one of these when it binds the device. Runtime reconfiguration is
 * not provided by the subsystem layer; it is done through board-level
 * re-binding.
 */
struct i2c_master_config {
	uint32_t clock_hz;
	uint16_t address;
	uint8_t address_bits;
};

/**
 * typedef i2c_master_done_cb_t - asynchronous transfer completion callback
 * @dev: I2C master device that completed the transfer
 * @num_msgs: number of messages successfully transferred, always non-negative
 * @errors: error bitmask, 0 if no error occurred, see I2C_ERR_ERR_*
 * @ctx: user context passed to i2c_master_transfer_async()
 *
 * Context: interrupt context (executed directly inside the ISR).
 *
 * @num_msgs is the number of messages that completed successfully. If the
 * transfer was aborted in the middle, it is less than the total number of
 * messages, and @errors is non-zero.
 *
 * The callback must not block, sleep, wait, allocate memory, or call any
 * subsystem API. Execution time must be bounded.
 *
 * The callback may run concurrently with task-context operations on the same
 * device. The adapter must guarantee API atomicity, or the callback must be
 * restricted to setting a flag without touching the device.
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
 * Transfers @num_msgs messages in order. Each message is either a write or a
 * read, never both. Between messages, a repeated start condition is issued;
 * after the last message, a stop condition is issued.
 *
 * The address and address width are taken from the device configuration.
 * Concurrent calls on the same device are serialized by the adapter or
 * rejected with I2C_ERR_BUSY.
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
 * @cb: completion callback
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * Starts an asynchronous transfer of @num_msgs messages. The @msgs array and
 * the buffers it points to must remain valid until @cb is invoked.
 *
 * Once started, the transfer has only two outcomes: completion or failure,
 * both reported through @cb. There is no separate timeout parameter; if the
 * caller needs to cancel the transfer, it calls i2c_master_abort().
 *
 * Return: 0 on success, negative I2C_ERR_* value on failure.
 */
int i2c_master_transfer_async(struct i2c_master_dev *dev,
			      const struct i2c_msg *msgs, size_t num_msgs,
			      i2c_master_done_cb_t cb, void *ctx);

/**
 * i2c_master_abort() - abort ongoing transfer
 * @dev: I2C master device
 *
 * Context: task context.
 *
 * Aborts any ongoing asynchronous transfer. After this function returns, no
 * further transfer callback will be invoked for this device.
 *
 * This is an active abort initiated by the caller. It does not trigger the
 * completion callback, because the caller already knows the operation was
 * cancelled. Passive aborts caused by hardware errors are reported through
 * the completion callback instead, using the I2C_ERR_ERR_ABORTED bit.
 *
 * If a callback is currently executing in an ISR, this function waits for it
 * to return, or the adapter serializes the operation.
 *
 * Return: 0 on success, negative I2C_ERR_* value on failure.
 */
int i2c_master_abort(struct i2c_master_dev *dev);

#endif /* SUBSYSTEM_I2C_MASTER_H */