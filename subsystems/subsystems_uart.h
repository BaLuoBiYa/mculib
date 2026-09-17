/* SPDX-License-Identifier: MIT */
/*
 * UART subsystem contract.
 */

#ifndef SUBSYSTEM_UART_H
#define SUBSYSTEM_UART_H

#include <stddef.h>
#include <stdint.h>

/*
 * Return values: >= 0 is the number of bytes transferred, or 0 for control
 * APIs; < 0 is one of the UART_ERR_* values below. These are not bitmask
 * flags.
 */
#define UART_OK 0
#define UART_ERR_PARAM (-1)
#define UART_ERR_TIMEOUT (-2)
#define UART_ERR_IO (-3)
#define UART_ERR_BUSY (-4)
#define UART_ERR_UNSUPPORTED (-5)

/*
 * Error bits for the @errors argument of uart_rx_cb_t; a frame may report
 * several of them at once.
 *
 * UART_RX_ERR_OVERRUN: receive data was lost before it could be consumed
 * UART_RX_ERR_PARITY:  parity check failed
 * UART_RX_ERR_FRAMING: framing error
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
 * One instance represents one bound UART port.
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
 * Fixed by board code at binding time; not reconfigurable at runtime.
 */
struct uart_config {
	uint32_t baudrate;
	uint8_t databits;
	uint8_t stopbits;
	uart_parity_t parity;
};

/**
 * typedef uart_rx_cb_t - receive callback
 * @dev: UART device that received the frame
 * @len: number of bytes received
 * @errors: error bitmask, 0 when the frame is clean, see UART_RX_ERR_*
 * @ctx: user context passed to uart_recv_async()
 *
 * Context: interrupt context.
 *
 * Invoked for every frame until uart_abort() clears the subscription. The
 * registered buffer may be reused by the adapter as soon as the callback
 * returns, so data that has to be kept must be copied out here. @ctx must stay
 * valid until uart_abort() returns.
 *
 * Must not block, sleep, wait, allocate, or call any subsystem API; execution
 * time must be bounded. May run concurrently with task context on the same
 * device.
 */
typedef void (*uart_rx_cb_t)(struct uart_dev *dev, size_t len, uint32_t errors,
			     void *ctx);

/**
 * typedef uart_tx_done_cb_t - transmit completion callback
 * @dev: UART device that completed the transmission
 * @result: 0 on completion, negative UART_ERR_* value on failure
 * @ctx: user context passed to uart_send_async()
 *
 * Context: interrupt context.
 *
 * Invoked once per uart_send_async() call, after which the transmit buffer may
 * be released. @ctx must stay valid until the callback or uart_abort()
 * returns.
 *
 * Must not block, sleep, wait, allocate, or call any subsystem API; execution
 * time must be bounded. May run concurrently with task context on the same
 * device.
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
 * @cb: completion callback, must not be NULL, invoked exactly once
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * @cb reports completion or failure; there is no timeout parameter, cancel the
 * transmission with uart_abort(). @buf may be released when @cb returns, or
 * when uart_abort() returns.
 *
 * Return: 0 on success, negative UART_ERR_* value on failure.
 */
int uart_send_async(struct uart_dev *dev, const void *buf, size_t len,
		    uart_tx_done_cb_t cb, void *ctx);

/**
 * uart_recv_async() - register the receive subscription
 * @dev: UART device
 * @buf: receive buffer, used for every frame of the subscription
 * @max_len: buffer capacity in bytes
 * @cb: frame callback, must not be NULL
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * Registers @cb once; it is invoked for every frame until uart_abort() clears
 * the subscription. The frame boundary is the hardware idle interrupt. A frame
 * longer than @max_len is truncated to @max_len and reported with
 * UART_RX_ERR_OVERRUN. Reception and transmission are independent and may
 * proceed concurrently.
 *
 * @buf must not be released, reused, or written while the subscription is
 * registered. One subscription per device is supported; registering while a
 * subscription is active is a contract breach, and uart_abort() followed by a
 * new registration is the only way to replace @cb or @ctx. uart_recv() must not
 * be used while a subscription is registered.
 *
 * If the bus never goes idle, no callback is invoked and the caller provides
 * timeout protection.
 *
 * Return: 0 on success, negative UART_ERR_* value on failure.
 */
int uart_recv_async(struct uart_dev *dev, void *buf, size_t max_len,
		    uart_rx_cb_t cb, void *ctx);

/**
 * uart_abort() - stop the device and clear every callback
 * @dev: UART device
 *
 * Context: task context.
 *
 * Aborts the ongoing transmit and receive operations and clears the receive
 * subscription. After it returns no callback will be invoked for this device
 * and every buffer handed to it may be released. The device must be registered
 * again before it is used; calling this with nothing registered returns the
 * same value.
 *
 * It does not trigger the completion callbacks. Passive aborts are reported
 * through them first - a negative @result for transmit, the
 * UART_RX_ERR_ABORTED bit for receive - and also clear the receive
 * subscription.
 *
 * If a callback is running in an ISR, this function waits for it to return, or
 * the adapter serializes the operation.
 *
 * Return: 0 on success, negative UART_ERR_* value on failure.
 */
int uart_abort(struct uart_dev *dev);

#endif /* SUBSYSTEM_UART_H */