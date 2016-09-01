/*
 * debug.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */


#include <stdio.h>
#include <time.h>
#include "debug.h"
#include <unistd.h>
#include <string>
#include "s3tp_daemon.h"

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
	S3TP_PACKET_WRAPPER * wrapper;
	for (uint16_t i=1050; i>750; i--) {
		pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		wrapper = new S3TP_PACKET_WRAPPER();
		wrapper->channel = 0;
		wrapper->pkt = pack;
		push(root, wrapper);
	}
	//Packet to be put at end of q
	pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1060;
	wrapper = new S3TP_PACKET_WRAPPER();
	wrapper->channel = 0;
	wrapper->pkt = pack;
	push(root, wrapper);

	//Packet one place before end of q
	pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1059;
	wrapper = new S3TP_PACKET_WRAPPER();
	wrapper->channel = 0;
	wrapper->pkt = pack;
	push(root, wrapper);

	//Packet one place after end of q
	pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1100;
	wrapper = new S3TP_PACKET_WRAPPER();
	wrapper->channel = 0;
	wrapper->pkt = pack;
	push(root, wrapper);

	for (uint16_t i=1; i<=500; i++) {
		pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		wrapper = new S3TP_PACKET_WRAPPER();
		wrapper->channel = 0;
		wrapper->pkt = pack;
		push(root, wrapper);
	}
	for (uint16_t i=750; i>500; i--) {
		pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		wrapper = new S3TP_PACKET_WRAPPER();
		wrapper->channel = 0;
		wrapper->pkt = pack;
		push(root, wrapper);
	}
	printf("Currently %d elements in queue. Buffer size: %d\n", root->size, computeBufferSize(root));

	//Now emptying q
	while (peek(root) != NULL) {
		wrapper = pop(root);
		pack = wrapper->pkt;
		printf("Popped element with seq num %d\n", pack->hdr.seq);
		free(pack);
		free(wrapper);
	}
	printf("Queue is empty now!\n");
	deinit_queue(root);
}

/**
 * Testing the buffer
 */
void * publisherRoutine(void * arg) {
	TxModule * tx = (TxModule*)arg;
	uint8_t appPort = (uint8_t) (rand() % DEFAULT_MAX_OUT_PORTS);
	uint16_t sleepSeconds = 1;

	for (uint16_t i=0; i < 100; i++) {
		S3TP_PACKET * packet = new S3TP_PACKET();
		packet->hdr.port = appPort;
		printf("Enqueuing packet %d...\n", i);
		tx->enqueuePacket(packet, 0, false, 0);
	}
	for (uint16_t i=100; i<110; i++) {
		S3TP_PACKET * packet = new S3TP_PACKET();
		packet->hdr.port = appPort;
		printf("Enqueuing packet %d...\n", i);
		tx->enqueuePacket(packet, 0, false, 0);
		sleep(sleepSeconds);
	}
	for (uint16_t i=110; i<120; i++) {
		S3TP_PACKET * packet = new S3TP_PACKET();
		packet->hdr.port = appPort;
		printf("Enqueuing packet %d...\n", i);
		tx->enqueuePacket(packet, 0, false, 0);
		sleep(sleepSeconds);
	}

	pthread_exit(NULL);
	return NULL;
}

void txModuleTest() {
	TxModule tx;
	tx.startRoutine(NULL);
	pthread_t publisherThread1;
	pthread_t publisherThread2;
	pthread_create(&publisherThread1, NULL, publisherRoutine, &tx);
	pthread_create(&publisherThread2, NULL, publisherRoutine, &tx);
	uint16_t sleepSeconds = 15;
	sleep(sleepSeconds);
	tx.stopRoutine();
	sleep(5);
}

void * applicationRoutine(void * args) {
	s3tp_main * main = (s3tp_main *)args;
	uint8_t appPort = (uint8_t) (rand() % DEFAULT_MAX_OUT_PORTS);
	int len, i, result;
	char * message;
	for (i=0; i<5; i++) {
		len = rand() % LEN_S3TP_PDU;
		message = new char[len];
		result = main->send(0, appPort, message, (size_t)len);
		if (result != CODE_SUCCESS) {
			printf("Error in sending message\n");
		}
	}
	//Message to fragment
	len = (rand() % (LEN_S3TP_PDU * 2)) + 2000;
	message = new char[len];
	result = main->send(0, appPort, message, (size_t)len);
	if (result != CODE_SUCCESS) {
		printf("Error in sending message\n");
	}
	for (i=0; i<5; i++) {
		len = rand() % LEN_S3TP_PDU;
		message = new char[len];
		result = main->send(0, appPort, message, (size_t)len);
		if (result != CODE_SUCCESS) {
			printf("Error in sending message\n");
		}
	}
	pthread_exit(NULL);
}

void s3tpMainModuleTest() {
	s3tp_main main;
	main.init();
	pthread_t appThread1;
	pthread_t appThread2;
	pthread_create(&appThread1, NULL, applicationRoutine, &main);
	pthread_create(&appThread2, NULL, applicationRoutine, &main);
	pthread_join(appThread1, NULL);
	pthread_join(appThread2, NULL);
	sleep(5);
	main.stop();
}

void simpleQueueTest() {
	SimpleQueue<int> testQueue(3, SIMPLE_QUEUE_NON_BLOCKING);
	for (uint8_t i=10; i<15; i++) {
		if (testQueue.push(i) == SIMPLE_QUEUE_FULL) {
			printf("Data for port %d not inserted. Queue full\n", i);
		}
	}

	while (!testQueue.isEmpty()) {
		printf("Popped data: %d\n", testQueue.pop());
	}
}

void crcTest() {
	std::string myTest = "helloworldblablub";
	size_t len = myTest.length();
	uint16_t crc = calc_checksum(myTest.data(), (uint16_t) len);
	uint8_t firstByte = (uint8_t)crc;
	uint8_t secondByte = (uint8_t)(crc >> 8);
	printf("CRC: %d Split: %d %d\n", crc, firstByte, secondByte);
	//Correct checksum
	if (verify_checksum(myTest.data(), (uint16_t)len, crc)) {
		printf("First verification succeeded!\n");
	} else {
		printf("First verification failed!\n");
	}
	myTest = "hellowarldblablub";
	len = myTest.length();
	if (verify_checksum(myTest.data(), (uint16_t)len, crc)) {
		printf("Second verification succeeded!\n");
	} else {
		printf("Second verification failed!\n");
	}
}

void daemonTest() {
	s3tp_daemon testDaemon;
	int result = testDaemon.init();
	if (result != 0) {
		printf("Exiting..\n");
	}
	testDaemon.startDaemon();
}

int main(int argc, char**argv) {
	srand(time(NULL));
	//queueTest();
	//txModuleTest();
	//simpleQueueTest();
	//s3tpMainModuleTest();
	crcTest();
	daemonTest();
}
