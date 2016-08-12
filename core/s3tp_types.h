/*
 * s3tp_types.h
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#ifndef CORE_S3TP_TYPES_H_
#define CORE_S3TP_TYPES_H_

#include <stdlib.h>
#include "constants.h"

#define IN
#define OUT
#define NULL 0

typedef int size_t;
typedef int SOCKET;

typedef unsigned char u8;
typedef char i8;
typedef unsigned short u16;
typedef short i16;
typedef int i32;
typedef unsigned int u32;
typedef long i64;
typedef unsigned long u64;
typedef long long i128;
typedef unsigned long long u128;

typedef int bool;

#pragma pack(push, 1)
typedef struct tag_s3tp_header
{
	i8 direction;
	i8 seq;
	i8 type;
	i16 appid;
	i16 pdu_length;
	i8 checksum;
}S3TP_HEADER;

typedef struct tag_s3tp_PACKET
{
	S3TP_HEADER hdr;
	i8 pdu[LEN_S3TP_PDU];
}S3TP_PACKET, * pS3TP_PACKET;
#pragma pack(pop)

typedef void (*S3TP_CALLBACK) (void* pData);
typedef size_t (*RAW_SEND) (SOCKET socket, const void * buffer, size_t length, int flags);
typedef size_t (*RAW_RECV) (SOCKET socekt, char * buf, int len, int flags);
typedef size_t (*ISR_RECV) (u8 identifier, i8 seq, const void * buffer);




#endif /* CORE_S3TP_TYPES_H_ */
