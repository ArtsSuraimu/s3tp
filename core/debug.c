/*
 * debug.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */


#include <stdio.h>
#include "debug.h"
#include "queue.h"

int hexdump(const unsigned char *buffer, ssize_t len)
{
	int i,n;

	n = 0;
	while (n<len) {
		fprintf(stdout,"%04x    ",n);
		for (i=0; i<16 && n<len; i++,n++) {
			if (i==8)
				fprintf(stdout," ");

			fprintf(stdout,"%02hhx ",buffer[n]);
		}
		fprintf(stdout,"\n");
	}

	return n;
}

void queueTest() {
	qhead_t * root = init_queue();
	S3TP_PACKET * pack;
	for (u16 i=1050; i>750; i--) {
		pack = calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		push(root, pack);
	}
	//Packet to be put at end of q
	pack = calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1060;
	push(root, pack);

	//Packet one place before end of q
	pack = calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1059;
	push(root, pack);

	//Packet one place after end of q
	pack = calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1100;
	push(root, pack);

	for (u16 i=1; i<=500; i++) {
		pack = calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		push(root, pack);
	}
	for (u16 i=750; i>500; i--) {
		pack = calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		push(root, pack);
	}
	printf("Currently %d elements in queue. Buffer size: %d\n", root->size, computeBufferSize(root));

	//Now emptying q
	while (peek(root) != NULL) {
		pack = pop(root);
		printf("Popped element with seq num %d\n", pack->hdr.seq);
		free(pack);
	}
	printf("Queue is empty now!\n");
	deinit_queue(root);
}

int main(int argc, char**argv) {
	queueTest();
}
