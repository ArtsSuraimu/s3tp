#ifndef CORE_S3TP_TYPES_H_
#define CORE_S3TP_TYPES_H_

#include <stdlib.h>
#include "Constants.h"

//S3TP Header Flags
#define S3TP_FLAG_DATA 0x00
#define S3TP_FLAG_CTRL 0x01
#define S3TP_FLAG_ACK 0x02
#define S3TP_FLAG_MORE_FRAGMENTS 0x04

//Flags and Controls
#define FLAG_ACK 0x01
#define FLAG_SYN 0x02
#define FLAG_FIN 0x04
#define FLAG_RST 0x08
#define FLAG_CTRL 0x16

#define CONTROL_SETUP 0x01

//Eigth channel is reserved and should not be available for applications to use
#define S3TP_VIRTUAL_CHANNELS 7

typedef int SOCKET;

#pragma pack(push, 1)
/**
 * Structure containing the header of an s3tp packet.
 * Each header is 6 bytes long and is setup as follows:
 *
 * 				8 bits				        8 bits
 * -----------------------------------------------------------
 * 				SRC_PORT	    |          DEST_PORT
 * -----------------------------------------------------------
 * 			SEQUENCE NUMBER		|        ACK SEQUENCE
 * -----------------------------------------------------------
 *          PAYLOAD LENGTH             |        FLAGS
 * -----------------------------------------------------------
 *
 * The first 4 fields are 1 byte each; payload length is 10 bits long,
 * while the flags field is 6 bits long.
 */
//TODO: update doc
typedef struct tag_s3tp_header
{
	uint16_t crc;
	/* Global Sequence Number
	 * First Byte: Seq_head,
     * 2nd Byte: Seq_sub used for packet fragmentation
    */
	uint16_t seq;

	/*
	 * Contains the length of the payload of this current packet.
	 * The last 2 bits are reserved for the protocol.
	 */
	uint16_t pdu_length;
	uint8_t port;
	uint8_t seq_port;		/* used for reordering */
	uint16_t ack;

	//Fragmentation bit functions (bit is the most significant bit of the pdu_length variable)
	uint8_t moreFragments() {
		return (uint8_t)((pdu_length >> 15) & 1);
	}

	void setMoreFragments() {
		pdu_length |= (1 << 15);
	}

	void unsetMoreFragments() {
		pdu_length &= ~(1 << 15);
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

    uint16_t getPduLength() {
        return (uint16_t )(pdu_length & 0x1FFF);
    }

    void setPduLength(uint16_t pdu_len) {
        pdu_length = (uint16_t)((pdu_length & 0x2000) | pdu_len);
    }

	uint8_t getFlags() {
		return (uint8_t ) (pdu_length >> 13);
	}

	void setFlags(uint8_t flags) {
		pdu_length = (uint16_t )((pdu_length & 0x1FFF) | (flags << 13));
	}

	void setData(bool data) {
		if (data) {
			pdu_length |= (0 << 13);
		} else {
			pdu_length &= ~(0 << 13);
		}
	}

	void setAck(bool ack) {
		if (ack) {
			pdu_length |= (1 << 14);
		} else {
			pdu_length &= ~(1 << 14);
		}
	}

	void setCtrl(bool control) {
		if (control) {
			pdu_length |= (1 << 13);
		} else {
			pdu_length &= ~(1 << 13);
		}
	}

}S3TP_HEADER;

/**
 * Structure containing an S3TP packet, made up of an S3TP header and its payload.
 * The underlying buffer is given by a char array, in which the first sizeof(S3TP_HEADER) bytes are the header,
 * while all the following bytes are part of the payload.
 *
 * The structure furthermore contains metadata needed by the protocol, such as options and the virtual channel.
 */
struct S3TP_PACKET{
	char * packet;
	uint8_t channel;  /* Logical Channel to be used on the SPI interface */
	uint8_t options;

	/**
	 * Constructs a packet given the payload. The payload is copied and appended to
	 * the header in a contiguous chunk of memory. By default, all header fields are unset,
	 * as it is up to the application to set them accordingly.
	 * @param pdu  The payload to be sent
	 * @param pduLen  The length of the payload that will be copied inside the packet
	 * @return The constructed S3TP packet
	 */
	S3TP_PACKET(const char * pdu, uint16_t pduLen) {
		packet = new char[sizeof(S3TP_HEADER) + (pduLen * sizeof(char))];
		memcpy(getPayload(), pdu, pduLen);
		S3TP_HEADER * header = getHeader();
		header->setPduLength(pduLen);
        header->setFlags(S3TP_FLAG_DATA);
	}

	/**
	 * Reverse Constructor. Called when receiving a well-formed packet, with a meaningful header
	 * @param packet  The pointer to the whole packet (header + payload)
	 * @param len  The total length of the packet
	 * @param channel  The virtual channel on which it was received
	 * @return The constructed S3TP packet
	 */
	S3TP_PACKET(const char * packet, int len, uint8_t channel) {
		//Copying a well formed packet, where all header fields should already be consistent
		this->packet = new char[len * sizeof(char)];
		memcpy(this->packet, packet, (size_t)len);
		this->channel = channel;
	}

	~S3TP_PACKET() {
		delete [] packet;
	}

	int getLength() {
		return (sizeof(S3TP_HEADER) + (getHeader()->getPduLength() * sizeof(char)));
	}

	char * getPayload() {
		return packet + (sizeof(S3TP_HEADER));
	}

	S3TP_HEADER * getHeader() {
		return (S3TP_HEADER *)packet;
	}
};

struct S3TP_CONTROL {
    uint8_t opcode;
};

#pragma pack(pop)

#endif /* CORE_S3TP_TYPES_H_ */
