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

#include <service_app.h>
#include "log.h"
#include <peripheral_io.h>
#include <unistd.h>

extern int resource_switch_close(void);
extern int resource_switch_open(void);
extern void resource_camera_close(void);

extern bool terminate_yield_thread;
extern int init_mqtt(void);

#define MAX_RETRY_COUNT	100

bool service_app_create(void *data)
{
	int ret = 0;

	INFO("service_app_create\n");

	ret = resource_switch_open();
	if (ret != 0 ) {
		ERR("open_led_dev() failed!![%d]", ret);
		return false;
	}

	int count = 0;
	while (count < MAX_RETRY_COUNT) {
		ret = init_mqtt();
		if (ret == 0) {
			INFO("mqtt init count [%d] success", count);
			return true;
		} else {
			count++;
			ERR("mqtt init count [%d] failed", count);
			sleep(2);
		}
	}

    return true;
}

void service_app_terminate(void *data)
{
	INFO("service_app_terminate\n");
	int ret = 0;

	terminate_yield_thread = true;

	ret = resource_switch_close();
	if (ret != 0 ) {
		ERR("resource_irtx_init() failed!![%d]", ret);
	}

	resource_camera_close();

	return;
}

void service_app_control(app_control_h app_control, void *data)
{
    // Todo: add your code here.
    return;
}

static void
service_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LANGUAGE_CHANGED*/
	return;
}

static void
service_app_region_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

static void
service_app_low_battery(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_BATTERY*/
}

static void
service_app_low_memory(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_MEMORY*/
}

int main(int argc, char* argv[])
{
    char ad[50] = {0,};
	service_app_lifecycle_callback_s event_callback;
	app_event_handler_h handlers[5] = {NULL, };

	event_callback.create = service_app_create;
	event_callback.terminate = service_app_terminate;
	event_callback.app_control = service_app_control;

	service_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, service_app_low_battery, &ad);
	service_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, service_app_low_memory, &ad);
	service_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, service_app_lang_changed, &ad);
	service_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED], APP_EVENT_REGION_FORMAT_CHANGED, service_app_region_changed, &ad);

	return service_app_main(argc, argv, &event_callback, ad);
}
