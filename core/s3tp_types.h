/*
 * s3tp_types.h
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#ifndef CORE_S3TP_TYPES_H_
#define CORE_S3TP_TYPES_H_

#include <stdlib.h>

#define IN
#define OUT

typedef int size_t;
typedef int SOCKET;

typedef struct tag_s3tp_PACKET
{

}S3TP_PACKET;

typedef void (*S3TP_CALLBACK) (void* pData);
typedef size_t (*RAW_SEND) (SOCKET socket, const void * buffer, size_t length, int flags);
typedef size_t (*RAW_RECV) (SOCKET socekt, char * buf, int len, int flags);



#endif /* CORE_S3TP_TYPES_H_ */
