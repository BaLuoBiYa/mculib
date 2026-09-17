/* SPDX-License-Identifier: MIT */
/*
 * RobStride motor group driver.
 *
 * One instance covers every motor on one CAN bus. The receive callback runs in
 * interrupt context and only queues the frame; robstride_process() decodes
 * frames and transmits pending ones from task context, never blocking.
 *
 * CAN event error bits describe the bus, not the motor: the driver counts them
 * internally and never reports them as motor faults.
 */

#ifndef ROBSTRIDE_H
#define ROBSTRIDE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/ringbuf.h"
#include "subsystems/subsystems_can.h"

#define ROBSTRIDE_MAX_MOTORS 8
#define ROBSTRIDE_RX_QUEUE_LEN 16

/*
 * Identifier fields: motor ID in bits 7:0, 16-bit data in bits 23:8, command in
 * bits 28:24. A feedback frame carries the motor ID in bits 15:8, the faults in
 * bits 21:16 and the mode state in bits 23:22.
 */
#define ROBSTRIDE_MASTER_ID 42
#define ROBSTRIDE_CMD_CONTROL 0x01u
#define ROBSTRIDE_CMD_STATE 0x02u
#define ROBSTRIDE_CMD_ENABLE 0x03u
#define ROBSTRIDE_CMD_DISABLE 0x04u

/* Motor faults, bits 16 to 21 of a feedback identifier, as a bitmask. */
#define ROBSTRIDE_FAULT_UNDERVOLTAGE (1u << 0) /* supply voltage low */
#define ROBSTRIDE_FAULT_OVERCURRENT (1u << 1) /* phase current high */
#define ROBSTRIDE_FAULT_OVERTEMP (1u << 2) /* winding too hot */
#define ROBSTRIDE_FAULT_MAG_ENCODER (1u << 3) /* magnetic encoder failed */
#define ROBSTRIDE_FAULT_HALL_ENCODER (1u << 4) /* hall encoder failed */
#define ROBSTRIDE_FAULT_UNCALIBRATED (1u << 5) /* not calibrated */

/**
 * enum robstride_mode - mode state reported by a feedback frame
 * @ROBSTRIDE_MODE_RESET: reset mode
 * @ROBSTRIDE_MODE_CALI: calibration mode
 * @ROBSTRIDE_MODE_MOTOR: running mode
 */
typedef enum {
	ROBSTRIDE_MODE_RESET = 0,
	ROBSTRIDE_MODE_CALI = 1,
	ROBSTRIDE_MODE_MOTOR = 2,
} robstride_mode_t;

#define ROBSTRIDE_POS_RANGE 12.56637061f /* 4*pi */

/* Errors for functions returning int; CAN_ERR_* values are passed through. */
#define ROBSTRIDE_OK 0
#define ROBSTRIDE_ERR_PARAM (-1)

/**
 * enum robstride_model - motor model, which fixes the velocity, gain and
 * torque limits
 * @ROBSTRIDE_MODEL_RS00: +/-33 rad/s, +/-14 Nm, KP 500, KD 5
 * @ROBSTRIDE_MODEL_RS01: +/-44 rad/s, +/-17 Nm, KP 500, KD 5
 * @ROBSTRIDE_MODEL_RS02: +/-44 rad/s, +/-17 Nm, KP 500, KD 5
 * @ROBSTRIDE_MODEL_RS03: +/-20 rad/s, +/-60 Nm, KP 5000, KD 100
 * @ROBSTRIDE_MODEL_RS04: +/-15 rad/s, +/-120 Nm, KP 5000, KD 100
 * @ROBSTRIDE_MODEL_RS05: +/-50 rad/s, +/-5.5 Nm, KP 500, KD 5
 * @ROBSTRIDE_MODEL_RS06: +/-50 rad/s, +/-36 Nm, KP 5000, KD 100
 * @ROBSTRIDE_MODEL_COUNT: number of models, not a valid model
 */
