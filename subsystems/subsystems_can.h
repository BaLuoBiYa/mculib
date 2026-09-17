/* SPDX-License-Identifier: MIT */
/*
 * CAN subsystem contract.
 */

#ifndef SUBSYSTEM_CAN_H
#define SUBSYSTEM_CAN_H

#include <stddef.h>
#include <stdint.h>

/*
 * Return values: >= 0 is the number of frames transferred, or 0 for control
 * APIs; < 0 is one of the CAN_ERR_* values below, which describe the operation
 * only and never frame-level bus errors. Those are reported through the
 * CAN_EVT_ERR_* bits in callbacks. CAN_ERR_TIMEOUT is returned by synchronous
 * APIs only.
 */
#define CAN_OK 0
#define CAN_ERR_PARAM (-1)
#define CAN_ERR_TIMEOUT (-2)
#define CAN_ERR_BUSY (-3)
#define CAN_ERR_UNSUPPORTED (-4)

/*
 * Event error bits for the @errors argument of callbacks, used for both
 * transmit and receive; a frame may report several of them at once.
 *
 * CAN_EVT_ERR_BIT:         bit error on the bus
 * CAN_EVT_ERR_FORM:        form error on the bus
 * CAN_EVT_ERR_ACK:         acknowledgement error
 * CAN_EVT_ERR_CRC:         CRC error
 * CAN_EVT_ERR_STUFF:       bit stuffing error
 * CAN_EVT_ERR_ARBITRATION: arbitration lost
 * CAN_EVT_ERR_BUS_OFF:     controller entered bus-off
 * CAN_EVT_ERR_ABORTED:     operation terminated by can_abort()
 */
#define CAN_EVT_ERR_BIT (1u << 0)
#define CAN_EVT_ERR_FORM (1u << 1)
#define CAN_EVT_ERR_ACK (1u << 2)
#define CAN_EVT_ERR_CRC (1u << 3)
#define CAN_EVT_ERR_STUFF (1u << 4)
#define CAN_EVT_ERR_ARBITRATION (1u << 5)
#define CAN_EVT_ERR_BUS_OFF (1u << 6)
#define CAN_EVT_ERR_ABORTED (1u << 7)

/*
 * CAN identifier flags and masks.
 *
 * CAN_EFF_FLAG: extended frame, 29-bit identifier
 * CAN_RTR_FLAG: remote transmission request
 * CAN_ERR_FLAG: error frame, set by the adapter on received error frames
 *
 * CAN_SFF_MASK: standard frame identifier mask, 11 bits
 * CAN_EFF_MASK: extended frame identifier mask, 29 bits
 */
#define CAN_EFF_FLAG (0x80000000U)
#define CAN_RTR_FLAG (0x40000000U)
#define CAN_ERR_FLAG (0x20000000U)
#define CAN_SFF_MASK (0x000007FFU)
#define CAN_EFF_MASK (0x1FFFFFFFU)

/**
 * struct can_dev - CAN device instance
 * @name: debug name, not interpreted by the subsystem layer
 * @priv: adapter-private data, not interpreted by the subsystem layer
 *
 * One instance represents one bound CAN controller.
 */
struct can_dev {
	const char *name;
	void *priv;
};

/**
 * struct can_msg - CAN frame
 * @id: identifier with CAN_*_FLAG bits, assembled by the caller
 * @dlc: data length code, 0 to 8 for classic CAN
 * @data: frame payload
 *
 * @id is assembled by the caller:
 *
 *   standard frame:  identifier & CAN_SFF_MASK
 *   extended frame:  CAN_EFF_FLAG | (identifier & CAN_EFF_MASK)
 *   remote frame:    the above, plus CAN_RTR_FLAG
 *
 * CAN_ERR_FLAG is set by the adapter on received error frames and must not be
 * set when transmitting. For a remote frame @dlc is the requested length and
 * @data is not transmitted.
 */
struct can_msg {
	uint32_t id;
	uint8_t dlc;
	uint8_t data[8];
};

/**
 * struct can_filter - acceptance filter
 * @id: identifier to match, with CAN_*_FLAG bits
 * @mask: bit mask, 1 means compare, 0 means don't care
 *
 * A frame is accepted if (frame_id & mask) == (filter_id & mask).
 */
struct can_filter {
	uint32_t id;
	uint32_t mask;
};

/**
 * struct can_config - CAN controller configuration
 * @bitrate: nominal bit rate in bits per second
 * @sample_point: sample point in 1/1000 units, e.g. 875 for 87.5%
 * @loopback: non-zero to enable loopback mode
 * @listen_only: non-zero to enable listen-only mode
 *
 * Fixed by board code at binding time; not reconfigurable at runtime.
 */
