/* SPDX-License-Identifier: MIT */
/*
 * spi_master subsystem contract.
 *
 * Provides a backend-independent SPI master API for applications and board
 * code. The subsystem layer declares only the contract; all hardware access
 * is performed by the adapter layer.
 */

#ifndef SUBSYSTEM_spi_master_H
#define SUBSYSTEM_spi_master_H

#include <stddef.h>
#include <stdint.h>

/*
 * Error codes for functions returning int.
 *
 *   >= 0  success: number of bytes transferred, or 0 for control APIs
 *   < 0   failure: one of the spi_master_ERR_* values below
 *
 * These values are not used as bitmask flags. Per-transfer errors are
 * reported separately through spi_master_ERR_* bits below.
 */
#define spi_master_OK 0
#define spi_master_ERR_PARAM (-1)
#define spi_master_ERR_TIMEOUT (-2)
#define spi_master_ERR_IO (-3)
#define spi_master_ERR_BUSY (-4)
#define spi_master_ERR_UNSUPPORTED (-5)

/*
 * Error bits reported in the @errors argument of spi_master_done_cb_t.
 *
 * These are bitmask flags, not return values. A single transfer may report
 * multiple error bits at the same time. Callers that do not distinguish
 * between error types may simply test @errors != 0.
 *
 * spi_master_ERR_OVERRUN:    receive data was lost because the adapter or
 *                            hardware could not consume it in time
 * spi_master_ERR_MODE_FAULT: SPI mode fault, typically caused by a slave
 *                            driving the chip select when not selected
 * spi_master_ERR_ABORTED:    transfer was terminated by hardware error or by
 *                            spi_master_abort()
 */
#define spi_master_ERR_OVERRUN (1u << 0)
#define spi_master_ERR_MODE_FAULT (1u << 1)
#define spi_master_ERR_ABORTED (1u << 2)

/**
 * enum spi_master_mode_t - SPI clock mode
 * @spi_master_MODE_0: CPOL=0, CPHA=0
 * @spi_master_MODE_1: CPOL=0, CPHA=1
 * @spi_master_MODE_2: CPOL=1, CPHA=0
 * @spi_master_MODE_3: CPOL=1, CPHA=1
 */
typedef enum {
	spi_master_MODE_0 = 0,
	spi_master_MODE_1,
	spi_master_MODE_2,
	spi_master_MODE_3,
} spi_master_mode_t;

/**
 * enum spi_master_bit_order_t - SPI bit order
 * @spi_master_BIT_ORDER_MSB_FIRST: most significant bit first
 * @spi_master_BIT_ORDER_LSB_FIRST: least significant bit first
 */
typedef enum {
	spi_master_BIT_ORDER_MSB_FIRST = 0,
	spi_master_BIT_ORDER_LSB_FIRST,
} spi_master_bit_order_t;

/**
 * struct spi_master_dev - SPI master device instance
 * @name: debug name, not interpreted by the subsystem layer
 * @priv: adapter-private data, not interpreted by the subsystem layer
 *
 * One instance represents one bound SPI slave device. Instances are created
 * by board code and exported to applications. Multiple instances may share
 * the same physical SPI controller; the adapter serializes access at the bus
 * level.
 */
struct spi_master_dev {
	const char *name;
	void *priv;
};

/**
 * typedef spi_master_cs_ctrl_t - chip select control callback
 * @cs_ctx: board-provided context, opaque to the adapter
 * @active: non-zero to assert chip select, zero to deassert
 *
 * Provided by board code and invoked by the adapter before and after a
 * transfer. The adapter does not interpret @cs_ctx, and does not insert any
 * delay between the chip select change and the transfer.
 *
 * @note Chip select setup and hold timing is the responsibility of this
 *       callback. When @active is non-zero, the callback should assert the
 *       chip select and wait for the required setup time before returning.
 *       When @active is zero, the callback should wait for the required hold
 *       time before deasserting the chip select.
 *
 * If hardware chip select is used, this callback may be NULL.
 */
typedef void (*spi_master_cs_ctrl_t)(void *cs_ctx, int active);

