#ifndef CORE_S3TP_TYPES_H_
#define CORE_S3TP_TYPES_H_

#include <stdlib.h>
#include "Constants.h"

//S3TP header Flags and Controls
#define FLAG_ACK 0x01
#define FLAG_SYN 0x02
#define FLAG_FIN 0x04
#define FLAG_RST 0x08
#define FLAG_CTRL 0x16

#define CTRL_SETUP 1
#define CTRL_SACK 2

#define CONTROL_SETUP 0x01

//Eigth channel is reserved and should not be available for applications to use
#define S3TP_VIRTUAL_CHANNELS 7

typedef int Socket;

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
    /*
     * Source port (application on the endpoint which is sending the packet)
     */
    uint8_t srcPort;

    /*
     * Destination port (application on the endpoint which is receiving the packet)
     */
    uint8_t destPort;

    /*
     * Sequence number, relative to the stream
     */
	uint8_t seq;

    /*
     * Acknowledgement number
     */
    uint8_t ack;

	/*
	 * Contains the length of the payload of this current packet.
	 * The last 2 bits are reserved for the protocol.
	 */
	uint16_t pdu_length;

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

    uint16_t getPduLength() {
        //Just taking the 10 least significant bits
        return (uint16_t )(pdu_length & 0x3FF);
    }

    void setPduLength(uint16_t pdu_len) {
        //6 most significant bits must remain the same
        pdu_length = (uint16_t)((pdu_length & 0xFC00) | (pdu_len & 0x3FF));
    }

	uint8_t getFlags() {
		return (uint8_t ) (pdu_length >> 10);
	}

	void setFlags(uint8_t flags) {
        //10 least significant bits must remain the same
        pdu_length = (uint16_t) ((pdu_length & 0x3FF) | (flags << 10));
	}

	void setAck(bool ack) {
		if (ack) {
			pdu_length |= (1 << 10);
		} else {
			pdu_length &= ~(1 << 10);
		}
	}

    void setSyn(bool syn) {
        if (syn) {
            pdu_length |= (1 << 11);
        } else {
            pdu_length &= ~(1 << 11);
        }
    }

    void setFin(bool fin) {
        if (fin) {
            pdu_length |= (1 << 12);
        } else {
            pdu_length &= ~(1 << 12);
        }
    }

    void setRst(bool rst) {
        if (rst) {
            pdu_length |= (1 << 13);
        } else {
            pdu_length &= ~(1 << 13);
        }
    }

	void setCtrl(bool control) {
		if (control) {
			pdu_length |= (1 << 14);
		} else {
			pdu_length &= ~(1 << 14);
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
        header->setFlags(0);
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
