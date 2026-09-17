/* SPDX-License-Identifier: MIT */

#include "app/inc/robstride.h"

#include <string.h>

#include "common/byteorder.h"
#include "common/time.h"

/*
 * Limits per model, indexed by model, in the order v_min, v_max, kp_min,
 * kp_max, kd_min, kd_max, t_min, t_max. robstride.h lists the values each row
 * holds.
 */
struct robstride_limits {
	float v_min;
	float v_max;
	float kp_min;
	float kp_max;
	float kd_min;
	float kd_max;
	float t_min;
	float t_max;
};

static const struct robstride_limits limits[] = {
	[ROBSTRIDE_MODEL_RS00] = { -33.0f, 33.0f, 0.0f, 500.0f, 0.0f, 5.0f,
				   -14.0f, 14.0f },
	[ROBSTRIDE_MODEL_RS01] = { -44.0f, 44.0f, 0.0f, 500.0f, 0.0f, 5.0f,
				   -17.0f, 17.0f },
	[ROBSTRIDE_MODEL_RS02] = { -44.0f, 44.0f, 0.0f, 500.0f, 0.0f, 5.0f,
				   -17.0f, 17.0f },
	[ROBSTRIDE_MODEL_RS03] = { -20.0f, 20.0f, 0.0f, 5000.0f, 0.0f, 100.0f,
				   -60.0f, 60.0f },
	[ROBSTRIDE_MODEL_RS04] = { -15.0f, 15.0f, 0.0f, 5000.0f, 0.0f, 100.0f,
				   -120.0f, 120.0f },
	[ROBSTRIDE_MODEL_RS05] = { -50.0f, 50.0f, 0.0f, 500.0f, 0.0f, 5.0f,
				   -5.5f, 5.5f },
	[ROBSTRIDE_MODEL_RS06] = { -50.0f, 50.0f, 0.0f, 5000.0f, 0.0f, 100.0f,
				   -36.0f, 36.0f },
};

_Static_assert(sizeof(limits) / sizeof(limits[0]) == ROBSTRIDE_MODEL_COUNT,
	       "the limit table must cover every model");

/* Only used to validate the group, the runtime paths index the table. */
static const struct robstride_limits *
robstride_limits_get(robstride_model_t model)
{
	if ((unsigned int)model >= (unsigned int)ROBSTRIDE_MODEL_COUNT) {
		return NULL;
	}

	return &limits[model];
}

/* Clamping and truncation match the reference driver. */
static uint16_t robstride_scale(float x, float x_min, float x_max)
{
	if (x < x_min) {
		x = x_min;
	}
	if (x > x_max) {
		x = x_max;
	}

	return (uint16_t)((x - x_min) / (x_max - x_min) * 65535.0f);
}

static float robstride_unscale(uint16_t x, float x_min, float x_max)
{
	return x_min + (((float)x * (x_max - x_min)) / 65535.0f);
}

/* CAN_EFF_FLAG marks the frame as extended, as the CAN contract asks. */
static uint32_t robstride_build_id(uint8_t id, uint16_t data, uint8_t cmd)
{
	return CAN_EFF_FLAG | ((uint32_t)(cmd & 0x1Fu) << 24) |
	       ((uint32_t)data << 8) | id;
}

/* The torque travels in the identifier, the rest in the payload. */
static void robstride_frame_control(const struct robstride_motor *motor,
				    const struct robstride_cmd *cmd,
				    struct can_msg *msg)
{
	const struct robstride_limits *lim = &limits[motor->model];
	uint16_t torque = robstride_scale(cmd->torque, lim->t_min, lim->t_max);

	*msg = (struct can_msg){
		.id = robstride_build_id(motor->id, torque,
					 ROBSTRIDE_CMD_CONTROL),
		.dlc = 8,
	};

	put_u16be(&msg->data[0],
		  robstride_scale(cmd->position, -ROBSTRIDE_POS_RANGE,
				  ROBSTRIDE_POS_RANGE));
	put_u16be(&msg->data[2],
		  robstride_scale(cmd->velocity, lim->v_min, lim->v_max));
	put_u16be(&msg->data[4],
		  robstride_scale(cmd->kp, lim->kp_min, lim->kp_max));
	put_u16be(&msg->data[6],
		  robstride_scale(cmd->kd, lim->kd_min, lim->kd_max));
}

static void robstride_queue(struct robstride *bus, uint8_t index,
			    const struct can_msg *msg)
{
	bus->pending[index] = *msg;
	bus->pending_mask |= 1u << index;
}

/* Enable and disable frames differ only in the command code. */
static void robstride_gate(struct robstride *bus, uint8_t cmd)
{
	uint8_t i;

	for (i = 0; i < bus->motor_count; i++) {
		struct can_msg msg = {
			.id = robstride_build_id(bus->motor[i].id,
						 ROBSTRIDE_MASTER_ID, cmd),
			.dlc = 8,
		};

		robstride_queue(bus, i, &msg);
	}
}

static int robstride_find(const struct robstride *bus, uint8_t id)
{
	uint8_t i;

	for (i = 0; i < bus->motor_count; i++) {
		if (bus->motor[i].id == id) {
			return (int)i;
		}
	}

	return -1;
}

