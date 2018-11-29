/** **************************************************************************
 * s3.c
 *
 * Copyright 2008 Bryan Ischo <bryan@ischo.com>
 *
 * This file is part of libs3.
 *
 * libs3 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, version 3 or above of the License.  You can also
 * redistribute and/or modify it under the terms of the GNU General Public
 * License, version 2 or above of the License.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of this library and its programs with the
 * OpenSSL library, and distribute linked combinations including the two.
 *
 * libs3 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with libs3, in a file named COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * You should also have received a copy of the GNU General Public License
 * version 2 along with libs3, in a file named COPYING-GPLv2.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 ************************************************************************** **/

/**
 * This is a 'driver' program that simply converts command-line input into
 * calls to libs3 functions, and prints the results.
 **/

#define _XOPEN_SOURCE 600
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "libs3/libs3.h"
#include "log.h"
#include <app_common.h>
#include <limits.h>
#include "s3_config.h"

extern int notify_mqtt(char *url);

const char *s3_url = "https://s3.ap-northeast-2.amazonaws.com/tizen.s3.camera.testbucket2";


// Some Windows stuff
#ifndef FOPEN_EXTRA_FLAGS
#define FOPEN_EXTRA_FLAGS ""
#endif

// Some Unix stuff (to work around Windows issues)
#ifndef SLEEP_UNITS_PER_SECOND
#define SLEEP_UNITS_PER_SECOND 1
#endif

// Also needed for Windows, because somehow MinGW doesn't define this
//extern int putenv(char *);


// Command-line options, saved as globals ------------------------------------

static int forceG = 0;
static int showResponsePropertiesG = 0;
static S3Protocol protocolG = S3ProtocolHTTPS;
static S3UriStyle uriStyleG = S3UriStylePath;
static int retriesG = 5;
static int timeoutMsG = 0;
static int verifyPeerG = 0;
static const char *awsRegionG = NULL;


// Environment variables, saved as globals ----------------------------------

static const char *accessKeyIdG = 0;
static const char *secretAccessKeyG = 0;


// Request results, saved as globals -----------------------------------------

static int statusG = 0;
static char errorDetailsG[4096] = { 0 };


// Other globals -------------------------------------------------------------

//static char putenvBufG[256];


// Option prefixes -----------------------------------------------------------

#define LOCATION_PREFIX "location="
#define LOCATION_PREFIX_LEN (sizeof(LOCATION_PREFIX) - 1)
#define CANNED_ACL_PREFIX "cannedAcl="
#define CANNED_ACL_PREFIX_LEN (sizeof(CANNED_ACL_PREFIX) - 1)
#define PREFIX_PREFIX "prefix="
#define PREFIX_PREFIX_LEN (sizeof(PREFIX_PREFIX) - 1)
#define MARKER_PREFIX "marker="
#define MARKER_PREFIX_LEN (sizeof(MARKER_PREFIX) - 1)
#define DELIMITER_PREFIX "delimiter="
#define DELIMITER_PREFIX_LEN (sizeof(DELIMITER_PREFIX) - 1)
#define ENCODING_TYPE_PREFIX "encoding-type="
#define ENCODING_TYPE_PREFIX_LEN (sizeof(ENCODING_TYPE_PREFIX) - 1)
#define MAX_UPLOADS_PREFIX "max-uploads="
#define MAX_UPLOADS_PREFIX_LEN (sizeof(MAX_UPLOADS_PREFIX) - 1)
#define KEY_MARKER_PREFIX "key-marker="
#define KEY_MARKER_PREFIX_LEN (sizeof(KEY_MARKER_PREFIX) - 1)
#define UPLOAD_ID_PREFIX "upload-id="
#define UPLOAD_ID_PREFIX_LEN (sizeof(UPLOAD_ID_PREFIX) - 1)
#define MAX_PARTS_PREFIX "max-parts="
#define MAX_PARTS_PREFIX_LEN (sizeof(MAX_PARTS_PREFIX) - 1)
#define PART_NUMBER_MARKER_PREFIX "part-number-marker="
#define PART_NUMBER_MARKER_PREFIX_LEN (sizeof(PART_NUMBER_MARKER_PREFIX) - 1)
#define UPLOAD_ID_MARKER_PREFIX "upload-id-marker="
#define UPLOAD_ID_MARKER_PREFIX_LEN (sizeof(UPLOAD_ID_MARKER_PREFIX) - 1)
#define MAXKEYS_PREFIX "maxkeys="
#define MAXKEYS_PREFIX_LEN (sizeof(MAXKEYS_PREFIX) - 1)
#define FILENAME_PREFIX "filename="
#define FILENAME_PREFIX_LEN (sizeof(FILENAME_PREFIX) - 1)
#define CONTENT_LENGTH_PREFIX "contentLength="
#define CONTENT_LENGTH_PREFIX_LEN (sizeof(CONTENT_LENGTH_PREFIX) - 1)
#define CACHE_CONTROL_PREFIX "cacheControl="
#define CACHE_CONTROL_PREFIX_LEN (sizeof(CACHE_CONTROL_PREFIX) - 1)
#define CONTENT_TYPE_PREFIX "contentType="
#define CONTENT_TYPE_PREFIX_LEN (sizeof(CONTENT_TYPE_PREFIX) - 1)
#define MD5_PREFIX "md5="
#define MD5_PREFIX_LEN (sizeof(MD5_PREFIX) - 1)
#define CONTENT_DISPOSITION_FILENAME_PREFIX "contentDispositionFilename="
#define CONTENT_DISPOSITION_FILENAME_PREFIX_LEN \
    (sizeof(CONTENT_DISPOSITION_FILENAME_PREFIX) - 1)
#define CONTENT_ENCODING_PREFIX "contentEncoding="
#define CONTENT_ENCODING_PREFIX_LEN (sizeof(CONTENT_ENCODING_PREFIX) - 1)
#define EXPIRES_PREFIX "expires="
#define EXPIRES_PREFIX_LEN (sizeof(EXPIRES_PREFIX) - 1)
#define X_AMZ_META_PREFIX "x-amz-meta-"
#define X_AMZ_META_PREFIX_LEN (sizeof(X_AMZ_META_PREFIX) - 1)
#define USE_SERVER_SIDE_ENCRYPTION_PREFIX "useServerSideEncryption="
#define USE_SERVER_SIDE_ENCRYPTION_PREFIX_LEN \
    (sizeof(USE_SERVER_SIDE_ENCRYPTION_PREFIX) - 1)
#define IF_MODIFIED_SINCE_PREFIX "ifModifiedSince="
#define IF_MODIFIED_SINCE_PREFIX_LEN (sizeof(IF_MODIFIED_SINCE_PREFIX) - 1)
#define IF_NOT_MODIFIED_SINCE_PREFIX "ifNotmodifiedSince="
#define IF_NOT_MODIFIED_SINCE_PREFIX_LEN \
    (sizeof(IF_NOT_MODIFIED_SINCE_PREFIX) - 1)