typedef enum {
	ROBSTRIDE_MODEL_RS00 = 0,
	ROBSTRIDE_MODEL_RS01,
	ROBSTRIDE_MODEL_RS02,
	ROBSTRIDE_MODEL_RS03,
	ROBSTRIDE_MODEL_RS04,
	ROBSTRIDE_MODEL_RS05,
	ROBSTRIDE_MODEL_RS06,
	ROBSTRIDE_MODEL_COUNT,
} robstride_model_t;

/**
 * struct robstride_motor - one motor of the group
 * @id: motor ID, 1 to 255, unique within the group
 * @model: motor model, fixes the limits used to encode and decode its frames
 */
struct robstride_motor {
	uint8_t id;
	robstride_model_t model;
};

/**
 * struct robstride_cmd - command for one motor, built into a frame by
 * robstride_set_cmd()
 * @torque: target torque in Nm
 * @position: target position in radians, clamped to +/-ROBSTRIDE_POS_RANGE
 * @velocity: target angular velocity in rad/s
 * @kp: stiffness gain
 * @kd: damping gain
 *
 * Every value is clamped to the limits of the motor model before encoding.
 */
struct robstride_cmd {
	float torque;
	float position;
	float velocity;
	float kp;
	float kd;
};

/**
 * struct robstride_state - last feedback decoded for one motor
 * @valid: %true once a feedback frame has been decoded, %false before
 * @timestamp_ms: value of time_now_ms() when that frame was decoded
 * @faults: motor fault bitmask of that frame, see ROBSTRIDE_FAULT_*, 0 if the
 * motor reports no fault
 * @mode: motor mode state of that frame, see enum robstride_mode
 * @position: measured position in radians
 * @velocity: measured angular velocity in rad/s
 * @torque: measured torque in Nm
 * @temp: measured temperature in degrees Celsius
 *
 * @timestamp_ms is the poll time, not the arrival time of the frame, so it
 * trails the arrival by up to one polling period.
 */
struct robstride_state {
	bool valid;
	uint32_t timestamp_ms;
	uint8_t faults;
	robstride_mode_t mode;
	float position;
	float velocity;
	float torque;
	float temp;
};

/* Receive queue element, internal to the driver. */
struct robstride_frame {
	struct can_msg msg;
	uint32_t errors;
};

RINGBUF_DEFINE(robstride_rx, struct robstride_frame, ROBSTRIDE_RX_QUEUE_LEN);

/**
 * struct robstride - CAN bus carrying a group of RobStride motors
 * @can: CAN device the group is attached to, bound through robstride_init()
 * @motor_count: number of motors in the group, 1 to ROBSTRIDE_MAX_MOTORS
 * @motor: group definition, fixed by robstride_init()
 * @cmd: command group, written through robstride_set_cmd()
 * @state: state group, refreshed by robstride_process()
 * @pending: the frame waiting to be transmitted for each motor
 * @pending_mask: bit per motor, set while its @pending frame has not been sent
 * @rx: receive queue between the receive callback and robstride_process()
 * @rx_dropped: frames the receive callback could not queue because @rx was full
 * @rx_errors: bus-level event errors seen on decoded frames, see CAN_EVT_ERR_*
 * @rx_total: frames decoded into @state since robstride_init()
 *
 * @pending holds one frame per motor: building a frame for a motor replaces the
 * one still waiting for it, so robstride_enable() and robstride_disable()
 * discard control frames that have not been transmitted yet.
 *
 * Concurrency: the receive callback only touches @rx and @rx_dropped. Every
 * other member belongs to the task context that calls robstride_process() and
 * robstride_set_cmd().
 */
struct robstride {
	struct can_dev *can;
	uint8_t motor_count;
	struct robstride_motor motor[ROBSTRIDE_MAX_MOTORS];
	struct robstride_cmd cmd[ROBSTRIDE_MAX_MOTORS];
	struct robstride_state state[ROBSTRIDE_MAX_MOTORS];
	struct can_msg pending[ROBSTRIDE_MAX_MOTORS];
	uint32_t pending_mask;
	struct robstride_rx rx;
	volatile uint32_t rx_dropped;
	volatile uint32_t rx_errors;
	volatile uint32_t rx_total;
};