/**
 * struct spi_master_master_config - SPI master configuration
 * @clock_hz: clock frequency in Hz
 * @mode: SPI clock mode
 * @bit_order: bit order
 * @data_bits: number of data bits per transfer, typically 8 or 16
 * @cs_ctrl: chip select control callback, may be NULL for hardware CS
 * @cs_ctx: context passed to @cs_ctrl
 *
 * Describes the line parameters of a slave device. The board provides one of
 * these when it binds the device. Runtime reconfiguration is not provided by
 * the subsystem layer; it is done through board-level re-binding.
 */
struct spi_master_master_config {
	uint32_t clock_hz;
	spi_master_mode_t mode;
	spi_master_bit_order_t bit_order;
	uint8_t data_bits;
	spi_master_cs_ctrl_t cs_ctrl;
	void *cs_ctx;
};

/**
 * typedef spi_master_done_cb_t - asynchronous transfer completion callback
 * @dev: SPI master device that completed the transfer
 * @len: number of bytes transferred, always non-negative
 * @errors: error bitmask, 0 if no error occurred, see spi_master_ERR_*
 * @ctx: user context passed to spi_master_transfer_async()
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
typedef void (*spi_master_done_cb_t)(struct spi_master_dev *dev, size_t len,
				     uint32_t errors, void *ctx);

/**
 * spi_master_transfer() - synchronously transfer data
 * @dev: SPI master device
 * @tx_buf: buffer to transmit, may be NULL to receive only
 * @rx_buf: buffer to receive into, may be NULL to transmit only
 * @len: number of bytes to transfer
 * @timeout: timeout in milliseconds, 0 for non-blocking
 *
 * Context: task context.
 *
 * Performs a full-duplex transfer of @len bytes. If both @tx_buf and @rx_buf
 * are non-NULL, data is transmitted and received simultaneously. The chip
 * select is asserted and deasserted by the adapter using the callback
 * provided in the configuration.
 *
 * Transmission and reception are not independent channels; they are part of
 * the same transfer. Concurrent calls on the same device are serialized by
 * the adapter or rejected with spi_master_ERR_BUSY.
 *
 * Return: number of bytes transferred on success, negative
 * spi_master_ERR_* value on failure.
 */
int spi_master_transfer(struct spi_master_dev *dev, const void *tx_buf,
			void *rx_buf, size_t len, uint32_t timeout);

/**
 * spi_master_transfer_async() - asynchronously transfer data
 * @dev: SPI master device
 * @tx_buf: buffer to transmit, may be NULL to receive only
 * @rx_buf: buffer to receive into, may be NULL to transmit only
 * @len: number of bytes to transfer
 * @cb: completion callback
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * Starts a full-duplex transfer of @len bytes. The buffers must remain valid
 * until @cb is invoked. Once started, the transfer has only two outcomes:
 * completion or failure, both reported through @cb. There is no separate
 * timeout parameter; if the caller needs to cancel the transfer, it calls
 * spi_master_abort().
 *
 * Return: 0 on success, negative spi_master_ERR_* value on failure.
 */
int spi_master_transfer_async(struct spi_master_dev *dev, const void *tx_buf,
			      void *rx_buf, size_t len, spi_master_done_cb_t cb,
			      void *ctx);

/**
 * spi_master_abort() - abort ongoing transfer
 * @dev: SPI master device
 *
 * Context: task context.
 *
 * Aborts any ongoing asynchronous transfer. After this function returns, no
 * further transfer callback will be invoked for this device.
 *
 * This is an active abort initiated by the caller. It does not trigger the
 * completion callback, because the caller already knows the operation was
 * cancelled. Passive aborts caused by hardware errors are reported through
 * the completion callback instead, using the spi_master_ERR_ABORTED bit.
 *
 * If a callback is currently executing in an ISR, this function waits for it
 * to return, or the adapter serializes the operation.
 *
 * Return: 0 on success, negative spi_master_ERR_* value on failure.
 */
int spi_master_abort(struct spi_master_dev *dev);

#endif /* SUBSYSTEM_spi_master_H */