#define IF_MATCH_PREFIX "ifMatch="
#define IF_MATCH_PREFIX_LEN (sizeof(IF_MATCH_PREFIX) - 1)
#define IF_NOT_MATCH_PREFIX "ifNotMatch="
#define IF_NOT_MATCH_PREFIX_LEN (sizeof(IF_NOT_MATCH_PREFIX) - 1)
#define START_BYTE_PREFIX "startByte="
#define START_BYTE_PREFIX_LEN (sizeof(START_BYTE_PREFIX) - 1)
#define BYTE_COUNT_PREFIX "byteCount="
#define BYTE_COUNT_PREFIX_LEN (sizeof(BYTE_COUNT_PREFIX) - 1)
#define ALL_DETAILS_PREFIX "allDetails="
#define ALL_DETAILS_PREFIX_LEN (sizeof(ALL_DETAILS_PREFIX) - 1)
#define NO_STATUS_PREFIX "noStatus="
#define NO_STATUS_PREFIX_LEN (sizeof(NO_STATUS_PREFIX) - 1)
#define RESOURCE_PREFIX "resource="
#define RESOURCE_PREFIX_LEN (sizeof(RESOURCE_PREFIX) - 1)
#define TARGET_BUCKET_PREFIX "targetBucket="
#define TARGET_BUCKET_PREFIX_LEN (sizeof(TARGET_BUCKET_PREFIX) - 1)
#define TARGET_PREFIX_PREFIX "targetPrefix="
#define TARGET_PREFIX_PREFIX_LEN (sizeof(TARGET_PREFIX_PREFIX) - 1)
#define HTTP_METHOD_PREFIX "method="
#define HTTP_METHOD_PREFIX_LEN (sizeof(HTTP_METHOD_PREFIX) - 1)

#define printf	INFO

// util ----------------------------------------------------------------------

static void S3_init()
{
    S3Status status;
    const char *hostname = getenv("S3_HOSTNAME");

    INFO("s3 hostname : %s", hostname);

    if ((status = S3_initialize("s3", verifyPeerG|S3_INIT_ALL, hostname))
        != S3StatusOK) {
        INFO("Failed to initialize libs3: %s\n",
                S3_get_status_name(status));
        exit(-1);
    }
}


static void printError()
{
    if (statusG < S3StatusErrorAccessDenied) {
        ERR("\nERROR: %s\n", S3_get_status_name(statusG));
    }
    else {
        ERR("\nERROR: %s\n", S3_get_status_name(statusG));
        ERR("%s\n", errorDetailsG);
    }
}


static void usageExit(FILE *out)
{
	ERR("usageExit...\n");
    exit(-1);
}

static uint64_t convertInt(const char *str, const char *paramName)
{
    uint64_t ret = 0;

    while (*str) {
        if (!isdigit(*str)) {
            INFO("\nERROR: Nondigit in %s parameter: %c\n",
                    paramName, *str);
            usageExit(stderr);
        }
        ret *= 10;
        ret += (*str++ - '0');
    }

    return ret;
}


typedef struct growbuffer
{
    // The total number of bytes, and the start byte
    int size;
    // The start byte
    int start;
    // The blocks
    char data[64 * 1024];
    struct growbuffer *prev, *next;
} growbuffer;


// returns nonzero on success, zero on out of memory
static int growbuffer_append(growbuffer **gb, const char *data, int dataLen)
{
    int toCopy = 0 ;
    while (dataLen) {
        growbuffer *buf = *gb ? (*gb)->prev : 0;
        if (!buf || (buf->size == sizeof(buf->data))) {
            buf = (growbuffer *) malloc(sizeof(growbuffer));
            if (!buf) {
                return 0;
            }
            buf->size = 0;
            buf->start = 0;
            if (*gb && (*gb)->prev) {
                buf->prev = (*gb)->prev;
                buf->next = *gb;
                (*gb)->prev->next = buf;
                (*gb)->prev = buf;
            }
            else {
                buf->prev = buf->next = buf;
                *gb = buf;
            }
        }

        toCopy = (sizeof(buf->data) - buf->size);
        if (toCopy > dataLen) {
            toCopy = dataLen;
        }

        memcpy(&(buf->data[buf->size]), data, toCopy);

        buf->size += toCopy, data += toCopy, dataLen -= toCopy;
    }

    return toCopy;
}


static void growbuffer_read(growbuffer **gb, int amt, int *amtReturn,
                            char *buffer)
{
    *amtReturn = 0;

    growbuffer *buf = *gb;

    if (!buf) {
        return;
    }

    *amtReturn = (buf->size > amt) ? amt : buf->size;

    memcpy(buffer, &(buf->data[buf->start]), *amtReturn);

    buf->start += *amtReturn, buf->size -= *amtReturn;

    if (buf->size == 0) {
        if (buf->next == buf) {
            *gb = 0;
        }
        else {
            *gb = buf->next;
            buf->prev->next = buf->next;
            buf->next->prev = buf->prev;
        }
        free(buf);
        buf = NULL;
    }
}


static void growbuffer_destroy(growbuffer *gb)
{
    growbuffer *start = gb;

    while (gb) {
        growbuffer *next = gb->next;
        free(gb);
        gb = (next == start) ? 0 : next;
    }
}


// Convenience utility for making the code look nicer.  Tests a string
// against a format; only the characters specified in the format are
// checked (i.e. if the string is longer than the format, the string still
// checks out ok).  Format characters are:
// d - is a digit
// anything else - is that character
// Returns nonzero the string checks out, zero if it does not.
//static int checkString(const char *str, const char *format)
//{
//    while (*format) {
//        if (*format == 'd') {
//            if (!isdigit(*str)) {
//                return 0;
//            }
//        }
//        else if (*str != *format) {
//            return 0;
//        }
//        str++, format++;
//    }
//
//    return 1;
//}

//static int64_t parseIso8601Time(const char *str)
//{
//    // Check to make sure that it has a valid format
//    if (!checkString(str, "dddd-dd-ddTdd:dd:dd")) {
//        return -1;
//    }
//
//#define nextnum() (((*str - '0') * 10) + (*(str + 1) - '0'))
//
//    // Convert it
//    struct tm stm;
//    memset(&stm, 0, sizeof(stm));
//
//    stm.tm_year = (nextnum() - 19) * 100;
//    str += 2;
//    stm.tm_year += nextnum();
//    str += 3;
//
//    stm.tm_mon = nextnum() - 1;
//    str += 3;
//
//    stm.tm_mday = nextnum();
//    str += 3;
//
//    stm.tm_hour = nextnum();
//    str += 3;
//
//    stm.tm_min = nextnum();
//    str += 3;
//
//    stm.tm_sec = nextnum();
//    str += 2;
//
//    stm.tm_isdst = -1;
//
//    // This is hokey but it's the recommended way ...
//    char *tz = getenv("TZ");
//    snprintf(putenvBufG, sizeof(putenvBufG), "TZ=UTC");
//    putenv(putenvBufG);
//
//    int64_t ret = mktime(&stm);
//
//    snprintf(putenvBufG, sizeof(putenvBufG), "TZ=%s", tz ? tz : "");
//    putenv(putenvBufG);
//
//    // Skip the millis
//
//    if (*str == '.') {
//        str++;
//        while (isdigit(*str)) {
//            str++;
//        }
//    }
//
//    if (checkString(str, "-dd:dd") || checkString(str, "+dd:dd")) {
//        int sign = (*str++ == '-') ? -1 : 1;
//        int hours = nextnum();
//        str += 3;
//        int minutes = nextnum();
//        ret += (-sign * (((hours * 60) + minutes) * 60));
//    }
//    // Else it should be Z to be a conformant time string, but we just assume
//    // that it is rather than enforcing that
//
//    return ret;
//}


