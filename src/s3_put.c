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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "libs3/libs3.h"
#include "log.h"
#include <app_common.h>
#include <limits.h>
#include "s3_config.h"

extern int notify_mqtt(char *url);

// Some Windows stuff
#ifndef FOPEN_EXTRA_FLAGS
#define FOPEN_EXTRA_FLAGS ""
#endif

// Some Unix stuff (to work around Windows issues)
#ifndef SLEEP_UNITS_PER_SECOND
#define SLEEP_UNITS_PER_SECOND 1
#endif


// Command-line options, saved as globals ------------------------------------

//static int forceG = 0;
static int showResponsePropertiesG = 0;
static S3Protocol protocolG = S3ProtocolHTTPS;
static S3UriStyle uriStyleG = S3UriStylePath;
static int retriesG = 5;
//static int timeoutMsG = 0;
static int verifyPeerG = 0;
static const char *awsRegionG = NULL;


// Environment variables, saved as globals ----------------------------------

static const char *accessKeyIdG = 0;
static const char *secretAccessKeyG = 0;


// Request results, saved as globals -----------------------------------------

static int statusG = 0;
static char errorDetailsG[4096] = { 0 };


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

//static void printListMultipartHeader(int allDetails)
//{
//    (void)allDetails;
//}



//static void printListPartsHeader()
//{
//    printf("%-25s  %-30s  %-30s   %-15s",
//           "LastModified",
//           "PartNumber", "ETag", "SIZE");
//
//    printf("\n");
//    printf("---------------------  "
//           "    -------------    "
//           "-------------------------------  "
//           "               -----");
//    printf("\n");
//}



//static S3Status listPartsCallback(int isTruncated,
//                                  const char *nextPartNumberMarker,
//                                  const char *initiatorId,
//                                  const char *initiatorDisplayName,
//                                  const char *ownerId,
//                                  const char *ownerDisplayName,
//                                  const char *storageClass,
//                                  int partsCount,
//                                  int handlePartsStart,
//                                  const S3ListPart *parts,
//                                  void *callbackData)
//{
//    list_parts_callback_data *data =
//        (list_parts_callback_data *) callbackData;
//
//    data->isTruncated = isTruncated;
//    data->handlePartsStart = handlePartsStart;
//    UploadManager *manager = data->manager;
//    /*
//    // This is tricky.  S3 doesn't return the NextMarker if there is no
//    // delimiter.  Why, I don't know, since it's still useful for paging
//    // through results.  We want NextMarker to be the last content in the
//    // list, so set it to that if necessary.
//    if ((!nextKeyMarker || !nextKeyMarker[0]) && uploadsCount) {
//        nextKeyMarker = uploads[uploadsCount - 1].key;
//    }*/
//    if (nextPartNumberMarker) {
//        snprintf(data->nextPartNumberMarker,
//                 sizeof(data->nextPartNumberMarker), "%s",
//                 nextPartNumberMarker);
//    }
//    else {
//        data->nextPartNumberMarker[0] = 0;
//    }
//
//    if (initiatorId) {
//        snprintf(data->initiatorId, sizeof(data->initiatorId), "%s",
//                 initiatorId);
//    }
//    else {
//        data->initiatorId[0] = 0;
//    }
//
//    if (initiatorDisplayName) {
//        snprintf(data->initiatorDisplayName,
//                 sizeof(data->initiatorDisplayName), "%s",
//                 initiatorDisplayName);
//    }
//    else {
//        data->initiatorDisplayName[0] = 0;
//    }
//
//    if (ownerId) {
//        snprintf(data->ownerId, sizeof(data->ownerId), "%s",
//                 ownerId);
//    }
//    else {
//        data->ownerId[0] = 0;
//    }
//
//    if (ownerDisplayName) {
//        snprintf(data->ownerDisplayName, sizeof(data->ownerDisplayName), "%s",
//                 ownerDisplayName);
//    }
//    else {
//        data->ownerDisplayName[0] = 0;
//    }
//
//    if (storageClass) {
//        snprintf(data->storageClass, sizeof(data->storageClass), "%s",
//                 storageClass);
//    }
//    else {
//        data->storageClass[0] = 0;
//    }
//
//    if (partsCount && !data->partsCount && !data->noPrint) {
//        printListPartsHeader();
//    }
//
//    int i;
//    for (i = 0; i < partsCount; i++) {
//        const S3ListPart *part = &(parts[i]);
//        char timebuf[256];
//        if (data->noPrint) {
//            manager->etags[handlePartsStart+i] = strdup(part->eTag);
//            manager->next_etags_pos++;
//            manager->remaining = manager->remaining - part->size;
//        } else {
//            time_t t = (time_t) part->lastModified;
//            strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ",
//                     gmtime(&t));
//            printf("%-30s", timebuf);
//            printf("%-15llu", (unsigned long long) part->partNumber);
//            printf("%-45s", part->eTag);
//            printf("%-15llu\n", (unsigned long long) part->size);
//        }
//    }
//
//    data->partsCount += partsCount;
//
//    return S3StatusOK;
//}


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


int simple_put_object(char* bucketName, char *key, char *filename)
{
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

	data_path = app_get_data_path();
	snprintf(app_file_path, PATH_MAX, "%s%s", data_path, filename);
	free(data_path);
	data_path = NULL;

	INFO("file [%s] upload ...", app_file_path);

    data.infile = 0;
    data.gb = 0;
    data.noStatus = noStatus;

    if (filename) {
        if (!contentLength) {
            struct stat statbuf;
            // Stat the file to get its length
            if (stat(app_file_path, &statbuf) == -1) {
                ERR("\nERROR: Failed to stat file %s: ", app_file_path);
                return -1;
            }
            contentLength = statbuf.st_size;
            INFO("contentLength : %d bytes", contentLength);
        }
        // Open the file
        if (!(data.infile = fopen(app_file_path, "r" FOPEN_EXTRA_FLAGS))) {
            ERR("\nERROR: Failed to open input file %s: ", app_file_path);
            return -1;
        }
    }
    else{
    	ERR("filename error...\n");
        return -1;
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

    notify_mqtt(filename);

    return 0;
}

