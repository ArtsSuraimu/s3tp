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

typedef int SOCKET;

#pragma pack(push, 1)
typedef struct tag_s3tp_header
{
	uint16_t crc;
	uint16_t seq;		/* Global Sequence Number */
				/* First Byte: Seq_head, 
				 * 2nd Byte: Seq_sub 
				 used for packet fragmentation 
				*/
	uint16_t pdu_length;
	uint8_t seq_port;		/* used for reordering */
	uint8_t port;

	//Fragmentation bit functions (bit is the most significant bit of the port variable)
	uint8_t moreFragments() {
		return (uint8_t)((port >> 7) & 1);
	}

	void setMoreFragments() {
		port |= 1 << 7;
	}

	void unsetMoreFragments() {
		port &= ~(1 << 7);
	}
}S3TP_HEADER;

typedef struct tag_s3tp_PACKET
{
	S3TP_HEADER hdr;
	uint8_t pdu[LEN_S3TP_PDU];
}S3TP_PACKET, * pS3TP_PACKET;
#pragma pack(pop)

typedef struct tag_queue_data
{
	S3TP_PACKET * pkt;
	uint8_t channel;			/* Logical Channel to be used on the SPI interface */
}S3TP_PACKET_WRAPPER;

#endif /* CORE_S3TP_TYPES_H_ */