static int should_retry()
{
    if (retriesG--) {
        // Sleep before next retry; start out with a 1 second sleep
        static int retrySleepInterval = 1 * SLEEP_UNITS_PER_SECOND;
        sleep(retrySleepInterval);
        // Next sleep 1 second longer
        retrySleepInterval++;
        return 1;
    }

    return 0;
}


static struct option longOptionsG[] =
{
    { "force",                no_argument,        0,  'f' },
    { "vhost-style",          no_argument,        0,  'h' },
    { "unencrypted",          no_argument,        0,  'u' },
    { "show-properties",      no_argument,        0,  's' },
    { "retries",              required_argument,  0,  'r' },
    { "timeout",              required_argument,  0,  't' },
    { "verify-peer",          no_argument,        0,  'v' },
    { "region",               required_argument,  0,  'g' },
    { 0,                      0,                  0,   0  }
};


// response properties callback ----------------------------------------------

// This callback does the same thing for every request type: prints out the
// properties if the user has requested them to be so
static S3Status responsePropertiesCallback
    (const S3ResponseProperties *properties, void *callbackData)
{
    (void) callbackData;

    if (!showResponsePropertiesG) {
        return S3StatusOK;
    }

#define print_nonnull(name, field)                                 \
    do {                                                           \
        if (properties-> field) {                                  \
            printf("%s: %s\n", name, properties-> field);          \
        }                                                          \
    } while (0)

    print_nonnull("Content-Type", contentType);
    print_nonnull("Request-Id", requestId);
    print_nonnull("Request-Id-2", requestId2);
    if (properties->contentLength > 0) {
        printf("Content-Length: %llu\n",
               (unsigned long long) properties->contentLength);
    }
    print_nonnull("Server", server);
    print_nonnull("ETag", eTag);
    if (properties->lastModified > 0) {
        char timebuf[256];
        time_t t = (time_t) properties->lastModified;
        // gmtime is not thread-safe but we don't care here.
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
        printf("Last-Modified: %s\n", timebuf);
    }
    int i;
    for (i = 0; i < properties->metaDataCount; i++) {
        printf("x-amz-meta-%s: %s\n", properties->metaData[i].name,
               properties->metaData[i].value);
    }
    if (properties->usesServerSideEncryption) {
        printf("UsesServerSideEncryption: true\n");
    }

    return S3StatusOK;
}


// response complete callback ------------------------------------------------

// This callback does the same thing for every request type: saves the status
// and error stuff in global variables
static void responseCompleteCallback(S3Status status,
                                     const S3ErrorDetails *error,
                                     void *callbackData)
{
    (void) callbackData;

    statusG = status;
    // Compose the error details message now, although we might not use it.
    // Can't just save a pointer to [error] since it's not guaranteed to last
    // beyond this callback
    int len = 0;
    if (error && error->message) {
        len += snprintf(&(errorDetailsG[len]), sizeof(errorDetailsG) - len,
                        "  Message: %s\n", error->message);
    }
    if (error && error->resource) {
        len += snprintf(&(errorDetailsG[len]), sizeof(errorDetailsG) - len,
                        "  Resource: %s\n", error->resource);
    }
    if (error && error->furtherDetails) {
        len += snprintf(&(errorDetailsG[len]), sizeof(errorDetailsG) - len,
                        "  Further Details: %s\n", error->furtherDetails);
    }
    if (error && error->extraDetailsCount) {
        len += snprintf(&(errorDetailsG[len]), sizeof(errorDetailsG) - len,
                        "%s", "  Extra Details:\n");
        int i;
        for (i = 0; i < error->extraDetailsCount; i++) {
            len += snprintf(&(errorDetailsG[len]),
                            sizeof(errorDetailsG) - len, "    %s: %s\n",
                            error->extraDetails[i].name,
                            error->extraDetails[i].value);
        }
    }
}


// list service --------------------------------------------------------------

//typedef struct list_service_data
//{
//    int headerPrinted;
//    int allDetails;
//} list_service_data;


//static void printListServiceHeader(int allDetails)
//{
//    printf("%-56s  %-20s", "                         Bucket",
//           "      Created");
//    if (allDetails) {
//        printf("  %-64s  %-12s",
//               "                            Owner ID",
//               "Display Name");
//    }
//    printf("\n");
//    printf("--------------------------------------------------------  "
//           "--------------------");
//    if (allDetails) {
//        printf("  -------------------------------------------------"
//               "---------------  ------------");
//    }
//    printf("\n");
//}


//static S3Status listServiceCallback(const char *ownerId,
//                                    const char *ownerDisplayName,
//                                    const char *bucketName,
//                                    int64_t creationDate, void *callbackData)
//{
//    list_service_data *data = (list_service_data *) callbackData;
//
//    if (!data->headerPrinted) {
//        data->headerPrinted = 1;
//        printListServiceHeader(data->allDetails);
//    }
//
//    char timebuf[256];
//    if (creationDate >= 0) {
//        time_t t = (time_t) creationDate;
//        strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
//    }
//    else {
//        timebuf[0] = 0;
//    }
//
//    printf("%-56s  %-20s", bucketName, timebuf);
//    if (data->allDetails) {
//        printf("  %-64s  %-12s", ownerId ? ownerId : "",
//               ownerDisplayName ? ownerDisplayName : "");
//    }
//    printf("\n");
//
//    return S3StatusOK;
//}


//static void list_service(int allDetails)
//{
//    list_service_data data;
//
//    data.headerPrinted = 0;
//    data.allDetails = allDetails;
//
//    S3_init();
//
//    S3ListServiceHandler listServiceHandler =
//    {
//        { &responsePropertiesCallback, &responseCompleteCallback },
//        &listServiceCallback
//    };
//
//    do {
//        S3_list_service(protocolG, accessKeyIdG, secretAccessKeyG, 0, 0,
//                        awsRegionG, 0, timeoutMsG, &listServiceHandler, &data);
//    } while (S3_status_is_retryable(statusG) && should_retry());
//
//    if (statusG == S3StatusOK) {
//        if (!data.headerPrinted) {
//            printListServiceHeader(allDetails);
//        }
//    }
//    else {
//        printError();
//    }
//
//    S3_deinitialize();
//}


// test bucket ---------------------------------------------------------------

