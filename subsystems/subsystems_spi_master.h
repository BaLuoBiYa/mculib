/* SPDX-License-Identifier: MIT */
/*
 * spi_master subsystem contract.
 */

#ifndef SUBSYSTEM_SPI_MASTER_H
#define SUBSYSTEM_SPI_MASTER_H

#include <stddef.h>
#include <stdint.h>

/*
 * Return values: >= 0 is the number of bytes transferred, or 0 for control
 * APIs; < 0 is one of the SPI_MASTER_ERR_* values below. These are not bitmask
 * flags.
 */
#define SPI_MASTER_OK 0
#define SPI_MASTER_ERR_PARAM (-1)
#define SPI_MASTER_ERR_TIMEOUT (-2)
#define SPI_MASTER_ERR_IO (-3)
#define SPI_MASTER_ERR_BUSY (-4)
#define SPI_MASTER_ERR_UNSUPPORTED (-5)

/*
 * Error bits for the @errors argument of spi_master_done_cb_t; a transfer may
 * report several of them at once.
 *
 * SPI_MASTER_ERR_OVERRUN:    receive data was lost before it could be consumed
 * SPI_MASTER_ERR_MODE_FAULT: SPI mode fault
 * SPI_MASTER_ERR_ABORTED:    transfer was terminated by hardware error or by
 *                            spi_master_abort()
 */
#define SPI_MASTER_ERR_OVERRUN (1u << 0)
#define SPI_MASTER_ERR_MODE_FAULT (1u << 1)
#define SPI_MASTER_ERR_ABORTED (1u << 2)

/**
 * enum spi_master_mode_t - SPI clock mode
 * @SPI_MASTER_MODE_0: CPOL=0, CPHA=0
 * @SPI_MASTER_MODE_1: CPOL=0, CPHA=1
 * @SPI_MASTER_MODE_2: CPOL=1, CPHA=0
 * @SPI_MASTER_MODE_3: CPOL=1, CPHA=1
 */
typedef enum {
	SPI_MASTER_MODE_0 = 0,
	SPI_MASTER_MODE_1,
	SPI_MASTER_MODE_2,
	SPI_MASTER_MODE_3,
} spi_master_mode_t;

/**
 * enum spi_master_bit_order_t - SPI bit order
 * @SPI_MASTER_BIT_ORDER_MSB_FIRST: most significant bit first
 * @SPI_MASTER_BIT_ORDER_LSB_FIRST: least significant bit first
 */
typedef enum {
	SPI_MASTER_BIT_ORDER_MSB_FIRST = 0,
	SPI_MASTER_BIT_ORDER_LSB_FIRST,
} spi_master_bit_order_t;

/**
 * struct spi_master_dev - SPI master device instance
 * @name: debug name, not interpreted by the subsystem layer
 * @priv: adapter-private data, not interpreted by the subsystem layer
 *
 * One instance represents one bound SPI slave device. Multiple instances may
 * share one physical controller; the adapter serializes access at the bus
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
 * transfer; the adapter inserts no delay of its own. Setup and hold timing is
 * the responsibility of the callback: wait for the required time before
 * returning, both when asserting (@active non-zero) and when deasserting
 * (@active zero).
 *
 * Fixed at binding time, not a runtime registration. May be NULL when hardware
 * chip select is used.
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
 * Fixed by board code at binding time; not reconfigurable at runtime.
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
 * typedef spi_master_done_cb_t - transfer completion callback
 * @dev: SPI master device that completed the transfer
 * @len: number of bytes transferred
 * @errors: error bitmask, 0 when clean, see SPI_MASTER_ERR_*
 * @ctx: user context passed to spi_master_transfer_async()
 *
 * Context: interrupt context.
 *
 * Invoked once per spi_master_transfer_async() call, after which the buffers
 * may be released. @ctx must stay valid until the callback or
 * spi_master_abort() returns.
 *
 * Must not block, sleep, wait, allocate, or call any subsystem API; execution
 * time must be bounded. May run concurrently with task context on the same
 * device.
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
 * Full-duplex transfer of @len bytes. The chip select is driven through the
 * configured @cs_ctrl callback. Concurrent calls on the same device are
 * serialized by the adapter or rejected with SPI_MASTER_ERR_BUSY.
 *
 * Return: number of bytes transferred on success, negative
 * SPI_MASTER_ERR_* value on failure.
 */
int spi_master_transfer(struct spi_master_dev *dev, const void *tx_buf,
			void *rx_buf, size_t len, uint32_t timeout);

/**
 * spi_master_transfer_async() - asynchronously transfer data
 * @dev: SPI master device
 * @tx_buf: buffer to transmit, may be NULL to receive only
 * @rx_buf: buffer to receive into, may be NULL to transmit only
 * @len: number of bytes to transfer
 * @cb: completion callback, must not be NULL, invoked exactly once
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * @cb reports completion or failure; there is no timeout parameter, cancel the
 * transfer with spi_master_abort(). The buffers must remain valid until @cb is
 * invoked, and may be released when @cb or spi_master_abort() returns.
 *
 * Return: 0 on success, negative SPI_MASTER_ERR_* value on failure.
 */
int spi_master_transfer_async(struct spi_master_dev *dev, const void *tx_buf,
			      void *rx_buf, size_t len, spi_master_done_cb_t cb,
			      void *ctx);

/**
 * spi_master_abort() - abort the ongoing transfer
 * @dev: SPI master device
 *
 * Context: task context.
 *
 * Aborts the ongoing transfer and clears its completion callback. After it
 * returns no transfer callback will be invoked for this device and the buffers
 * may be released. Calling this with no transfer in flight returns the same
 * value.
 *
 * It does not trigger the completion callback; a passive abort is reported
 * through it first, using the SPI_MASTER_ERR_ABORTED bit. The @cs_ctrl hook is
 * not affected.
 *
 * If a callback is running in an ISR, this function waits for it to return, or
 * the adapter serializes the operation.
 *
 * Return: 0 on success, negative SPI_MASTER_ERR_* value on failure.
 */
int spi_master_abort(struct spi_master_dev *dev);

#endif /* SUBSYSTEM_SPI_MASTER_H */