/**
 * robstride_init() - define the motor group of a bus instance
 * @bus: instance to initialize
 * @can: CAN device carrying the group, bound by board code
 * @motors: group definition, copied into @bus
 * @count: number of entries in @motors
 *
 * Context: task context, before robstride_start() and before any other call on
 * @bus.
 *
 * Validates the whole group once, so the runtime paths do not repeat the
 * checks. A rejected call leaves @bus unmodified.
 *
 * Return: ROBSTRIDE_OK on success, ROBSTRIDE_ERR_PARAM on failure.
 */
int robstride_init(struct robstride *bus, struct can_dev *can,
		   const struct robstride_motor *motors, uint8_t count);

/**
 * robstride_start() - start receiving feedback frames
 * @bus: initialized instance
 *
 * Context: task context, after robstride_init().
 *
 * Registers the receive subscription of the CAN device. Reception has no frame
 * boundary to re-arm, so this is called once, not per frame. The subscription
 * lasts until can_abort() is called on the bus; after can_abort(), and after a
 * passive abort reported by the callback (CAN_EVT_ERR_ABORTED or
 * CAN_EVT_ERR_BUS_OFF), reception has to be registered again through this
 * function.
 *
 * Return: CAN_OK on success, negative CAN_ERR_* value on failure.
 */
int robstride_start(struct robstride *bus);

/**
 * robstride_set_cmd() - store the command of one motor
 * @bus: initialized instance
 * @index: motor position in the group, below @bus->motor_count
 * @cmd: command to store, copied into @bus
 *
 * Context: task context.
 *
 * The frame goes out during the next robstride_process() call, or in this
 * iteration when the call follows this one.
 *
 * Return: ROBSTRIDE_OK on success, ROBSTRIDE_ERR_PARAM if @index is out of
 * range.
 */
int robstride_set_cmd(struct robstride *bus, uint8_t index,
		      const struct robstride_cmd *cmd);

/**
 * robstride_enable() - queue an enable frame for every motor of the group
 * @bus: initialized instance
 *
 * Context: task context.
 *
 * Return: nothing, the frames are queued unconditionally.
 */
void robstride_enable(struct robstride *bus);

/**
 * robstride_disable() - queue a disable frame for every motor of the group
 * @bus: initialized instance
 *
 * Context: task context.
 *
 * Return: nothing, the frames are queued unconditionally.
 */
void robstride_disable(struct robstride *bus);

/**
 * robstride_process() - transmit pending frames and decode received ones
 * @bus: initialized and started instance
 *
 * Context: task context, typically once per control loop iteration.
 *
 * Never blocks: pending frames are sent with a zero timeout, then the receive
 * queue is drained. Frames that are not eight bytes long, or that carry an ID
 * outside the group, are dropped. A returned count of 0 leaves the state group
 * as it is.
 *
 * Return: number of motor states refreshed by this call.
 */
int robstride_process(struct robstride *bus);

/**
 * robstride_state() - read the state of one motor
 * @bus: initialized instance
 * @index: motor position in the group, below @bus->motor_count
 *
 * Context: any context that serializes with robstride_process().
 *
 * Returned in place, so the caller must not hold the pointer across a
 * robstride_process() call it does not control.
 *
 * Return: pointer to the state, or NULL if @index is out of range.
 */
static inline const struct robstride_state *
robstride_state(const struct robstride *bus, uint8_t index)
{
	if (index >= bus->motor_count) {
		return NULL;
	}

	return &bus->state[index];
}

/**
 * robstride_feedback_motor_id() - motor ID carried by a feedback frame
 * @can_id: @id field of a received CAN frame
 *
 * Context: any context.
 *
 * Return: motor ID encoded in bits 15:8 of @can_id.
 */
static inline uint8_t robstride_feedback_motor_id(uint32_t can_id)
{
	return (uint8_t)((can_id >> 8) & 0xFFu);
}

#endif /* ROBSTRIDE_H */
