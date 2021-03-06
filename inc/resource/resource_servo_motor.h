/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RESOURCE_SERVO_MOTOR_H_
#define RESOURCE_SERVO_MOTOR_H_

#include <peripheral_io.h>

typedef enum {
	DOOR_STATE_CLOSE = 0,
	DOOR_STATE_CLOSING,
	DOOR_STATE_OPENING,
	DOOR_STATE_OPENED,
	DOOR_STATE_UNKNOWN,
} door_state_e;

#endif /* RESOURCE_SERVO_MOTOR_H_ */
