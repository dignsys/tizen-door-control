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

#include <camera.h>
#include <stdlib.h>
#include <pthread.h>

#include "log.h"
#include "resource/resource_camera.h"

#define DEFAULT_CAMERA_IMAGE_WIDTH 640 //int
#define DEFAULT_CAMERA_IMAGE_HEIGHT 480 //int
#define DEFAULT_CAMERA_IMAGE_QUALITY 100 //1~100

struct __camera_data {
	camera_h cam_handle;

	void *captured_file;
	unsigned int image_size;

	capture_completed_cb capture_completed_cb;
	void *capture_completed_cb_data;

	bool is_capturing;
	bool is_focusing;
	bool is_af_enabled;
};

static struct __camera_data *camera_data = NULL;

static const char * __cam_err_to_str(camera_error_e err)
{
	const char *err_str;

	switch (err) {
	case CAMERA_ERROR_NONE:
		err_str = "CAMERA_ERROR_NONE";
		break;
	case CAMERA_ERROR_INVALID_PARAMETER:
		err_str = "CAMERA_ERROR_INVALID_PARAMETER";
		break;
	case CAMERA_ERROR_INVALID_STATE:
		err_str = "CAMERA_ERROR_INVALID_STATE";
		break;
	case CAMERA_ERROR_OUT_OF_MEMORY:
		err_str = "CAMERA_ERROR_OUT_OF_MEMORY";
		break;
	case CAMERA_ERROR_DEVICE:
		err_str = "CAMERA_ERROR_DEVICE";
		break;
	case CAMERA_ERROR_INVALID_OPERATION:
		err_str = "CAMERA_ERROR_INVALID_OPERATION";
		break;
	case CAMERA_ERROR_SOUND_POLICY:
		err_str = "CAMERA_ERROR_SOUND_POLICY";
		break;
	case CAMERA_ERROR_SECURITY_RESTRICTED:
		err_str = "CAMERA_ERROR_SECURITY_RESTRICTED";
		break;
	case CAMERA_ERROR_DEVICE_BUSY:
		err_str = "CAMERA_ERROR_DEVICE_BUSY";
		break;
	case CAMERA_ERROR_DEVICE_NOT_FOUND:
		err_str = "CAMERA_ERROR_DEVICE_NOT_FOUND";
		break;
	case CAMERA_ERROR_SOUND_POLICY_BY_CALL:
		err_str = "CAMERA_ERROR_SOUND_POLICY_BY_CALL";
		break;
	case CAMERA_ERROR_SOUND_POLICY_BY_ALARM:
		err_str = "CAMERA_ERROR_SOUND_POLICY_BY_ALARM";
		break;
	case CAMERA_ERROR_ESD:
		err_str = "CAMERA_ERROR_ESD";
		break;
	case CAMERA_ERROR_PERMISSION_DENIED:
		err_str = "CAMERA_ERROR_PERMISSION_DENIED";
		break;
	case CAMERA_ERROR_NOT_SUPPORTED:
		err_str = "CAMERA_ERROR_NOT_SUPPORTED";
		break;
	case CAMERA_ERROR_RESOURCE_CONFLICT:
		err_str = "CAMERA_ERROR_RESOURCE_CONFLICT";
		break;
	case CAMERA_ERROR_SERVICE_DISCONNECTED:
		err_str = "CAMERA_ERROR_SERVICE_DISCONNECTED";
		break;
	default:
		err_str = "Unknown";
		break;
	}
	return err_str;
}

static void __print_thread_id(char *msg)
{
//    pthread_t id;
//    id = pthread_self();
//    DBG("[%s] tid %u", msg,  (unsigned int)id);
}

static void __print_camera_state(camera_state_e previous, camera_state_e current, bool by_policy, void *user_data)
{
	switch (current) {
	case CAMERA_STATE_CREATED:
		DBG("Camera state: CAMERA_STATE_CREATED");
		break;

	case CAMERA_STATE_PREVIEW:
		DBG("Camera state: CAMERA_STATE_PREVIEW");
		break;

	case CAMERA_STATE_CAPTURING:
		DBG("Camera state: CAMERA_STATE_CAPTURING");
		break;

	case CAMERA_STATE_CAPTURED:
		DBG("Camera state: CAMERA_STATE_CAPTURED");
		break;

	default:
		DBG("Camera state: CAMERA_STATE_NONE");
		break;
	}
}

static bool __camera_attr_supported_af_mode_cb(camera_attr_af_mode_e mode, void *user_data)
{
	struct __camera_data *camera_data = user_data;

	INFO("Auto-Focus Mode [%d]", mode);

	if (mode != CAMERA_ATTR_AF_NONE)
		camera_data->is_af_enabled = true;

	return true;
}

static void __capturing_cb(camera_image_data_s *image, camera_image_data_s *postview, camera_image_data_s *thumbnail, void *user_data)
{
	struct __camera_data *camera_data = user_data;
	if (image == NULL) {
		ERR("Image is NULL");
		return;
	}

	__print_thread_id("CAPTURING");

	free(camera_data->captured_file);
	camera_data->captured_file = malloc(image->size);
	if (camera_data->captured_file == NULL)
		return;

	DBG("Now is on Capturing: Image size[%d x %d]", image->width, image->height);

	memcpy(camera_data->captured_file, image->data, image->size);
	camera_data->image_size = image->size;

	return;
}