//static void test_bucket(int argc, char **argv, int optindex)
//{
//    // test bucket
//    if (optindex == argc) {
//        INFO("\nERROR: Missing parameter: bucket\n");
//        usageExit(stderr);
//    }
//
//    const char *bucketName = argv[optindex++];
//
//    if (optindex != argc) {
//        INFO("\nERROR: Extraneous parameter: %s\n", argv[optindex]);
//        usageExit(stderr);
//    }
//
//    INFO("test bucketName : %s", bucketName);
//
//    S3_init();
//
//    S3ResponseHandler responseHandler =
//    {
//        &responsePropertiesCallback, &responseCompleteCallback
//    };
//
//    char locationConstraint[64];
//    do {
//        S3_test_bucket(protocolG, uriStyleG, accessKeyIdG, secretAccessKeyG, 0,
//                       0, bucketName, awsRegionG, sizeof(locationConstraint),
//                       locationConstraint, 0, timeoutMsG, &responseHandler, 0);
//    } while (S3_status_is_retryable(statusG) && should_retry());
//
//    const char *result;
//
//    switch (statusG) {
//    case S3StatusOK:
//        // bucket exists
//        result = locationConstraint[0] ? locationConstraint : "USA";
//        break;
//    case S3StatusErrorNoSuchBucket:
//        result = "Does Not Exist";
//        break;
//    case S3StatusErrorAccessDenied:
//        result = "Access Denied";
//        break;
//    default:
//        result = 0;
//        break;
//    }
//
//    if (result) {
//        printf("%-56s  %-20s\n", "                         Bucket",
//               "       Status");
//        printf("--------------------------------------------------------  "
//               "--------------------\n");
//        printf("%-56s  %-20s\n", bucketName, result);
//    }
//    else {
//        printError();
//    }
//
//    S3_deinitialize();
//}


// create bucket -------------------------------------------------------------

static void create_bucket(int argc, char **argv, int optindex)
{
    if (optindex == argc) {
        INFO("\nERROR: Missing parameter: bucket\n");
        usageExit(stderr);
    }

    const char *bucketName = argv[optindex++];

    if (!forceG && (S3_validate_bucket_name
                    (bucketName, S3UriStyleVirtualHost) != S3StatusOK)) {
        INFO("\nWARNING: Bucket name is not valid for "
                "virtual-host style URI access.\n");
        INFO("Bucket not created.  Use -f option to force the "
                "bucket to be created despite\n");
        INFO("this warning.\n\n");
        exit(-1);
    }

    const char *locationConstraint = 0;
    S3CannedAcl cannedAcl = S3CannedAclPublicReadWrite;
//    while (optindex < argc) {
//        char *param = argv[optindex++];
//        if (!strncmp(param, LOCATION_PREFIX, LOCATION_PREFIX_LEN)) {
//            locationConstraint = &(param[LOCATION_PREFIX_LEN]);
//        }
//        else if (!strncmp(param, CANNED_ACL_PREFIX, CANNED_ACL_PREFIX_LEN)) {
//            char *val = &(param[CANNED_ACL_PREFIX_LEN]);
//            if (!strcmp(val, "private")) {
//                cannedAcl = S3CannedAclPrivate;
//            }
//            else if (!strcmp(val, "public-read")) {
//                cannedAcl = S3CannedAclPublicRead;
//            }
//            else if (!strcmp(val, "public-read-write")) {
//                cannedAcl = S3CannedAclPublicReadWrite;
//            }
//            else if (!strcmp(val, "authenticated-read")) {
//                cannedAcl = S3CannedAclAuthenticatedRead;
//            }
//            else {
//                INFO("\nERROR: Unknown canned ACL: %s\n", val);
//                usageExit(stderr);
//            }
//        }
//        else {
//            INFO("\nERROR: Unknown param: %s\n", param);
//            usageExit(stderr);
//        }
//    }

    S3_init();

    S3ResponseHandler responseHandler =
    {
        &responsePropertiesCallback, &responseCompleteCallback
    };

    do {
        S3_create_bucket(protocolG, accessKeyIdG, secretAccessKeyG, 0, 0,
                         bucketName, awsRegionG, cannedAcl, locationConstraint,
                         0, 0, &responseHandler, 0);
    } while (S3_status_is_retryable(statusG) && should_retry());

    if (statusG == S3StatusOK) {
        printf("Bucket successfully created.\n");
    }
    else {
        printError();
    }

    S3_deinitialize();
}


// delete bucket -------------------------------------------------------------

//static void delete_bucket(int argc, char **argv, int optindex)
//{
//    if (optindex == argc) {
//        INFO("\nERROR: Missing parameter: bucket\n");
//        usageExit(stderr);
//    }
//
//    const char *bucketName = argv[optindex++];
//
//    if (optindex != argc) {
//        INFO("\nERROR: Extraneous parameter: %s\n", argv[optindex]);
//        usageExit(stderr);
//    }
//
//    S3_init();
//
//    S3ResponseHandler responseHandler =
//    {
//        &responsePropertiesCallback, &responseCompleteCallback
//    };
//
//    do {
//        S3_delete_bucket(protocolG, uriStyleG, accessKeyIdG, secretAccessKeyG,
//                         0, 0, bucketName, awsRegionG, 0, timeoutMsG, &responseHandler, 0);
//    } while (S3_status_is_retryable(statusG) && should_retry());
//
//    if (statusG != S3StatusOK) {
//        printError();
//    }
//
//    S3_deinitialize();
//}




typedef struct UploadManager{
    //used for initial multipart
    char * upload_id;

    //used for upload part object
    char **etags;
    int next_etags_pos;

    //used for commit Upload
    growbuffer *gb;
    int remaining;
} UploadManager;


typedef struct list_parts_callback_data
{
    int isTruncated;
    char nextPartNumberMarker[24];
    char initiatorId[256];
    char initiatorDisplayName[256];
    char ownerId[256];
    char ownerDisplayName[256];
    char storageClass[256];
    int partsCount;
    int handlePartsStart;
    int allDetails;
    int noPrint;
    UploadManager *manager;
} list_parts_callback_data;


typedef struct list_upload_callback_data
{
    char uploadId[1024];
} abort_upload_callback_data;

static void printListMultipartHeader(int allDetails)
{
    (void)allDetails;
}



static void printListPartsHeader()
{
    printf("%-25s  %-30s  %-30s   %-15s",
           "LastModified",
           "PartNumber", "ETag", "SIZE");

    printf("\n");
    printf("---------------------  "
           "    -------------    "
           "-------------------------------  "
           "               -----");
    printf("\n");
}