/* A zero timeout: a refused frame stays pending for the next call. */
static void robstride_flush(struct robstride *bus)
{
	uint8_t i;

	for (i = 0; i < bus->motor_count; i++) {
		if ((bus->pending_mask & (1u << i)) == 0) {
			continue;
		}

		if (can_send(bus->can, &bus->pending[i], 0) == CAN_OK) {
			bus->pending_mask &= ~(1u << i);
		}
	}
}

static void robstride_apply(struct robstride *bus, uint8_t index,
			    const struct robstride_frame *frame,
			    uint32_t now_ms)
{
	const uint8_t *data = frame->msg.data;
	/* Validated by robstride_init(). */
	const struct robstride_limits *lim = &limits[bus->motor[index].model];
	struct robstride_state *state = &bus->state[index];
	uint16_t raw;

	raw = get_u16be(&data[0]);
	state->position = robstride_unscale(raw, -ROBSTRIDE_POS_RANGE,
					    ROBSTRIDE_POS_RANGE);
	raw = get_u16be(&data[2]);
	state->velocity = robstride_unscale(raw, lim->v_min, lim->v_max);
	raw = get_u16be(&data[4]);
	state->torque = robstride_unscale(raw, lim->t_min, lim->t_max);
	state->temp = (float)get_u16be(&data[6]) / 10.0f;
	/* The fault mask and the mode state are fields of the identifier. */
	state->faults = (uint8_t)((frame->msg.id >> 16) & 0x3Fu);
	state->mode = (robstride_mode_t)((frame->msg.id >> 22) & 0x03u);
	state->timestamp_ms = now_ms;
	state->valid = true;
}

/*
 * Receive callback, interrupt context: only queue the frame. A full queue
 * drops the newest frame instead of overwriting buffered ones, which is what
 * keeps the queue usable without a lock.
 */
static void robstride_can_rx_cb(struct can_dev *dev, const struct can_msg *msg,
				uint32_t errors, void *ctx)
{
	struct robstride *bus = ctx;
	struct robstride_frame frame = {
		.msg = {
			.id = msg->id,
			.dlc = msg->dlc,
		},
		.errors = errors,
	};

	(void)dev;

	memcpy(frame.msg.data, msg->data, sizeof(frame.msg.data));

	if (!robstride_rx_push(&bus->rx, frame)) {
		bus->rx_dropped++;
	}
}

int robstride_init(struct robstride *bus, struct can_dev *can,
		   const struct robstride_motor *motors, uint8_t count)
{
	uint8_t i;
	uint8_t j;

	if (count == 0 || count > ROBSTRIDE_MAX_MOTORS) {
		return ROBSTRIDE_ERR_PARAM;
	}

	for (i = 0; i < count; i++) {
		if (robstride_limits_get(motors[i].model) == NULL) {
			return ROBSTRIDE_ERR_PARAM;
		}

		for (j = 0; j < i; j++) {
			if (motors[j].id == motors[i].id) {
				return ROBSTRIDE_ERR_PARAM;
			}
		}
	}

	*bus = (struct robstride){
		.can = can,
		.motor_count = count,
	};

	for (i = 0; i < count; i++) {
		bus->motor[i] = motors[i];
	}

	robstride_rx_init(&bus->rx);

	return ROBSTRIDE_OK;
}

int robstride_start(struct robstride *bus)
{
	return can_recv_async(bus->can, robstride_can_rx_cb, bus);
}

int robstride_set_cmd(struct robstride *bus, uint8_t index,
		      const struct robstride_cmd *cmd)
{
	if (index >= bus->motor_count) {
		return ROBSTRIDE_ERR_PARAM;
	}

	bus->cmd[index] = *cmd;
	robstride_frame_control(&bus->motor[index], cmd, &bus->pending[index]);
	bus->pending_mask |= 1u << index;

	return ROBSTRIDE_OK;
}

void robstride_enable(struct robstride *bus)
{
	robstride_gate(bus, ROBSTRIDE_CMD_ENABLE);
}

void robstride_disable(struct robstride *bus)
{
	robstride_gate(bus, ROBSTRIDE_CMD_DISABLE);
}

int robstride_process(struct robstride *bus)
{
	struct robstride_frame frame;
	uint32_t now_ms = 0;
	bool have_now = false;
	int decoded = 0;

	/* Pending frames first, a command stored now still goes out. */
	robstride_flush(bus);

	while (robstride_rx_pop(&bus->rx, &frame)) {
		uint8_t id;
		int index;

		if (frame.msg.dlc != 8) {
			continue;
		}

		id = robstride_feedback_motor_id(frame.msg.id);
		index = robstride_find(bus, id);
		if (index < 0) {
			continue;
		}

		/* Read once per batch: an idle bus costs no clock read. */
		if (!have_now) {
			now_ms = time_now_ms();
			have_now = true;
		}

		robstride_apply(bus, (uint8_t)index, &frame, now_ms);

		/* Bus errors stay internal, they are not motor faults. */
		if (frame.errors != 0) {
			bus->rx_errors++;
		}

		bus->rx_total++;
		decoded++;
	}

	return decoded;
}
