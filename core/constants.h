/*
 * constants.h
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#define VER "0.1"

#define ETH_S3TP 0xBEEF
#define MTU 1021

#define TIMEOUT 1000 //in ms

#define LEN_ETH_HDR 14
#define LEN_S3TP_HDR 8
#define MAX_LEN_S3TP_PACKET 1000
#define LEN_S3TP_PDU (MAX_LEN_S3TP_PACKET - LEN_S3TP_HDR)

#define DEFAULT_NUM_PACKETS 256

#define DEFAULT_MAX_IN_PORTS 128
#define DEFAULT_MAX_OUT_PORTS 128

#define DEFAULT_MAX_FRAGMENTS 256
#define MAX_PDU_LENGTH (LEN_S3TP_PDU * DEFAULT_MAX_FRAGMENTS)
//#define DEFAULT_MAX_PDU_LENGTH (1 << 16)

#define CODE_SUCCESS 0

#define REORDERING 0x1
#define TYPE_SAT 0x2
#define TYPE_GND 0x4

#define ACK 0x06
#define PDU 0x1
#define NAK 0x15
#define RST 0x21
#define SYN 0x29

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) > (b)) ? (a) : (b))

#endif /* CONSTANTS_H_ */