static S3Status listPartsCallback(int isTruncated,
                                  const char *nextPartNumberMarker,
                                  const char *initiatorId,
                                  const char *initiatorDisplayName,
                                  const char *ownerId,
                                  const char *ownerDisplayName,
                                  const char *storageClass,
                                  int partsCount,
                                  int handlePartsStart,
                                  const S3ListPart *parts,
                                  void *callbackData)
{
    list_parts_callback_data *data =
        (list_parts_callback_data *) callbackData;

    data->isTruncated = isTruncated;
    data->handlePartsStart = handlePartsStart;
    UploadManager *manager = data->manager;
    /*
    // This is tricky.  S3 doesn't return the NextMarker if there is no
    // delimiter.  Why, I don't know, since it's still useful for paging
    // through results.  We want NextMarker to be the last content in the
    // list, so set it to that if necessary.
    if ((!nextKeyMarker || !nextKeyMarker[0]) && uploadsCount) {
        nextKeyMarker = uploads[uploadsCount - 1].key;
    }*/
    if (nextPartNumberMarker) {
        snprintf(data->nextPartNumberMarker,
                 sizeof(data->nextPartNumberMarker), "%s",
                 nextPartNumberMarker);
    }
    else {
        data->nextPartNumberMarker[0] = 0;
    }

    if (initiatorId) {
        snprintf(data->initiatorId, sizeof(data->initiatorId), "%s",
                 initiatorId);
    }
    else {
        data->initiatorId[0] = 0;
    }

    if (initiatorDisplayName) {
        snprintf(data->initiatorDisplayName,
                 sizeof(data->initiatorDisplayName), "%s",
                 initiatorDisplayName);
    }
    else {
        data->initiatorDisplayName[0] = 0;
    }

    if (ownerId) {
        snprintf(data->ownerId, sizeof(data->ownerId), "%s",
                 ownerId);
    }
    else {
        data->ownerId[0] = 0;
    }

    if (ownerDisplayName) {
        snprintf(data->ownerDisplayName, sizeof(data->ownerDisplayName), "%s",
                 ownerDisplayName);
    }
    else {
        data->ownerDisplayName[0] = 0;
    }

    if (storageClass) {
        snprintf(data->storageClass, sizeof(data->storageClass), "%s",
                 storageClass);
    }
    else {
        data->storageClass[0] = 0;
    }

    if (partsCount && !data->partsCount && !data->noPrint) {
        printListPartsHeader();
    }

    int i;
    for (i = 0; i < partsCount; i++) {
        const S3ListPart *part = &(parts[i]);
        char timebuf[256];
        if (data->noPrint) {
            manager->etags[handlePartsStart+i] = strdup(part->eTag);
            manager->next_etags_pos++;
            manager->remaining = manager->remaining - part->size;
        } else {
            time_t t = (time_t) part->lastModified;
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ",
                     gmtime(&t));
            printf("%-30s", timebuf);
            printf("%-15llu", (unsigned long long) part->partNumber);
            printf("%-45s", part->eTag);
            printf("%-15llu\n", (unsigned long long) part->size);
        }
    }

    data->partsCount += partsCount;

    return S3StatusOK;
}


// put object ----------------------------------------------------------------

typedef struct put_object_callback_data
{
    FILE *infile;
    growbuffer *gb;
    uint64_t contentLength, originalContentLength;
    uint64_t totalContentLength, totalOriginalContentLength;
    int noStatus;
} put_object_callback_data;


static int putObjectDataCallback(int bufferSize, char *buffer,
                                 void *callbackData)
{
    put_object_callback_data *data =
        (put_object_callback_data *) callbackData;

    int ret = 0;

    if (data->contentLength) {
        int toRead = ((data->contentLength > (unsigned) bufferSize) ?
                      (unsigned) bufferSize : data->contentLength);
        if (data->gb) {
            growbuffer_read(&(data->gb), toRead, &ret, buffer);
        }
        else if (data->infile) {
            ret = fread(buffer, 1, toRead, data->infile);
        }
    }

    data->contentLength -= ret;
    data->totalContentLength -= ret;

    if (data->contentLength && !data->noStatus) {
        // Avoid a weird bug in MingW, which won't print the second integer
        // value properly when it's in the same call, so print separately
        printf("%llu bytes remaining ",
               (unsigned long long) data->totalContentLength);
        printf("(%d%% complete) ...\n",
               (int) (((data->totalOriginalContentLength -
                        data->totalContentLength) * 100) /
                      data->totalOriginalContentLength));
    }

    return ret;
}

#define MULTIPART_CHUNK_SIZE (15 << 20) // multipart is 15M

typedef struct MultipartPartData {
    put_object_callback_data put_object_data;
    int seq;
    UploadManager *manager;
} MultipartPartData;


S3Status initial_multipart_callback(const char * upload_id,
                                    void * callbackData)
{
    UploadManager *manager = (UploadManager *) callbackData;
    manager->upload_id = strdup(upload_id);
    return S3StatusOK;
}


S3Status MultipartResponseProperiesCallback
    (const S3ResponseProperties *properties, void *callbackData)
{
    responsePropertiesCallback(properties, callbackData);
    MultipartPartData *data = (MultipartPartData *) callbackData;
    int seq = data->seq;
    const char *etag = properties->eTag;
    data->manager->etags[seq - 1] = strdup(etag);
    data->manager->next_etags_pos = seq;
    return S3StatusOK;
}


static int multipartPutXmlCallback(int bufferSize, char *buffer,
                                   void *callbackData)
{
    UploadManager *manager = (UploadManager*)callbackData;
    int ret = 0;
    if (manager->remaining) {
        int toRead = ((manager->remaining > bufferSize) ?
                      bufferSize : manager->remaining);
        growbuffer_read(&(manager->gb), toRead, &ret, buffer);
    }
    manager->remaining -= ret;
    return ret;
}


static int try_get_parts_info(const char *bucketName, const char *key,
                              UploadManager *manager)
{
    S3BucketContext bucketContext =
    {
        0,
        bucketName,
        protocolG,
        uriStyleG,
        accessKeyIdG,
        secretAccessKeyG,
        0,
        awsRegionG
    };

    S3ListPartsHandler listPartsHandler =
    {
        { &responsePropertiesCallback, &responseCompleteCallback },
        &listPartsCallback
    };

    list_parts_callback_data data;

    memset(&data, 0, sizeof(list_parts_callback_data));

    data.partsCount = 0;
    data.allDetails = 0;
    data.manager = manager;
    data.noPrint = 1;
    do {
        data.isTruncated = 0;
        do {
            S3_list_parts(&bucketContext, key, data.nextPartNumberMarker,
                          manager->upload_id, 0, 0, 0, timeoutMsG, &listPartsHandler,
                          &data);
        } while (S3_status_is_retryable(statusG) && should_retry());
        if (statusG != S3StatusOK) {
            break;
        }
    } while (data.isTruncated);

    if (statusG == S3StatusOK) {
        if (!data.partsCount) {
            printListMultipartHeader(data.allDetails);
        }
    }
    else {
        printError();
        return -1;
    }

    return 0;
}