struct can_config {
	uint32_t bitrate;
	uint32_t sample_point;
	int loopback;
	int listen_only;
};

/**
 * typedef can_rx_cb_t - receive callback
 * @dev: CAN device that received the frame
 * @msg: received frame, valid only for the duration of the callback
 * @errors: event error bitmask, 0 when the frame is clean, see CAN_EVT_ERR_*
 * @ctx: user context passed to can_recv_async()
 *
 * Context: interrupt context.
 *
 * Invoked for every accepted frame until can_abort() clears the subscription.
 * Anything to be retained has to be copied out of @msg here. @ctx must stay
 * valid until can_abort() returns.
 *
 * Must not block, sleep, wait, allocate, or call any subsystem API; execution
 * time must be bounded. May run concurrently with task context on the same
 * device.
 */
typedef void (*can_rx_cb_t)(struct can_dev *dev, const struct can_msg *msg,
			    uint32_t errors, void *ctx);

/**
 * typedef can_tx_done_cb_t - transmit completion callback
 * @dev: CAN device that completed the transmission
 * @result: 0 or a negative CAN_ERR_* value, the operation-level result
 * @errors: event error bitmask, 0 when clean, see CAN_EVT_ERR_*
 * @ctx: user context passed to can_send_async()
 *
 * Context: interrupt context.
 *
 * Invoked once per can_send_async() call, after which @msg may be released.
 * @result and @errors are independent: a non-zero @result means the operation
 * failed, a non-zero @errors means the bus reported frame-level errors. @ctx
 * must stay valid until the callback or can_abort() returns.
 *
 * Must not block, sleep, wait, allocate, or call any subsystem API; execution
 * time must be bounded. May run concurrently with task context on the same
 * device.
 */
typedef void (*can_tx_done_cb_t)(struct can_dev *dev, int result,
				 uint32_t errors, void *ctx);

/**
 * can_set_filter() - set acceptance filters
 * @dev: CAN device
 * @filters: array of filters
 * @num_filters: number of filters in @filters
 *
 * Context: task context.
 *
 * Can be called after binding. A frame is accepted when it matches any of the
 * filters; @filters may be NULL with @num_filters 0 to accept all frames.
 * Exceeding the filter capacity of the backend returns CAN_ERR_UNSUPPORTED.
 *
 * Return: CAN_OK on success, negative CAN_ERR_* value on failure.
 */
int can_set_filter(struct can_dev *dev, const struct can_filter *filters,
		   size_t num_filters);

/**
 * can_send() - synchronously transmit a frame
 * @dev: CAN device
 * @msg: frame to transmit
 * @timeout: timeout in milliseconds, 0 for non-blocking
 *
 * Context: task context.
 *
 * Transmits a single frame and returns when it has been acknowledged or the
 * timeout expires. Only operation-level results are reported; use
 * can_send_async() to observe the CAN_EVT_ERR_* bits.
 *
 * Return: CAN_OK on success, negative CAN_ERR_* value on failure.
 */
int can_send(struct can_dev *dev, const struct can_msg *msg, uint32_t timeout);

/**
 * can_send_async() - asynchronously transmit a frame
 * @dev: CAN device
 * @msg: frame to transmit, must remain valid until @cb is invoked
 * @cb: completion callback, must not be NULL, invoked exactly once
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * @cb reports completion or failure. @msg may be released when @cb returns, or
 * when can_abort() returns.
 *
 * Return: CAN_OK on success, negative CAN_ERR_* value on failure.
 */
int can_send_async(struct can_dev *dev, const struct can_msg *msg,
		   can_tx_done_cb_t cb, void *ctx);

/**
 * can_recv_async() - register the receive subscription
 * @dev: CAN device
 * @cb: receive callback, must not be NULL
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * Registers @cb once; it is invoked for every accepted frame until can_abort()
 * clears the subscription. A passive abort clears it as well, after delivering
 * the event that caused it. @ctx must not be released before that.
 *
 * One subscription per device is supported; registering while a subscription is
 * active is a contract breach, and can_abort() followed by a new registration
 * is the only way to replace @cb or @ctx.
 *
 * Return: CAN_OK on success, negative CAN_ERR_* value on failure.
 */
int can_recv_async(struct can_dev *dev, can_rx_cb_t cb, void *ctx);

/**
 * can_abort() - stop the device and clear every callback
 * @dev: CAN device
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
 * CAN_EVT_ERR_ABORTED and CAN_EVT_ERR_BUS_OFF bits for the bus state - and also
 * clear the receive subscription.
 *
 * If a callback is running in an ISR, this function waits for it to return, or
 * the adapter serializes the operation.
 *
 * Return: CAN_OK on success, negative CAN_ERR_* value on failure.
 */
int can_abort(struct can_dev *dev);

#endif /* SUBSYSTEM_CAN_H */