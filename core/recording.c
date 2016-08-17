/*
 * recording.c
 *
 *  Created on: Aug 17, 2016
 *      Author: dai
 */

#include <stdio.h>
#include <stdlib.h>
#include "s3tp_types.h"
#include "utilities.h"

typedef struct tag_rec_data rec_data;



static FILE * recFile;

int init_recording(char len)
{
	char fn[256];

	sprintf(fn, "dump-%llu.hex", get_timestamp());
	recFile = fopen(fn, "ab+");

	if(recFile == 0)
	{
		return -1;
	}

	return 0;

}

int dump_packet(S3TP_PACKET* packet)
{
	int ret = 0;
	i8* ptr = (i8*) packet;
	if(!packet)
	{
		ret = fwrite(
				ptr,
				sizeof(i8),
				sizeof(packet->hdr)+ packet->hdr.pdu_length,
				recFile);
	}else{
		ret = -1;
	}
	return ret;
}


void deinit_recording()
{

}