static void put_object(int argc, char **argv, int optindex,
                       const char *srcBucketName, const char *srcKey, unsigned long long srcSize)
{
    if (optindex == argc) {
        INFO("\nERROR: Missing parameter: bucket/key\n");
        usageExit(stderr);
    }

    // Split bucket/key
    char *slash = argv[optindex];
    while (*slash && (*slash != '/')) {
        slash++;
    }
    if (!*slash || !*(slash + 1)) {
        INFO("\nERROR: Invalid bucket/key name: %s\n",
                argv[optindex]);
        usageExit(stderr);
    }
    *slash++ = 0;

    const char *bucketName = argv[optindex++];
    const char *key = slash;
    const char *uploadId = 0;
    const char *filename = 0;
    uint64_t contentLength = 0;
    const char *cacheControl = 0, *contentType = 0, *md5 = 0;
    const char *contentDispositionFilename = 0, *contentEncoding = 0;
    int64_t expires = -1;
    S3CannedAcl cannedAcl = S3CannedAclPrivate;
    int metaPropertiesCount = 0;
    S3NameValue metaProperties[S3_MAX_METADATA_COUNT];
    char useServerSideEncryption = 0;
    int noStatus = 0;

    while (optindex < argc) {
        char *param = argv[optindex++];
        if (!strncmp(param, FILENAME_PREFIX, FILENAME_PREFIX_LEN)) {
            filename = &(param[FILENAME_PREFIX_LEN]);
        }
        else if (!strncmp(param, CONTENT_LENGTH_PREFIX,
                          CONTENT_LENGTH_PREFIX_LEN)) {
            contentLength = convertInt(&(param[CONTENT_LENGTH_PREFIX_LEN]),
                                       "contentLength");
            if (contentLength > (5LL * 1024 * 1024 * 1024)) {
                INFO("\nERROR: contentLength must be no greater "
                        "than 5 GB\n");
                usageExit(stderr);
            }
        }
        else if (!strncmp(param, CACHE_CONTROL_PREFIX,
                          CACHE_CONTROL_PREFIX_LEN)) {
            cacheControl = &(param[CACHE_CONTROL_PREFIX_LEN]);
        }
        else if (!strncmp(param, CONTENT_TYPE_PREFIX,
                          CONTENT_TYPE_PREFIX_LEN)) {
            contentType = &(param[CONTENT_TYPE_PREFIX_LEN]);
        }
        else if (!strncmp(param, MD5_PREFIX, MD5_PREFIX_LEN)) {
            md5 = &(param[MD5_PREFIX_LEN]);
        }
        else if (!strncmp(param, CONTENT_DISPOSITION_FILENAME_PREFIX,
                          CONTENT_DISPOSITION_FILENAME_PREFIX_LEN)) {
            contentDispositionFilename =
                &(param[CONTENT_DISPOSITION_FILENAME_PREFIX_LEN]);
        }
        else if (!strncmp(param, CONTENT_ENCODING_PREFIX,
                          CONTENT_ENCODING_PREFIX_LEN)) {
            contentEncoding = &(param[CONTENT_ENCODING_PREFIX_LEN]);
        }
        else if (!strncmp(param, UPLOAD_ID_PREFIX,
                          UPLOAD_ID_PREFIX_LEN)) {
            uploadId = &(param[UPLOAD_ID_PREFIX_LEN]);
        }
//        else if (!strncmp(param, EXPIRES_PREFIX, EXPIRES_PREFIX_LEN)) {
//            expires = parseIso8601Time(&(param[EXPIRES_PREFIX_LEN]));
//            if (expires < 0) {
//                INFO("\nERROR: Invalid expires time "
//                        "value; ISO 8601 time format required\n");
//                usageExit(stderr);
//            }
//        }
        else if (!strncmp(param, X_AMZ_META_PREFIX, X_AMZ_META_PREFIX_LEN)) {
            if (metaPropertiesCount == S3_MAX_METADATA_COUNT) {
                INFO("\nERROR: Too many x-amz-meta- properties, "
                        "limit %lu: %s\n",
                        (unsigned long) S3_MAX_METADATA_COUNT, param);
                usageExit(stderr);
            }
            char *name = &(param[X_AMZ_META_PREFIX_LEN]);
            char *value = name;
            while (*value && (*value != '=')) {
                value++;
            }
            if (!*value || !*(value + 1)) {
                INFO("\nERROR: Invalid parameter: %s\n", param);
                usageExit(stderr);
            }
            *value++ = 0;
            metaProperties[metaPropertiesCount].name = name;
            metaProperties[metaPropertiesCount++].value = value;
        }
        else if (!strncmp(param, USE_SERVER_SIDE_ENCRYPTION_PREFIX,
                          USE_SERVER_SIDE_ENCRYPTION_PREFIX_LEN)) {
            const char *val = &(param[USE_SERVER_SIDE_ENCRYPTION_PREFIX_LEN]);
            if (!strcmp(val, "true") || !strcmp(val, "TRUE") ||
                !strcmp(val, "yes") || !strcmp(val, "YES") ||
                !strcmp(val, "1")) {
                useServerSideEncryption = 1;
            }
            else {
                useServerSideEncryption = 0;
            }
        }
        else if (!strncmp(param, CANNED_ACL_PREFIX, CANNED_ACL_PREFIX_LEN)) {
            char *val = &(param[CANNED_ACL_PREFIX_LEN]);
            if (!strcmp(val, "private")) {
                cannedAcl = S3CannedAclPrivate;
            }
            else if (!strcmp(val, "public-read")) {
                cannedAcl = S3CannedAclPublicRead;
            }
            else if (!strcmp(val, "public-read-write")) {
                cannedAcl = S3CannedAclPublicReadWrite;
            }
            else if (!strcmp(val, "bucket-owner-full-control")) {
                cannedAcl = S3CannedAclBucketOwnerFullControl;
            }
            else if (!strcmp(val, "authenticated-read")) {
                cannedAcl = S3CannedAclAuthenticatedRead;
            }
            else {
                INFO("\nERROR: Unknown canned ACL: %s\n", val);
                usageExit(stderr);
            }
        }
        else if (!strncmp(param, NO_STATUS_PREFIX, NO_STATUS_PREFIX_LEN)) {
            const char *ns = &(param[NO_STATUS_PREFIX_LEN]);
            if (!strcmp(ns, "true") || !strcmp(ns, "TRUE") ||
                !strcmp(ns, "yes") || !strcmp(ns, "YES") ||
                !strcmp(ns, "1")) {
                noStatus = 1;
            }
        }
        else {
            INFO("\nERROR: Unknown param: %s\n", param);
            usageExit(stderr);
        }
    }

    put_object_callback_data data;

    data.infile = 0;
    data.gb = 0;
    data.noStatus = noStatus;

    if (srcSize) {
        // This is really a COPY multipart, not a put, so take from source object
        contentLength = srcSize;
        data.infile = NULL;
    }
    else if (filename) {
        if (!contentLength) {
            struct stat statbuf;
            // Stat the file to get its length
            if (stat(filename, &statbuf) == -1) {
                INFO("\nERROR: Failed to stat file %s: ",
                        filename);
                perror(0);
                exit(-1);
            }
            contentLength = statbuf.st_size;
        }
        // Open the file
        if (!(data.infile = fopen(filename, "r" FOPEN_EXTRA_FLAGS))) {
            INFO("\nERROR: Failed to open input file %s: ",
                    filename);
            perror(0);
            exit(-1);
        }
    }
    else {
        // Read from stdin.  If contentLength is not provided, we have
        // to read it all in to get contentLength.
        if (!contentLength) {
            // Read all if stdin to get the data
            char buffer[64 * 1024];
            while (1) {
                int amtRead = fread(buffer, 1, sizeof(buffer), stdin);
                if (amtRead == 0) {
                    break;
                }
                if (!growbuffer_append(&(data.gb), buffer, amtRead)) {
                    INFO("\nERROR: Out of memory while reading "
                            "stdin\n");
                    exit(-1);
                }
                contentLength += amtRead;
                if (amtRead < (int) sizeof(buffer)) {
                    break;
                }
            }
        }
        else {
            data.infile = stdin;
        }
    }

    data.totalContentLength =
    data.totalOriginalContentLength =
    data.contentLength =
    data.originalContentLength =
            contentLength;

    S3_init();

    S3BucketContext bucketContext =
    {
        0,
        bucketName,
        protocolG,
        uriStyleG,
        accessKeyIdG,
        secretAccessKeyG,
        0,
        awsRegionG
    };

    S3PutProperties putProperties =
    {
        contentType,
        md5,
        cacheControl,
        contentDispositionFilename,
        contentEncoding,
        expires,
        cannedAcl,
        metaPropertiesCount,
        metaProperties,
        useServerSideEncryption
    };

    if (contentLength <= MULTIPART_CHUNK_SIZE) {
        S3PutObjectHandler putObjectHandler =
        {
            { &responsePropertiesCallback, &responseCompleteCallback },
            &putObjectDataCallback
        };

        do {
            S3_put_object(&bucketContext, key, contentLength, &putProperties, 0,
                          0, &putObjectHandler, &data);
        } while (S3_status_is_retryable(statusG) && should_retry());

        if (data.infile) {
            fclose(data.infile);
        }
        else if (data.gb) {
            growbuffer_destroy(data.gb);
        }

        if (statusG != S3StatusOK) {
            printError();
        }
        else if (data.contentLength) {
            INFO("\nERROR: Failed to read remaining %llu bytes from "
                    "input\n", (unsigned long long) data.contentLength);
        }
    }
    else {
        uint64_t totalContentLength = contentLength;
        uint64_t todoContentLength = contentLength;
        UploadManager manager;
        manager.upload_id = 0;
        manager.gb = 0;

        //div round up
        int seq;
        int totalSeq = ((contentLength + MULTIPART_CHUNK_SIZE- 1) /
                        MULTIPART_CHUNK_SIZE);

        MultipartPartData partData;
        memset(&partData, 0, sizeof(MultipartPartData));
        int partContentLength = 0;

        S3MultipartInitialHandler handler = {
            {
                &responsePropertiesCallback,
                &responseCompleteCallback
            },
            &initial_multipart_callback
        };

        S3PutObjectHandler putObjectHandler = {
            {&MultipartResponseProperiesCallback, &responseCompleteCallback },
            &putObjectDataCallback
        };

        S3MultipartCommitHandler commit_handler = {
            {
                &responsePropertiesCallback,&responseCompleteCallback
            },
            &multipartPutXmlCallback,
            0
        };

        manager.etags = (char **) malloc(sizeof(char *) * totalSeq);
        manager.next_etags_pos = 0;

        if (uploadId) {
            manager.upload_id = strdup(uploadId);
            manager.remaining = contentLength;
            if (!try_get_parts_info(bucketName, key, &manager)) {
                fseek(data.infile, -(manager.remaining), 2);
                contentLength = manager.remaining;
                goto upload;
            } else {
                goto clean;
            }
        }

        do {
            S3_initiate_multipart(&bucketContext, key,0, &handler,0, timeoutMsG, &manager);
        } while (S3_status_is_retryable(statusG) && should_retry());

        if (manager.upload_id == 0 || statusG != S3StatusOK) {
            printError();
            goto clean;
        }

upload:
        todoContentLength -= MULTIPART_CHUNK_SIZE * manager.next_etags_pos;
        for (seq = manager.next_etags_pos + 1; seq <= totalSeq; seq++) {
            partData.manager = &manager;
            partData.seq = seq;
            if (partData.put_object_data.gb==NULL) {
              partData.put_object_data = data;
            }
            partContentLength = ((contentLength > MULTIPART_CHUNK_SIZE) ?
                                 MULTIPART_CHUNK_SIZE : contentLength);
            printf("%s Part Seq %d, length=%d\n", srcSize ? "Copying" : "Sending", seq, partContentLength);
            partData.put_object_data.contentLength = partContentLength;
            partData.put_object_data.originalContentLength = partContentLength;
            partData.put_object_data.totalContentLength = todoContentLength;
            partData.put_object_data.totalOriginalContentLength = totalContentLength;
            putProperties.md5 = 0;
            do {
                if (srcSize) {
                    S3BucketContext srcBucketContext =
                    {
                        0,
                        srcBucketName,
                        protocolG,
                        uriStyleG,
                        accessKeyIdG,
                        secretAccessKeyG,
                        0,
                        awsRegionG
                    };

                    S3ResponseHandler copyResponseHandler = { &responsePropertiesCallback, &responseCompleteCallback };
                    int64_t lastModified;

                    unsigned long long startOffset = (unsigned long long)MULTIPART_CHUNK_SIZE * (unsigned long long)(seq-1);
                    unsigned long long count = partContentLength - 1; // Inclusive for copies
                    // The default copy callback tries to set this for us, need to allocate here
                    manager.etags[seq-1] = malloc(512); // TBD - magic #!  Isa there a max etag defined?
                    S3_copy_object_range(&srcBucketContext, srcKey,
                                         bucketName, key,
                                         seq, manager.upload_id,
                                         startOffset, count,
                                         &putProperties,
                                         &lastModified, 512 /*TBD - magic # */,
                                         manager.etags[seq-1], 0,
                                         timeoutMsG,
                                         &copyResponseHandler, 0);
                } else {
                    S3_upload_part(&bucketContext, key, &putProperties,
                                   &putObjectHandler, seq, manager.upload_id,
                                   partContentLength,
                                   0, timeoutMsG,
                                   &partData);
                }
            } while (S3_status_is_retryable(statusG) && should_retry());
            if (statusG != S3StatusOK) {
                printError();
                goto clean;
            }
            contentLength -= MULTIPART_CHUNK_SIZE;
            todoContentLength -= MULTIPART_CHUNK_SIZE;
        }

        int i;
        int size = 0;
        size += growbuffer_append(&(manager.gb), "<CompleteMultipartUpload>",
                                  strlen("<CompleteMultipartUpload>"));
        char buf[256];
        int n;
        for (i = 0; i < totalSeq; i++) {
            n = snprintf(buf, sizeof(buf), "<Part><PartNumber>%d</PartNumber>"
                         "<ETag>%s</ETag></Part>", i + 1, manager.etags[i]);
            size += growbuffer_append(&(manager.gb), buf, n);
        }
        size += growbuffer_append(&(manager.gb), "</CompleteMultipartUpload>",
                                  strlen("</CompleteMultipartUpload>"));
        manager.remaining = size;

        do {
            S3_complete_multipart_upload(&bucketContext, key, &commit_handler,
                                         manager.upload_id, manager.remaining,
                                         0, timeoutMsG, &manager);
        } while (S3_status_is_retryable(statusG) && should_retry());
        if (statusG != S3StatusOK) {
            printError();
            goto clean;
        }

    clean:
        if(manager.upload_id) {
            free(manager.upload_id);
        }
        for (i = 0; i < manager.next_etags_pos; i++) {
            free(manager.etags[i]);
        }
        growbuffer_destroy(manager.gb);
        free(manager.etags);
    }

    S3_deinitialize();
}