static void __completed_cb(void *user_data)
{
	int ret = 0;
	struct __camera_data *camera_data = user_data;

	DBG("Capture is completed");

	if (camera_data->capture_completed_cb)
		camera_data->capture_completed_cb(camera_data->captured_file, camera_data->image_size, camera_data->capture_completed_cb_data);

	camera_data->capture_completed_cb = NULL;
	free(camera_data->captured_file);
	camera_data->captured_file = NULL;

	if (!camera_data->cam_handle) {
		ERR("Camera is NULL");
		return;
	}

	ret = camera_start_preview(camera_data->cam_handle);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to start preview [%s]", __cam_err_to_str(ret));
		return;
	}

	ret = camera_stop_preview(camera_data->cam_handle);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to stop preview [%s]", __cam_err_to_str(ret));
		return;
	}

    if (camera_data->is_focusing)
    	camera_data->is_focusing = false;

    if (camera_data->is_capturing)
    	camera_data->is_capturing = false;

    return;
}

static void __start_capture(void *user_data)
{
	int ret = 0;
	struct __camera_data *camera_data = user_data;

	ret = camera_start_capture(camera_data->cam_handle, __capturing_cb, __completed_cb, camera_data);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to start capturing [%s]", __cam_err_to_str(ret));
		camera_data->is_focusing = false;
		return;
	}

	camera_data->is_capturing = true;
}

static void __camera_focus_cb(camera_focus_state_e state, void *user_data)
{
	struct __camera_data *camera_data = user_data;
	DBG("Camera focus state: [%d]", state);

	if (camera_data->is_af_enabled && state == CAMERA_FOCUS_STATE_FOCUSED && !camera_data->is_capturing) {
		__start_capture(camera_data);
	}
}

static void __camera_preview_cb(camera_preview_data_s *frame, void *user_data)
{
    __print_thread_id("PREVIEW Callback");
}

static int __init(void)
{
	int ret = CAMERA_ERROR_NONE;

	camera_data = malloc(sizeof(struct __camera_data));
	if (camera_data == NULL) {
		ERR("Failed to allocate Camera data");
		return -1;
	}
	memset(camera_data, 0, sizeof(struct __camera_data));

	ret = camera_create(CAMERA_DEVICE_CAMERA0, &(camera_data->cam_handle));
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to create camera [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_attr_set_image_quality(camera_data->cam_handle, DEFAULT_CAMERA_IMAGE_QUALITY);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to set image quality [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_preview_resolution(camera_data->cam_handle, DEFAULT_CAMERA_IMAGE_WIDTH, DEFAULT_CAMERA_IMAGE_HEIGHT);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to set preview resolution [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_capture_resolution(camera_data->cam_handle, DEFAULT_CAMERA_IMAGE_WIDTH, DEFAULT_CAMERA_IMAGE_HEIGHT);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to set capture resolution [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_capture_format(camera_data->cam_handle, CAMERA_PIXEL_FORMAT_JPEG);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to set capture format [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_state_changed_cb(camera_data->cam_handle, __print_camera_state, NULL);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to set state changed callback [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_preview_cb(camera_data->cam_handle, __camera_preview_cb, camera_data);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to set preview callback [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_set_focus_changed_cb(camera_data->cam_handle, __camera_focus_cb, camera_data);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to set focus changed callback [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	ret = camera_attr_foreach_supported_af_mode(camera_data->cam_handle, __camera_attr_supported_af_mode_cb, camera_data);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to set auto focus attribute check callback [%s]", __cam_err_to_str(ret));
		goto ERROR;
	}

	return 0;

ERROR:
	if (camera_data->cam_handle)
		camera_destroy(camera_data->cam_handle);

	free(camera_data);
	camera_data = NULL;
	return -1;
}

int resource_camera_capture(capture_completed_cb capture_completed_cb, void *user_data)
{
	camera_state_e state;
	int ret = CAMERA_ERROR_NONE;

	if (camera_data == NULL) {
		INFO("Camera is not initialized");
		ret = __init();
		if (ret < 0) {
			ERR("Failed to initialize camera");
			return -1;
		}
	}

	ret = camera_get_state(camera_data->cam_handle, &state);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to get camera state [%s]", __cam_err_to_str(ret));
		return -1;
	}

	if (state == CAMERA_STATE_CAPTURING) {
		DBG("Camera is now capturing");
		return -1;
	}

	if (state == CAMERA_STATE_PREVIEW) {
		INFO("Capturing is not completed");
		ret = camera_stop_preview(camera_data->cam_handle);
		if (ret != CAMERA_ERROR_NONE) {
			ERR("Failed to stop preview [%s]", __cam_err_to_str(ret));
			return -1;
		}
		camera_data->is_capturing = false;
		camera_data->is_focusing = false;
	}

	camera_data->capture_completed_cb = capture_completed_cb;
	camera_data->capture_completed_cb_data = user_data;

	ret = camera_start_preview(camera_data->cam_handle);
	if (ret != CAMERA_ERROR_NONE) {
		ERR("Failed to start preview [%s]", __cam_err_to_str(ret));
	} else {
		INFO("Success camera_start_preview [%s]", __cam_err_to_str(ret));
	}

	if (!camera_data->is_af_enabled) {
		__start_capture(camera_data);
	} else {
		ret = camera_start_focusing(camera_data->cam_handle, true);

		if (ret == CAMERA_ERROR_NOT_SUPPORTED) {
			camera_data->is_af_enabled = false;
			return -1;
		}

		camera_data->is_focusing = true;
	}

	return 0;
}

void resource_camera_close(void)
{
	if (camera_data == NULL)
		return;

	camera_stop_preview(camera_data->cam_handle);

	camera_destroy(camera_data->cam_handle);
	camera_data->cam_handle = NULL;

	free(camera_data->captured_file);
	camera_data->captured_file = NULL;

	free(camera_data);
	camera_data = NULL;
}
