/*
 * utilities.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#include "utilities.h"

/**
 * Return CRC-8 of the data, using x^8 + x^2 + x + 1 polynomial.
 */

#define CRC16 0x8005

u16 gen_crc16(const u8 *data, u16 size)
{
	u16 out = 0;
    int bits_read = 0, bit_flag;

    /* Sanity check: */
    if(data == NULL)
        return 0;

    while(size > 0)
    {
        bit_flag = out >> 15;

        /* Get next bit: */
        out <<= 1;
        out |= (*data >> (7 - bits_read)) & 1;

        /* Increment bit counter: */
        bits_read++;
        if(bits_read > 7)
        {
            bits_read = 0;
            data++;
            size--;
        }

        /* Cycle check: */
        if(bit_flag)
            out ^= CRC16;

    }
    return out;
}
u16 calc_checksum(S3TP_PACKET * pkt)
{

	return gen_crc16(pkt->pdu, pkt->hdr.pdu_length);
}

/**
 * Calculate the CRC and check against stored CRC
 */
bool verify_checksum(S3TP_PACKET* pkt)
{
	bool ret = (calc_checksum(pkt) - pkt->hdr.checksum);
	return ret;
}

/*Get next available sequence number */
u8 get_nxt_seq(int * seed)
{
	(*seed) ++;
	return *seed;

}

u64 get_timestamp()
{
	return (u64) time(0);
}


/**
 * Pack a custom length of payload into a packet
 * The array of packets are calloced and shall be freed
 * after use. This is at responsibility of sender.
 */
ssize_t pack_packets(
		const void * payload,
		ssize_t len,
		i8 direction,
		i8 appid,
		S3TP_PACKET** packets)
{
	int i =0;
	u8 *pdu;
	int pCount;
	int cur;
	S3TP_PACKET* pkt;
	int pktlen;

	if(payload == 0 || packets == 0)
	{
		return -1;
	}

	pCount = len / LEN_S3TP_PDU + 1;
	packets = (S3TP_PACKET ** )calloc(pCount, sizeof(S3TP_PACKET*));

	for(i=0; i<pCount; i++)
	{
		pktlen = (len>LEN_S3TP_PDU) ? len % LEN_S3TP_PDU +1 : len;
		pkt = (S3TP_PACKET*) calloc(1, sizeof(S3TP_PACKET));
		pkt->hdr.appid = appid;
		pkt->hdr.direction = direction;
		pkt->hdr.pdu_length = pktlen;
		pkt->hdr.type = 0x1;
		memcpy(pkt->pdu, payload, pktlen);
		payload += pktlen;
		pkt->hdr.checksum = calc_checksum(pkt);
	}

	return (ssize_t) pCount;
}

/**
 * Unpack packets into a single buffer
 * assume the buffer is already given.
 */
ssize_t unpack_packets(
		const void * payload,
		int count,
		i8 direction,
		i8 appid,
		S3TP_PACKET ** packets
		)
{
	int i=0;
	int len=0;
	u8* pdu = (u8*) payload;

	for(i=0; i<count; i++)
	{
		if(verify_checksum(packets[i])!=0){
			return (0);
		}

		if(packets[i]->hdr.appid != appid){
			continue;
		}

		if(packets[i]->hdr.direction != direction)
		{
			continue;
		}

		memcpy(pdu +len, packets[i]->pdu, packets[i]->hdr.pdu_length);
		len += packets[i]->hdr.pdu_length;
	}
	return len;
}
