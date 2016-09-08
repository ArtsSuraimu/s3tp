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
	/*
	 * Contains the length of the payload of this current packet.
	 * The last 2 bits are reserved for the protocol.
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

	//Getters and setters
	uint8_t getPort() {
		return port & (uint8_t)0x7F;
	}

	void setPort(uint8_t port) {
		uint8_t fragFlag = moreFragments();
		this->port = (fragFlag << 7) | port;
	}

	uint8_t getGlobalSequence() {
		return (uint8_t) (seq >> 8);
	}

	void setGlobalSequence(uint8_t global_seq) {
		seq = (uint16_t )((seq & 0xFF) | (global_seq << 8));
	}

	uint8_t getSubSequence() {
		return (uint8_t) (seq & 0xFF);
	}

	void setSubSequence(uint8_t sub_seq) {
		seq = (uint16_t)((seq & 0xFF00) | sub_seq);
	}

	uint8_t getMessageType() {
		return (uint8_t)(pdu_length >> 14);
	}

	void setMessageType(uint8_t type) {
		pdu_length |= (type << 14);
	}
}S3TP_HEADER;

typedef struct tag_s3tp_PACKET {
	S3TP_HEADER hdr;
	char pdu[LEN_S3TP_PDU];
}S3TP_PACKET;
#pragma pack(pop)

typedef struct tag_queue_data
{
	S3TP_PACKET * pkt;
	uint8_t channel;			/* Logical Channel to be used on the SPI interface */
	uint8_t options;
}S3TP_PACKET_WRAPPER;

#endif /* CORE_S3TP_TYPES_H_ */
