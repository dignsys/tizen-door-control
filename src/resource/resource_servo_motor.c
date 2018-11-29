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

#include "log.h"
#include "resource/resource_servo_motor.h"

#define ARTIK_PWM_CHIPID	0
#define ARTIK_PWM_PIN		2

#define MILLI_SECOND	(1000000)

peripheral_pwm_h g_pwm_h = NULL;

static int _get_duty_cycle(door_state_e mode, int *duty_cycle)
{
	if ((mode == DOOR_STATE_OPENED) || (mode == DOOR_STATE_OPENING))
		*duty_cycle = 2 * MILLI_SECOND;		// CLOCKWISE
	else if ((mode == DOOR_STATE_CLOSE) || (mode == DOOR_STATE_CLOSING))
		*duty_cycle = 1 * MILLI_SECOND;		// COUNTER-CLOCKWISE
	else {
		ERR("get_duty_cycle unknown mode=[%d]", mode);
		return -1;
	}

	INFO("get_duty_cycle mode=[%d:%s] : duty_cycle [%d] ms",
							mode,
							mode == DOOR_STATE_OPENED ? "OPENED " :
							(mode == DOOR_STATE_CLOSE ? "CLOSE  " :
							(mode == DOOR_STATE_CLOSING ? "CLOSING" : "OPENING")),
							*duty_cycle/(1000));

	return 0;
}
peripheral_error_e resource_motor_driving(door_state_e mode)
{
	peripheral_error_e ret = PERIPHERAL_ERROR_NONE;

	int chip = ARTIK_PWM_CHIPID;	// Chip 0
	int pin  = ARTIK_PWM_PIN;		// Pin 2

	int period = 20 * MILLI_SECOND;
	int duty_cycle = 0;
	bool enable = true;

	ret = _get_duty_cycle(mode, &duty_cycle);
	if (ret != 0) {
		ERR("_get_duty_cycle unknown mode=[%d]", mode);
		return PERIPHERAL_ERROR_INVALID_PARAMETER;
	}

	if (g_pwm_h == NULL){
		// Opening a PWM Handle : The chip and pin parameters required for this function must be set
		if ((ret = peripheral_pwm_open(chip, pin, &g_pwm_h)) != PERIPHERAL_ERROR_NONE ) {
			ERR("peripheral_pwm_open() failed!![%d]", ret);
			return ret;
		}
	}

	// Setting the Period : sets the period to 20 milliseconds. The unit is nanoseconds
	if ((ret = peripheral_pwm_set_period(g_pwm_h, period)) != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_pwm_set_period() failed!![%d]", ret);
		return ret;
	}

	// Setting the Duty Cycle : sets the duty cycle to 1~2 milliseconds. The unit is nanoseconds
	if ((ret = peripheral_pwm_set_duty_cycle(g_pwm_h, duty_cycle)) != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_pwm_set_duty_cycle() failed!![%d]", ret);
		return ret;
	}

	// Enabling Repetition
	if ((ret = peripheral_pwm_set_enabled(g_pwm_h, enable)) != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_pwm_set_enabled() failed!![%d]", ret);
		return ret;
	}

	if (g_pwm_h != NULL) {
		// Closing a PWM Handle : close a PWM handle that is no longer used,
		if ((ret = peripheral_pwm_close(g_pwm_h)) != PERIPHERAL_ERROR_NONE ) {
			ERR("peripheral_pwm_close() failed!![%d]", ret);
			return ret;
		}
		g_pwm_h = NULL;
	}

	return ret;
}
