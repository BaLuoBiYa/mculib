/* SPDX-License-Identifier: MIT */
/*
 * UART subsystem contract.
 *
 * Provides a backend-independent UART API for applications and board code.
 * The subsystem layer declares only the contract; all hardware access is
 * performed by the adapter layer.
 */

#ifndef SUBSYSTEM_UART_H
#define SUBSYSTEM_UART_H

#include <stddef.h>
#include <stdint.h>

/*
 * Error codes for functions returning int.
 *
 *   >= 0  success: number of bytes transferred, or 0 for control APIs
 *   < 0   failure: one of the UART_ERR_* values below
 *
 * These values are not used as bitmask flags. Per-frame receive errors are
 * reported separately through UART_RX_ERR_* below.
 */
#define UART_OK 0
#define UART_ERR_PARAM (-1)
#define UART_ERR_TIMEOUT (-2)
#define UART_ERR_IO (-3)
#define UART_ERR_BUSY (-4)
#define UART_ERR_UNSUPPORTED (-5)

/*
 * Error bits reported in the @errors argument of uart_rx_cb_t.
 *
 * These are bitmask flags, not return values. A single frame may report
 * multiple error bits at the same time. Callers that do not distinguish
 * between error types may simply test @errors != 0.
 *
 * UART_RX_ERR_OVERRUN: receive data was lost because the adapter or hardware
 *                      could not consume it in time
 * UART_RX_ERR_PARITY:  parity check failed
 * UART_RX_ERR_FRAMING: framing error, typically caused by baud rate mismatch
 *                      or line noise
 * UART_RX_ERR_ABORTED: reception was terminated by hardware error or by
 *                      uart_abort()
 */
#define UART_RX_ERR_OVERRUN (1u << 0)
#define UART_RX_ERR_PARITY (1u << 1)
#define UART_RX_ERR_FRAMING (1u << 2)
#define UART_RX_ERR_ABORTED (1u << 3)

/**
 * enum uart_parity_t - UART parity configuration
 * @UART_PARITY_NONE: no parity
 * @UART_PARITY_ODD: odd parity
 * @UART_PARITY_EVEN: even parity
 */
typedef enum {
	UART_PARITY_NONE = 0,
	UART_PARITY_ODD,
	UART_PARITY_EVEN,
} uart_parity_t;

/**
 * struct uart_dev - UART device instance
 * @name: debug name, not interpreted by the subsystem layer
 * @priv: adapter-private data, not interpreted by the subsystem layer
 *
 * One instance represents one bound UART port. Instances are created by
 * board code and exported to applications.
 */
struct uart_dev {
	const char *name;
	void *priv;
};

/**
 * struct uart_config - UART line configuration
 * @baudrate: baud rate in bits per second
 * @databits: number of data bits, typically 7 or 8
 * @stopbits: number of stop bits, typically 1 or 2
 * @parity: parity mode, see enum uart_parity_t
 *
 * Describes the line parameters of a device. The board provides one of these
 * when it binds the device. Runtime reconfiguration is not provided by the
 * subsystem layer; it is done through board-level re-binding.
 */
struct uart_config {
	uint32_t baudrate;
	uint8_t databits;
	uint8_t stopbits;
	uart_parity_t parity;
};

/**
 * typedef uart_rx_cb_t - asynchronous receive completion callback
 * @dev: UART device that completed the reception
 * @len: number of bytes received, always non-negative
 * @errors: error bitmask, 0 if no error occurred, see UART_RX_ERR_*
 * @ctx: user context passed to uart_recv_async()
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
typedef void (*uart_rx_cb_t)(struct uart_dev *dev, size_t len, uint32_t errors,
			     void *ctx);

/**
 * typedef uart_tx_done_cb_t - asynchronous transmit completion callback
 * @dev: UART device that completed the transmission
 * @result: 0 on completion, negative UART_ERR_* value on failure
 * @ctx: user context passed to uart_send_async()
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
typedef void (*uart_tx_done_cb_t)(struct uart_dev *dev, int result, void *ctx);

/**
 * uart_send() - synchronously transmit data
 * @dev: UART device
 * @buf: buffer to transmit
 * @len: number of bytes to transmit
 * @timeout: timeout in milliseconds, 0 for non-blocking
 *
 * Context: task context.
 *
 * Return: number of bytes transmitted on success, negative UART_ERR_* value
 * on failure.
 */
int uart_send(struct uart_dev *dev, const void *buf, size_t len,
	      uint32_t timeout);

/**
 * uart_recv() - synchronously receive data
 * @dev: UART device
 * @buf: buffer to receive into
 * @len: maximum number of bytes to receive
 * @timeout: timeout in milliseconds, 0 for non-blocking
 *
 * Context: task context.
 *
 * Return: number of bytes received on success, negative UART_ERR_* value on
 * failure.
 */
int uart_recv(struct uart_dev *dev, void *buf, size_t len, uint32_t timeout);

/**
 * uart_send_async() - asynchronously transmit data
 * @dev: UART device
 * @buf: buffer to transmit, must remain valid until @cb is invoked
 * @len: number of bytes to transmit
 * @cb: completion callback
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * The buffer must remain valid until the completion callback is invoked.
 * Once started, transmission has only two outcomes: completion or failure,
 * both reported through @cb. There is no separate timeout parameter; if the
 * caller needs to cancel the transmission, it calls uart_abort().
 *
 * Return: 0 on success, negative UART_ERR_* value on failure.
 */
int uart_send_async(struct uart_dev *dev, const void *buf, size_t len,
		    uart_tx_done_cb_t cb, void *ctx);

/**
 * uart_recv_async() - asynchronously receive one frame
 * @dev: UART device
 * @buf: buffer to receive into, must remain valid until @cb is invoked
 * @max_len: buffer capacity, not expected frame length
 * @cb: frame completion callback
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * The frame boundary is determined by the hardware idle interrupt. When the
 * bus goes idle, the adapter invokes @cb with the actual number of bytes
 * received and an error bitmask.
 *
 * @max_len is the buffer capacity, not the expected frame length. Reception
 * stops after the callback, and @buf is returned to the caller. The caller
 * must invoke uart_recv_async() again to receive the next frame.
 *
 * If the bus never goes idle, the callback is never invoked. The caller is
 * responsible for implementing timeout protection, for example by using a
 * timer to call uart_abort().
 *
 * Transmission and reception are independent channels and may proceed
 * concurrently. Concurrent calls in the same direction are serialized by the
 * adapter or rejected with UART_ERR_BUSY.
 *
 * Return: 0 on success, negative UART_ERR_* value on failure.
 */
int uart_recv_async(struct uart_dev *dev, void *buf, size_t max_len,
		    uart_rx_cb_t cb, void *ctx);

/**
 * uart_abort() - abort ongoing transmit and receive
 * @dev: UART device
 *
 * Context: task context.
 *
 * Aborts any ongoing asynchronous transmit or receive operation. After this
 * function returns, no further transmit or receive callback will be invoked
 * for this device.
 *
 * This is an active abort initiated by the caller. It does not trigger the
 * completion callback, because the caller already knows the operation was
 * cancelled. Passive aborts caused by hardware errors are reported through
 * the completion callbacks instead: transmit uses @result with a negative
 * UART_ERR_* value, receive uses the UART_RX_ERR_ABORTED bit.
 *
 * If a callback is currently executing in an ISR, this function waits for it
 * to return, or the adapter serializes the operation.
 *
 * Return: 0 on success, negative UART_ERR_* value on failure.
 */
int uart_abort(struct uart_dev *dev);

#endif /* SUBSYSTEM_UART_H */