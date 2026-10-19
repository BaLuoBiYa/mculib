/* SPDX-License-Identifier: MIT */
/*
 * RobStride motor group driver: one instance covers every motor on one CAN bus.
 *
 * The receive callback runs in interrupt context and queues each frame with its
 * arrival time; robstride_process() decodes the queued frames and transmits the
 * pending ones. A state holds the fault mask and mode state of the last
 * feedback frame; CAN event error bits are counted, never reported as motor
 * faults.
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
 * struct robstride_cmd - command for one motor
 * @torque: target torque in Nm
 * @position: target position in radians, clamped to +/-ROBSTRIDE_POS_RANGE
 * @velocity: target angular velocity in rad/s
 * @kp: stiffness gain
 * @kd: damping gain
 *
 * Every value is clamped to the limits of the motor model.
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
 * @timestamp_ms: arrival time of the frame, stamped by the receive callback
 * @faults: motor fault bitmask of that frame, see ROBSTRIDE_FAULT_*, 0 if the
 * motor reports no fault
 * @mode: motor mode state of that frame, see enum robstride_mode
 * @position: measured position in radians
 * @velocity: measured angular velocity in rad/s
 * @torque: measured torque in Nm
 * @temp: measured temperature in degrees Celsius
 */
struct robstride_state {
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
	uint32_t timestamp_ms;
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
 * A pending frame gets one transmission attempt and is dropped when the
 * controller refuses it. A frame built for a motor replaces the one already
 * waiting for it.
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
 * A rejected call leaves @bus unmodified.
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
 * Registers the receive subscription of the CAN device. Reception then runs
 * until can_abort(); after that, and after a passive abort reported by the
 * callback, this function has to be called again.
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
 * The frame goes out in the next robstride_process() call.
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
 */
void robstride_enable(struct robstride *bus);

/**
 * robstride_disable() - queue a disable frame for every motor of the group
 * @bus: initialized instance
 *
 * Context: task context.
 */
void robstride_disable(struct robstride *bus);

/**
 * robstride_process() - transmit pending frames and decode received ones
 * @bus: initialized and started instance
 *
 * Context: task context, typically once per control loop iteration.
 *
 * Pending frames get one transmission attempt each, then the receive queue is
 * drained. Frames that are not eight bytes long, or that carry an ID outside
 * the group, are dropped. A count of 0 leaves the state group as it is.
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
 * The pointer stays valid until the next robstride_process() call.
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
