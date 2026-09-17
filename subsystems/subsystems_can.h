/* SPDX-License-Identifier: MIT */
/*
 * can subsystem contract.
 *
 * Provides a backend-independent CAN API for applications and board code.
 * The subsystem layer declares only the contract; all hardware access is
 * performed by the adapter layer.
 *
 * A CAN instance represents one bound CAN controller. Bit timing and
 * controller mode are fixed by board code at binding time and are not
 * changed at runtime. Acceptance filters may be set after binding.
 */

#ifndef SUBSYSTEM_CAN_H
#define SUBSYSTEM_CAN_H

#include <stddef.h>
#include <stdint.h>

/*
 * Error codes for functions returning int.
 *
 *   >= 0  success: number of frames transferred, or 0 for control APIs
 *   < 0   failure: one of the CAN_ERR_* values below
 *
 * These values express operation-level results only: whether the call was
 * accepted, completed, or timed out. They do not describe frame-level errors
 * on the bus. Frame-level errors are reported through the CAN_EVT_ERR_* bit
 * mask in callbacks.
 *
 * CAN_ERR_TIMEOUT is returned only by synchronous APIs.
 */
#define CAN_OK 0
#define CAN_ERR_PARAM (-1)
#define CAN_ERR_TIMEOUT (-2)
#define CAN_ERR_BUSY (-3)
#define CAN_ERR_UNSUPPORTED (-4)

/*
 * Event error bits reported in the @errors argument of callbacks.
 *
 * These are bitmask flags, not return values. A single frame may report
 * multiple error bits at the same time. They are used for both transmit and
 * receive events. Callers that do not distinguish between error types may
 * simply test @errors != 0.
 *
 * CAN_EVT_ERR_BIT:         bit error detected on the bus
 * CAN_EVT_ERR_FORM:        form error detected on the bus
 * CAN_EVT_ERR_ACK:         acknowledgement error
 * CAN_EVT_ERR_CRC:         CRC error
 * CAN_EVT_ERR_STUFF:       bit stuffing error
 * CAN_EVT_ERR_ARBITRATION: arbitration was lost
 * CAN_EVT_ERR_BUS_OFF:     controller entered bus-off state
 * CAN_EVT_ERR_ABORTED:     operation was terminated by can_abort()
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
 * One instance represents one bound CAN controller. Instances are created by
 * board code and exported to applications.
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
 * The caller assembles @id by combining the frame identifier with the
 * appropriate flags:
 *
 *   - standard frame:  @id = identifier & CAN_SFF_MASK
 *   - extended frame:  @id = CAN_EFF_FLAG | (identifier & CAN_EFF_MASK)
 *   - remote frame:    @id |= CAN_RTR_FLAG
 *
 * The adapter extracts the flags and identifier and maps them to hardware
 * registers. CAN_ERR_FLAG is set by the adapter on received error frames and
 * must not be set by the caller when transmitting.
 *
 * For a remote frame, CAN_RTR_FLAG is set and @dlc indicates the requested
 * length; @data is not transmitted.
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
 * A frame is accepted if (frame_id & mask) == (filter_id & mask). Both
 * standard and extended frames can be filtered. The board or application
 * provides an array of filters; the adapter programs them into hardware
 * acceptance filters.
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
 * Describes the physical layer parameters of a CAN controller. The board
 * provides one of these when it binds the device. Runtime reconfiguration is
 * not provided by the subsystem layer; it is done through board-level
 * re-binding.
 */
struct can_config {
	uint32_t bitrate;
	uint32_t sample_point;
	int loopback;
	int listen_only;
};

/**
 * typedef can_rx_cb_t - asynchronous receive callback
 * @dev: CAN device that received the frame
 * @msg: received frame, valid only for the duration of the callback
 * @errors: event error bitmask, 0 if no error occurred, see CAN_EVT_ERR_*
 * @ctx: user context passed to can_recv_async()
 *
 * Context: interrupt context (executed directly inside the ISR).
 *
 * @msg is valid only for the duration of the callback. The caller must copy
 * any data it needs to retain.
 *
 * The callback must not block, sleep, wait, allocate memory, or call any
 * subsystem API. Execution time must be bounded.
 *
 * The callback may run concurrently with task-context operations on the same
 * device. The adapter must guarantee API atomicity, or the callback must be
 * restricted to setting a flag without touching the device.
 */