int simple_put_object(char* bucketName, char *key, char *filename)
{
//    const char* bucketName = argv[1];
//    const char *key = argv[2];
//    const char *uploadId = 0;
//    const char *filename = argv[3];
    uint64_t contentLength = 0;
    const char *cacheControl = 0, *contentType = 0, *md5 = 0;
    const char *contentDispositionFilename = 0, *contentEncoding = 0;
    int64_t expires = -1;
    S3CannedAcl cannedAcl = S3CannedAclPrivate;
    int metaPropertiesCount = 0;
    S3NameValue metaProperties[S3_MAX_METADATA_COUNT];
    char useServerSideEncryption = 0;
    int noStatus = 0;

    put_object_callback_data data;

    accessKeyIdG = AWSAccessKeyId;
    if (!accessKeyIdG) {
        INFO("Missing environment variable: S3_ACCESS_KEY_ID\n");
        return -1;
    }
    secretAccessKeyG = AWSSecretKey;
    if (!secretAccessKeyG) {
        INFO("Missing environment variable: S3_SECRET_ACCESS_KEY\n");
        return -1;
    }

	char app_file_path[128] = {0,};
	char *data_path = NULL;
	char upload_file_path[128] = {0,};

	data_path = app_get_data_path();
	snprintf(app_file_path, PATH_MAX, "%s%s", data_path, filename);
	free(data_path);
	data_path = NULL;

	INFO("file [%s] upload ...", app_file_path);

	/* upload to this place */
	snprintf(upload_file_path, sizeof(upload_file_path), "%s/%s", s3_url, filename);

	INFO("upload path - [%s]", upload_file_path);

    data.infile = 0;
    data.gb = 0;
    data.noStatus = noStatus;

    if (filename) {
        if (!contentLength) {
            struct stat statbuf;
            // Stat the file to get its length
            if (stat(app_file_path, &statbuf) == -1) {
                ERR("\nERROR: Failed to stat file %s: ", app_file_path);
                exit(-1);
            }
            contentLength = statbuf.st_size;
            INFO("contentLength : %d bytes", contentLength);
        }
        // Open the file
        if (!(data.infile = fopen(app_file_path, "r" FOPEN_EXTRA_FLAGS))) {
            ERR("\nERROR: Failed to open input file %s: ", app_file_path);
            perror(0);
            exit(-1);
        }
    }
    else{
    	usageExit(stderr);
    }

    protocolG = S3ProtocolHTTP;

    data.totalContentLength =
    data.totalOriginalContentLength =
    data.contentLength =
    data.originalContentLength =
            contentLength;

    S3_init();

    S3BucketContext bucketContext =
    {
        0,
        bucketName,
        protocolG,
        uriStyleG,
        accessKeyIdG,
        secretAccessKeyG,
        0,
        awsRegionG
    };

    S3PutProperties putProperties =
    {
        contentType,
        md5,
        cacheControl,
        contentDispositionFilename,
        contentEncoding,
        expires,
        cannedAcl,
        metaPropertiesCount,
        metaProperties,
        useServerSideEncryption
    };


//    S3PutObjectHandler putObjectHandler =
//    {
//        { &responsePropertiesCallback, &responseCompleteCallback },
//        &putObjectDataCallback
//    };
//    //----------------------create bucket-----------------//
//    S3_put_object(&bucketContext, key, contentLength, &putProperties, 0,
//                          0, &putObjectHandler, &data);

    if (contentLength <= MULTIPART_CHUNK_SIZE) {
        S3PutObjectHandler putObjectHandler =
        {
            { &responsePropertiesCallback, &responseCompleteCallback },
            &putObjectDataCallback
        };

        do {
            S3_put_object(&bucketContext, key, contentLength, &putProperties, 0,
                          0, &putObjectHandler, &data);
        } while (S3_status_is_retryable(statusG) && should_retry());

        if (data.infile) {
            fclose(data.infile);
        }
        else if (data.gb) {
            growbuffer_destroy(data.gb);
        }

        if (statusG != S3StatusOK) {
            printError();
        }
        else if (data.contentLength) {
            INFO("\nERROR: Failed to read remaining %llu bytes from "
                    "input\n", (unsigned long long) data.contentLength);
        }
    }

    S3_deinitialize();

    notify_mqtt(upload_file_path);

    return 0;
}


