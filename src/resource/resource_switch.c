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
#include <peripheral_io.h>
#include <camera.h>
#include "resource/resource_camera.h"

#define SWITCH_IN				27		// GPIO9
static peripheral_gpio_h g_gpio_h = NULL;

extern void resource_camera_capture_completed_cb(const void *image, unsigned int size, void *user_data);
extern int resource_camera_capture(capture_completed_cb capture_completed_cb, void *user_data);

static void interrupted_cb(peripheral_gpio_h gpio_h, peripheral_error_e error, void *user_data)
{
	int ret = PERIPHERAL_ERROR_NONE;

#if 1
	// disable interrupt callback
	ret = peripheral_gpio_set_edge_mode(gpio_h, PERIPHERAL_GPIO_EDGE_NONE);
	if (ret != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_gpio_set_edge_mode failed, ret=[%d]", ret);
	}
#endif

	uint32_t value;
	peripheral_gpio_read(gpio_h, &value);

	if (value == 1) {
		INFO("value : %d... starting camera capture", value);

		ret = resource_camera_capture(resource_camera_capture_completed_cb, NULL);
		if (ret < 0) {
			ERR("Failed to capture camera");
		}
	}

#if 1
	// enable interrupt again
	ret = peripheral_gpio_set_edge_mode(gpio_h, PERIPHERAL_GPIO_EDGE_RISING);
	if (ret != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_gpio_set_edge_mode failed, ret=[%d]", ret);
	}
#endif
}

int resource_switch_close(void)
{
	int ret = PERIPHERAL_ERROR_NONE;

	if (g_gpio_h == NULL)
		return ret;

	INFO("gpio is finishing...");
	ret = peripheral_gpio_unset_interrupted_cb(g_gpio_h);
	if (ret != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_gpio_unset_interrupted_cb failed");
	}

	ret = peripheral_gpio_close(g_gpio_h);
	if (ret != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_gpio_close failed");
	}
	return ret;
}

int resource_switch_open(void)
{
	int ret = PERIPHERAL_ERROR_NONE;

	INFO("gpio_pin:%d is opening...", SWITCH_IN);
	ret = peripheral_gpio_open(SWITCH_IN, &g_gpio_h);
	if (ret != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_gpio_open failed, ret=[%d]", ret);
	}

	ret = peripheral_gpio_set_direction(g_gpio_h, PERIPHERAL_GPIO_DIRECTION_IN);
	if (ret != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_gpio_set_direction failed, ret=[%d]", ret);
	}

	ret = peripheral_gpio_set_interrupted_cb(g_gpio_h, interrupted_cb, NULL);
	if (ret != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_gpio_set_interrupted_cb failed, ret=[%d]", ret);
	}

	ret = peripheral_gpio_set_edge_mode(g_gpio_h, PERIPHERAL_GPIO_EDGE_RISING);
	if (ret != PERIPHERAL_ERROR_NONE) {
		ERR("peripheral_gpio_set_edge_mode failed, ret=[%d]", ret);
	}

	return ret;
}