typedef void (*can_rx_cb_t)(struct can_dev *dev, const struct can_msg *msg,
			    uint32_t errors, void *ctx);

/**
 * typedef can_tx_done_cb_t - asynchronous transmit completion callback
 * @dev: CAN device that completed the transmission
 * @result: operation-level result, 0 or a negative CAN_ERR_* value
 * @errors: event error bitmask, 0 if no error occurred, see CAN_EVT_ERR_*
 * @ctx: user context passed to can_send_async()
 *
 * Context: interrupt context (executed directly inside the ISR).
 *
 * @result expresses whether the operation was accepted and completed.
 * @errors expresses frame-level errors observed on the bus. The two are
 * independent: a transfer may complete with @result == 0 and non-zero
 * @errors, meaning the frame was processed but the bus reported errors.
 *
 * The callback must not block, sleep, wait, allocate memory, or call any
 * subsystem API. Execution time must be bounded.
 *
 * The callback may run concurrently with task-context operations on the same
 * device. The adapter must guarantee API atomicity, or the callback must be
 * restricted to setting a flag without touching the device.
 */
typedef void (*can_tx_done_cb_t)(struct can_dev *dev, int result,
				 uint32_t errors, void *ctx);

/**
 * can_set_filter() - set acceptance filters
 * @dev: CAN device
 * @filters: array of filters, may be NULL to accept all frames
 * @num_filters: number of filters in @filters
 *
 * Context: task context.
 *
 * Programs the hardware acceptance filters. A frame is accepted if it
 * matches any of the filters. If @filters is NULL and @num_filters is 0, all
 * frames are accepted.
 *
 * The number of filters a controller supports is backend-specific. If
 * @num_filters exceeds the hardware capacity, the function returns
 * CAN_ERR_UNSUPPORTED.
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
 * Transmits a single frame. If the bus is busy or arbitration is lost, the
 * adapter retries according to the CAN protocol. The function returns when
 * the frame has been acknowledged or the timeout expires.
 *
 * This function returns operation-level results only. Frame-level errors
 * observed on the bus are not reported here; use can_send_async() if the
 * caller needs access to CAN_EVT_ERR_* bits.
 *
 * Return: CAN_OK on success, negative CAN_ERR_* value on failure.
 */
int can_send(struct can_dev *dev, const struct can_msg *msg, uint32_t timeout);

/**
 * can_send_async() - asynchronously transmit a frame
 * @dev: CAN device
 * @msg: frame to transmit, must remain valid until @cb is invoked
 * @cb: completion callback
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * Starts an asynchronous transmission. The @msg must remain valid until @cb
 * is invoked. Once started, the transmission has only two outcomes:
 * completion or failure, both reported through @cb.
 *
 * Return: CAN_OK on success, negative CAN_ERR_* value on failure.
 */
int can_send_async(struct can_dev *dev, const struct can_msg *msg,
		   can_tx_done_cb_t cb, void *ctx);

/**
 * can_recv_async() - start asynchronous reception
 * @dev: CAN device
 * @cb: receive callback
 * @ctx: user context passed to @cb
 *
 * Context: task context.
 *
 * Starts asynchronous reception. For each accepted frame, @cb is invoked with
 * the frame and any event error bits. Reception continues until
 * can_recv_disable() or can_abort() is called.
 *
 * Return: CAN_OK on success, negative CAN_ERR_* value on failure.
 */
int can_recv_async(struct can_dev *dev, can_rx_cb_t cb, void *ctx);

/**
 * can_abort() - abort ongoing transmit and receive
 * @dev: CAN device
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
 * the completion callbacks instead, using the CAN_EVT_ERR_ABORTED bit.
 *
 * If a callback is currently executing in an ISR, this function waits for it
 * to return, or the adapter serializes the operation.
 *
 * Return: CAN_OK on success, negative CAN_ERR_* value on failure.
 */
int can_abort(struct can_dev *dev);

#endif /* SUBSYSTEM_CAN_H */