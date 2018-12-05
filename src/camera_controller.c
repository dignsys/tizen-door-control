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

#include <glib.h>
#include <Ecore.h>
#include <tizen.h>
#include <service_app.h>
#include "log.h"
#include "resource/resource_camera.h"
//#include "s3_config.h"

#define IMAGE_FILE_PREFIX "CAM_"
#define IMAGE_FILE_POSTFIX "doorcamera.jpg"

// AWS S3 bucket name  -------------------------------------------------------
char *S3_BUCKET_NAME = "<YOUR BUCKET NAME>";

typedef struct app_data_s {
	Ecore_Timer *event_timer;
} app_data;

extern int simple_put_object(char* bucketName, char *key, char *filename);

static long long int __get_monotonic_ms(void)
{
	long long int ret_time = 0;
	struct timespec time_s;

	if (0 == clock_gettime(CLOCK_MONOTONIC, &time_s))
		ret_time = time_s.tv_sec* 1000 + time_s.tv_nsec / 1000000;
	else
		ERR("Failed to get ms");

	return ret_time;
}

static int __image_data_to_file(const char *filename, 	const void *image_data, unsigned int size)
{
	FILE *fp = NULL;
	char *data_path = NULL;
	char file[PATH_MAX] = {'\0', };

	data_path = app_get_data_path();

	snprintf(file, PATH_MAX, "%s%s", data_path, filename);
	free(data_path);
	data_path = NULL;

	DBG("File : %s", file);

	fp = fopen(file, "w");
	if (!fp) {
		ERR("Failed to open file: %s", file);
		return -1;
	}

	if (fwrite(image_data, size, 1, fp) != 1) {
		ERR("Failed to write image to file : %s", file);
		fclose(fp);
		return -1;
	}

	fclose(fp);

	return 0;
}

void resource_camera_capture_completed_cb(const void *image, unsigned int size, void *user_data)
{
	char filename[PATH_MAX] = {'\0', };
	int ret = 0;

	snprintf(filename, PATH_MAX, "%s%s", IMAGE_FILE_PREFIX, IMAGE_FILE_POSTFIX);

	ret = __image_data_to_file(filename, image, size);
	if (ret != 0) {
		ERR("__image_data_to_file : error\n");
		return;
	} else {
		INFO("image [%s] saved...", filename);

		ret = simple_put_object(S3_BUCKET_NAME, filename, filename);
		if (ret != 0) {
			ERR("__image_data_to_file : error\n");
			return;
		}
	}
}
