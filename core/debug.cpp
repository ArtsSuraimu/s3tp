/*
 * debug.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */


#include <stdio.h>
#include <time.h>
#include "debug.h"
#include "TxModule.h"
#include <unistd.h>
#include "SimpleQueue.h"

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
	PriorityQueue * root = (PriorityQueue *) init_queue();
	S3TP_PACKET * pack;
	for (u16 i=1050; i>750; i--) {
		pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		push(root, pack);
	}
	//Packet to be put at end of q
	pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1060;
	push(root, pack);

	//Packet one place before end of q
	pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1059;
	push(root, pack);

	//Packet one place after end of q
	pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1100;
	push(root, pack);

	for (u16 i=1; i<=500; i++) {
		pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		push(root, pack);
	}
	for (u16 i=750; i>500; i--) {
		pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
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

void * publisherRoutine(void * arg) {
	Buffer * buffer = (Buffer *)arg;
	u8 appPort = (u8) (rand() % 128);
	u16 sleepSeconds = 1;

	for (u16 i=0; i < 100; i++) {
		S3TP_PACKET * packet = new S3TP_PACKET();
		packet->hdr.port = appPort;
		packet->hdr.seq = i;
		printf("Writing packet %d into buffer...\n", i);
		buffer->write(appPort, packet);
	}
	for (u16 i=100; i<110; i++) {
		S3TP_PACKET * packet = new S3TP_PACKET();
		packet->hdr.port = appPort;
		packet->hdr.seq = i;
		printf("Writing packet %d into buffer...\n", i);
		buffer->write(appPort, packet);
		sleep(sleepSeconds);
	}
	for (u16 i=110; i<120; i++) {
		S3TP_PACKET * packet = new S3TP_PACKET();
		packet->hdr.port = appPort;
		packet->hdr.seq = i;
		printf("Writing packet %d into buffer...\n", i);
		buffer->write(appPort, packet);
		sleep(sleepSeconds);
	}

	pthread_exit(NULL);
	return NULL;
}

void txModuleTest() {
	TxModule tx;
	Buffer * buffer = new Buffer();
	tx.startRoutine(buffer, NULL);
	pthread_t publisherThread1;
	pthread_t publisherThread2;
	pthread_create(&publisherThread1, NULL, publisherRoutine, buffer);
	pthread_create(&publisherThread2, NULL, publisherRoutine, buffer);
	u16 sleepSeconds = 15;
	sleep(sleepSeconds);
	tx.stopRoutine();
	sleep(5);
}

void simpleQueueTest() {
	SimpleQueue<RawData> testQueue(3);
	for (u8 i=10; i<15; i++) {
		if (testQueue.push(RawData(NULL, 0, i, i)) == SIMPLE_QUEUE_FULL) {
			printf("Data for port %d not inserted. Queue full\n", i);
		}
	}

	while (!testQueue.isEmpty()) {
		printf("Popped data for port %d\n", testQueue.pop().port);
	}
}

int main(int argc, char**argv) {
	srand(time(NULL));
	//queueTest();
	//txModuleTest();
	simpleQueueTest();
	printf("Size of S3TP Packet: %d\n", sizeof(S3TP_PACKET));
}
