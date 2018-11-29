/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file mqtt_main.c
 * @brief simple MQTT publish and subscribe on the same topic using the SDK as a library
 *
 * This example takes the parameters from the aws_iot_config.h file and establishes a connection to the AWS IoT MQTT Platform.
 * It subscribes and publishes to the same topic - "tizen/sub"
 *
 * If all the certs are correct, you should see the messages received by the application in a loop.
 *
 * The application takes in the certificate path, host name , port and the number of times the publish should happen.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <pthread.h>
#include <service_app.h>

#include "aws_iot_config.h"
#include "sdk/aws_iot_log.h"
#include "sdk/aws_iot_version.h"
#include "sdk/aws_iot_mqtt_client_interface.h"

#include <peripheral_io.h>
#include "resource/resource_servo_motor.h"

#define TIMER_EVENT_INTERVAL	(2.0f)	// event timer : 2 seconds interval

#define HOST_ADDRESS_SIZE 255

/* Max number of initial connect retries */
#define CONNECT_MAX_ATTEMPT_COUNT 3

/* Interval that each thread sleeps for */
#define THREAD_SLEEP_INTERVAL_USEC 500000

#define timersub(a, b, result) \
  do { \
      (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
      (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
      if ((result)->tv_usec < 0) { \
         --(result)->tv_sec; \
         (result)->tv_usec += 1000000; \
     } \
  } while (0)

/**
 * @brief Topic name
 */
const char *TOPIC_SUB = "tizen/cmd";
const char *TOPIC_PUB = "tizen/notify";

/**
 * @brief Default cert location
 */
char certDirectory[PATH_MAX + 1] = "certs";

/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[HOST_ADDRESS_SIZE] = AWS_IOT_MQTT_HOST;

/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;

/**
 * @brief This parameter will avoid infinite loop of publish and exit the program after certain number of publishes
 */
uint32_t publishCount = 0;

AWS_IoT_Client client;

//pthread_t p_thread;
bool terminate_yield_thread;
pthread_t yield_thread;
bool mqtt_initalized = false;

extern peripheral_error_e resource_motor_driving(door_state_e mode);


bool process_command(int length, char *cmd)
{
	int ret = 0;

	if (strcmp(cmd, "DOOR_OPEN") == 0)
	{
		// open door
		INFO("Opening Door\n");
		ret = resource_motor_driving(DOOR_STATE_OPENING);
		if (ret != 0) {
			ERR("error resource_motor_driving : ret = %d", ret);
			return false;
		}
	} else if (strcmp(cmd, "DOOR_CLOSE") == 0) {
		// close door
		INFO("Closing Door\n");
		ret = resource_motor_driving(DOOR_STATE_CLOSING);
		if (ret != 0) {
			ERR("error resource_motor_driving : ret = %d", ret);
			return false;
		}
	} else {
		INFO("unknown cmd : %s", cmd);
	}

	return true;
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
									IoT_Publish_Message_Params *params, void *pData) {
	IOT_UNUSED(pData);
	IOT_UNUSED(pClient);
	IOT_INFO("Subscribe callback : %.*s\t%.*s", topicNameLen, topicName, (int)params->payloadLen, (char *)params->payload);

	int length = (int)params->payloadLen;
	char cmd[30];
	memset(cmd, 0, 30);
	strncpy(cmd, (char *)params->payload, length);

	if (length > 0) {
		INFO("[%d : %s] ", length, cmd);
		if(process_command(length, cmd) == false) {
			ERR("cmd [%s] send failed", cmd);
		}
	}
}

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
	IOT_WARN("MQTT Disconnect");
	IoT_Error_t rc = FAILURE;

	if(NULL == pClient) {
		return;
	}

	IOT_UNUSED(data);

	if(aws_iot_is_autoreconnect_enabled(pClient)) {
		IOT_INFO("Auto Reconnect is enabled, Reconnecting attempt will start now");
	} else {
		IOT_WARN("Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect(pClient);
		if(NETWORK_RECONNECTED == rc) {
			IOT_WARN("Manual Reconnect Successful");
		} else {
			IOT_WARN("Manual Reconnect Failed - %d", rc);
		}
	}
}

int notify_mqtt(char *url)
{
	IoT_Publish_Message_Params paramsQOS0;
	IoT_Error_t rc = FAILURE;

	if (mqtt_initalized != true) {
		INFO("MQTT not inialized\n");
		return 0;
	}

	INFO("url : %s", url);

	paramsQOS0.qos = QOS0;
	paramsQOS0.payload = (void *) url;
	paramsQOS0.isRetained = 0;

	paramsQOS0.payloadLen = strlen(url);

	INFO("publish %d : %s", paramsQOS0.payloadLen, paramsQOS0.payload);
	rc = aws_iot_mqtt_publish(&client, TOPIC_PUB, strlen(TOPIC_PUB), &paramsQOS0);
	if (rc != SUCCESS) {
		ERR("QOS0 publish failed [%d].\n", rc);
	}

	return rc;
}

static void *aws_iot_mqtt_yield_thread_runner(void *ptr)
{
	IoT_Error_t rc = SUCCESS;
	AWS_IoT_Client *pClient = (AWS_IoT_Client *) ptr;
	while(SUCCESS == rc && terminate_yield_thread == false) {
		do {
			usleep(THREAD_SLEEP_INTERVAL_USEC);
			rc = aws_iot_mqtt_yield(pClient, 100);
		} while(MQTT_CLIENT_NOT_IDLE_ERROR == rc); // Client is busy, wait to get lock

		if(SUCCESS != rc) {
			IOT_DEBUG("Yield Returned : %d\n", rc);
		}
	}
	IOT_DEBUG("Yield Thread Runner terminating  rc : %d, terminate_yield_thread : %d\n", rc, terminate_yield_thread);

	return NULL;
}

int init_mqtt(void)
{
	char rootCA[PATH_MAX + 1];
	char clientCRT[PATH_MAX + 1];
	char clientKey[PATH_MAX + 1];
	IoT_Error_t rc = FAILURE;
	char *app_cert_path = NULL;
	struct timeval connectTime;
	struct timeval start, end;
	unsigned int connectCounter = 0;
	int yieldThreadReturn = 0;

	terminate_yield_thread = false;

	//AWS_IoT_Client client;
	IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
	IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

	IOT_INFO("AWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	app_cert_path = app_get_resource_path();
	if (!app_cert_path) {
		ERR("app_cert_path is NULL!!");
		return -1;
	}

	snprintf(rootCA, PATH_MAX + 1, "%s%s/%s", app_cert_path, certDirectory, AWS_IOT_ROOT_CA_FILENAME);
	snprintf(clientCRT, PATH_MAX + 1, "%s%s/%s", app_cert_path, certDirectory, AWS_IOT_CERTIFICATE_FILENAME);
	snprintf(clientKey, PATH_MAX + 1, "%s%s/%s", app_cert_path, certDirectory, AWS_IOT_PRIVATE_KEY_FILENAME);
	free(app_cert_path);

	IOT_DEBUG("rootCA %s", rootCA);
	IOT_DEBUG("clientCRT %s", clientCRT);
	IOT_DEBUG("clientKey %s", clientKey);
	mqttInitParams.enableAutoReconnect = false; // We enable this later below
	mqttInitParams.pHostURL = HostAddress;
	mqttInitParams.port = port;
	mqttInitParams.pRootCALocation = rootCA;
	mqttInitParams.pDeviceCertLocation = clientCRT;
	mqttInitParams.pDevicePrivateKeyLocation = clientKey;
	mqttInitParams.mqttCommandTimeout_ms = 20000;
	mqttInitParams.tlsHandshakeTimeout_ms = 5000;
	mqttInitParams.isSSLHostnameVerify = true;
	mqttInitParams.disconnectHandler = disconnectCallbackHandler;
	mqttInitParams.disconnectHandlerData = NULL;

	rc = aws_iot_mqtt_init(&client, &mqttInitParams);
	if(SUCCESS != rc) {
		IOT_ERROR("aws_iot_mqtt_init returned error : %d ", rc);
		return rc;
	}

	connectParams.keepAliveIntervalInSec = 600;
	connectParams.isCleanSession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	connectParams.pClientID = AWS_IOT_MQTT_CLIENT_ID;
	connectParams.clientIDLen = (uint16_t) strlen(AWS_IOT_MQTT_CLIENT_ID);
	connectParams.isWillMsgPresent = false;

	IOT_DEBUG("Connecting Client\n");
	do {
		gettimeofday(&start, NULL);
		rc = aws_iot_mqtt_connect(&client, &connectParams);
		gettimeofday(&end, NULL);
		timersub(&end, &start, &connectTime);

		connectCounter++;
	} while(rc != SUCCESS && connectCounter < CONNECT_MAX_ATTEMPT_COUNT);

	if(SUCCESS == rc) {
		IOT_DEBUG("## Connect Success. Time sec: %d, usec: %d\n", connectTime.tv_sec, connectTime.tv_usec);
	} else {
		IOT_ERROR("## Connect Failed. error code %d\n", rc);
		return -1;
	}

	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
	if(SUCCESS != rc) {
		IOT_ERROR("Unable to set Auto Reconnect to true - %d\n", rc);
		return rc;
	}

	IOT_INFO("Subscribing...");
	rc = aws_iot_mqtt_subscribe(&client, TOPIC_SUB, strlen(TOPIC_SUB), QOS0, iot_subscribe_callback_handler, NULL);
	if(SUCCESS != rc) {
		IOT_ERROR("Error subscribing : %d\n", rc);
		return rc;
	} else {
		mqtt_initalized = true;
		INFO("OK\n");
	}

#if 0
	rc = aws_iot_mqtt_subscribe(&client, TOPIC_SUB, strlen(TOPIC_SUB), QOS1, iot_subscribe_callback_handler, NULL);
	if(SUCCESS != rc) {
		IOT_ERROR("Error subscribing : %d\n", rc);
		return rc;
	} else {
		mqtt_initalized = true;
		INFO("OK\n");
	}
#endif

	yieldThreadReturn = pthread_create(&yield_thread, NULL, aws_iot_mqtt_yield_thread_runner, &client);
	if(SUCCESS != yieldThreadReturn) {
		IOT_ERROR("An error occurred pthread_create.\n");
	} else {
		IOT_INFO("pthread_create - yield_thread done\n");
	}

	if(SUCCESS != rc) {
		IOT_ERROR("An error occurred in the init_mqtt.\n");
	} else {
		IOT_INFO("init_mqtt done\n");
	}

	return rc;
}

#if 0
int mqtt_main(void)
{
	bool infinitePublishFlag = true;

	char rootCA[PATH_MAX + 1];
	char clientCRT[PATH_MAX + 1];
	char clientKey[PATH_MAX + 1];
	char cPayload[100];

	int32_t i = 0;

	IoT_Error_t rc = FAILURE;
	char *app_cert_path = NULL;

	//AWS_IoT_Client client;
	IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
	IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

	IoT_Publish_Message_Params paramsQOS0;
	IoT_Publish_Message_Params paramsQOS1;

	IOT_INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	app_cert_path = app_get_resource_path();
	if (!app_cert_path) {
		ERR("app_cert_path is NULL!!");
		return -1;
	}

	snprintf(rootCA, PATH_MAX + 1, "%s%s/%s", app_cert_path, certDirectory, AWS_IOT_ROOT_CA_FILENAME);
	snprintf(clientCRT, PATH_MAX + 1, "%s%s/%s", app_cert_path, certDirectory, AWS_IOT_CERTIFICATE_FILENAME);
	snprintf(clientKey, PATH_MAX + 1, "%s%s/%s", app_cert_path, certDirectory, AWS_IOT_PRIVATE_KEY_FILENAME);

	free(app_cert_path);

	IOT_DEBUG("rootCA %s", rootCA);
	IOT_DEBUG("clientCRT %s", clientCRT);
	IOT_DEBUG("clientKey %s", clientKey);
	mqttInitParams.enableAutoReconnect = false; // We enable this later below
	mqttInitParams.pHostURL = HostAddress;
	mqttInitParams.port = port;
	mqttInitParams.pRootCALocation = rootCA;
	mqttInitParams.pDeviceCertLocation = clientCRT;
	mqttInitParams.pDevicePrivateKeyLocation = clientKey;
	mqttInitParams.mqttCommandTimeout_ms = 20000;
	mqttInitParams.tlsHandshakeTimeout_ms = 5000;
	mqttInitParams.isSSLHostnameVerify = true;
	mqttInitParams.disconnectHandler = disconnectCallbackHandler;
	mqttInitParams.disconnectHandlerData = NULL;

	rc = aws_iot_mqtt_init(&client, &mqttInitParams);
	if(SUCCESS != rc) {
		IOT_ERROR("aws_iot_mqtt_init returned error : %d ", rc);
		return rc;
	}

	connectParams.keepAliveIntervalInSec = 600;
	connectParams.isCleanSession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	connectParams.pClientID = AWS_IOT_MQTT_CLIENT_ID;
	connectParams.clientIDLen = (uint16_t) strlen(AWS_IOT_MQTT_CLIENT_ID);
	connectParams.isWillMsgPresent = false;

	IOT_INFO("Connecting...");
	rc = aws_iot_mqtt_connect(&client, &connectParams);
	if(SUCCESS != rc) {
		IOT_ERROR("Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
		return rc;
	}
	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
	if(SUCCESS != rc) {
		IOT_ERROR("Unable to set Auto Reconnect to true - %d", rc);
		return rc;
	}

	IOT_INFO("Subscribing...");
	rc = aws_iot_mqtt_subscribe(&client, TOPIC_SUB, strlen(TOPIC_SUB), QOS0, iot_subscribe_callback_handler, NULL);
	if(SUCCESS != rc) {
		IOT_ERROR("Error subscribing : %d ", rc);
		return rc;
	}

	sprintf(cPayload, "%s : %d ", "hello from SDK", i);

	paramsQOS0.qos = QOS0;
	paramsQOS0.payload = (void *) cPayload;
	paramsQOS0.isRetained = 0;

	paramsQOS1.qos = QOS1;
	paramsQOS1.payload = (void *) cPayload;
	paramsQOS1.isRetained = 0;

	if(publishCount != 0) {
		infinitePublishFlag = false;
	}

	while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)
		  && (publishCount > 0 || infinitePublishFlag)) {

		//Max time the yield function will wait for read messages
		rc = aws_iot_mqtt_yield(&client, 100);
		if(NETWORK_ATTEMPTING_RECONNECT == rc) {
			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}

		usleep(100);
	}

	if(SUCCESS != rc) {
		IOT_ERROR("An error occurred in the loop.\n");
	} else {
		IOT_INFO("Publish done\n");
	}

	return rc;
}
#endif

#if 0
void *thread_main(void)
{
	int rc;
	bool connected = false;

	while (!connected) {
		rc = mqtt_main();
		if(SUCCESS != rc) {
			IOT_ERROR("mqtt_main error occurred in the loop....try again\n");
			sleep(60);
		} else {
			connected = true;
		}
	}
	IOT_INFO("mqtt_main done\n");
	return NULL;
}

bool terminate_main_thread(void)
{
	if (pthread_cancel(p_thread))
		return false;
	else
		return true;
}

bool launch_main_thread(void)
{
	int thread_id;

	thread_id = pthread_create(&p_thread, NULL, (void *)thread_main, NULL);
    if (thread_id < 0) {
        ERR("thread create error\n");
        return false;
    } else {
    	INFO("thread_main created\n");
    }

    return true;
}
#endif