// main ----------------------------------------------------------------------

//int s3main(int argc, char **argv)
int s3main(void)
{
	INFO("s3main : ...");
	int argc = 4;
	char *argv[4] = {"s3", "-u", "create", "tizen.s3.camera.testbucket2"};
	//char *argv[4] = {"s3", "-u", "list", "tizen.s3.camera.testbucket"};
//	argc = 5;
//	char *argv[5] = {"s3", "-u", "put", "tizen.s3.camera.testbucket/test.key", "test2.jpg"};

	// Parse args
    while (1) {
        int idx = 0;
        int c = getopt_long(argc, argv, "vfhusr:t:g:", longOptionsG, &idx);

        if (c == -1) {
            // End of options
            break;
        }

        switch (c) {
        case 'f':
            forceG = 1;
            break;
        case 'h':
            uriStyleG = S3UriStyleVirtualHost;
            break;
        case 'u':
            protocolG = S3ProtocolHTTP;
            break;
        case 's':
            showResponsePropertiesG = 1;
            break;
        case 'r': {
            const char *v = optarg;
            retriesG = 0;
            while (*v) {
                retriesG *= 10;
                retriesG += *v - '0';
                v++;
            }
            }
            break;
        case 't': {
            const char *v = optarg;
            timeoutMsG = 0;
            while (*v) {
                timeoutMsG *= 10;
                timeoutMsG += *v - '0';
                v++;
            }
            }
            break;
        case 'v':
            verifyPeerG = S3_INIT_VERIFY_PEER;
            break;
        case 'g':
            awsRegionG = strdup(optarg);
            break;
        default:
            INFO("\nERROR: Unknown option: -%c\n", c);
            // Usage exit
            usageExit(stderr);
        }
    }

    // The first non-option argument gives the operation to perform
    if (optind == argc) {
        INFO("\n\nERROR: Missing argument: command\n\n");
        usageExit(stderr);
    }

    const char *command = argv[optind++];

    if (!strcmp(command, "help")) {
        INFO("\ns3 is a program for performing single requests "
                "to Amazon S3.\n");
        usageExit(stdout);
    }

    accessKeyIdG = AWSAccessKeyId;
    if (!accessKeyIdG) {
        INFO("Missing environment variable: S3_ACCESS_KEY_ID\n");
        return -1;
    }
    secretAccessKeyG = AWSSecretKey;
    if (!secretAccessKeyG) {
        INFO("Missing environment variable: S3_SECRET_ACCESS_KEY\n");
        return -1;
    }

//    if (!strcmp(command, "list")) {
//        list(argc, argv, optind);
//    }
    if (!strcmp(command, "create")) {
        create_bucket(argc, argv, optind);
    }
    else if (!strcmp(command, "put")) {
    	INFO("put - optind = %d", optind);
        put_object(argc, argv, optind, NULL, NULL, 0);
    }
    else {
        INFO("Unknown command: %s\n", command);
        return -1;
    }

    return 0;
}
