/* SPDX-License-Identifier: MIT */
#ifndef ADAPTER_CAN_HOST_MOCK_H
#define ADAPTER_CAN_HOST_MOCK_H

#include <stdio.h>

#include "subsystems/subsystems_can.h"

/**
 * can_host_mock_bind() - bind a stdio stream to a CAN device instance
 * @dev: CAN device to bind
 * @stream: stdio stream carrying the frames, see the record format below
 * @cfg: controller parameters, fixed for the lifetime of the binding
 *
 * Context: task context, typically during board initialization. Must be
 * called before the device is used. The adapter keeps a static slot per
 * binding; bindings beyond the available slots are silently ignored.
 *
 * The stream carries one 13-byte record per frame: the identifier as a
 * little-endian 32-bit value, the length code, then the eight payload bytes.
 * can_send() appends records and can_recv_async() consumes them.
 */
void can_host_mock_bind(struct can_dev *dev, FILE *stream,
			const struct can_config *cfg);

#endif /* ADAPTER_CAN_HOST_MOCK_H */
