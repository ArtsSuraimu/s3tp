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
#include "TransportDaemon.h"

const int default_opts = S3TP_OPTION_ARQ;

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

class MyTest : public PriorityComparator<S3TP_PACKET_WRAPPER*> {
public:
	uint8_t global_seq_num;
	uint8_t to_consume;
	int comparePriority(S3TP_PACKET_WRAPPER* element1, S3TP_PACKET_WRAPPER* element2) {
		int8_t comp = 0;
		uint8_t seq1, seq2, offset;
		offset = to_consume;
		//First check global seq number for comparison
		seq1 = ((uint8_t)(element1->pkt->hdr.seq >> 8)) - offset;
		seq2 = ((uint8_t)(element2->pkt->hdr.seq >> 8)) - offset;
		if (seq1 < seq2) {
			comp = -1; //Element 1 is lower, hence has higher priority
		} else if (seq1 > seq2) {
			comp = 1; //Element 2 is lower, hence has higher priority
		}
		if (comp != 0) {
			return comp;
		}
		seq1 = (uint8_t)(element1->pkt->hdr.seq_port);
		seq2 = (uint8_t)(element2->pkt->hdr.seq_port);
		if (seq1 < seq2) {
			comp = -1; //Element 1 is lower, hence has higher priority
		} else if (seq1 > seq2) {
			comp = 1; //Element 2 is lower, hence has higher priority
		}
		return comp;
	}
};

void queueIntegrityTest() {
	MyTest test;
	PriorityQueue<S3TP_PACKET_WRAPPER*> * q = new PriorityQueue<S3TP_PACKET_WRAPPER*>();
	S3TP_PACKET * pack;
	S3TP_PACKET_WRAPPER * wrapper;
	test.global_seq_num = 0;
	test.to_consume = 0;
	char data1 [] = {'h','e','l','l','o'};
	size_t s = sizeof(data1);

	pack = new S3TP_PACKET();
	memcpy(pack->pdu, data1, s);
	pack->hdr.pdu_length = (uint16_t )s;
	pack->hdr.setPort(20);
	pack->hdr.seq = (uint16_t)(test.global_seq_num << 8);
	pack->hdr.seq_port = 0;
	wrapper = new S3TP_PACKET_WRAPPER();
	wrapper->channel = 0;
	wrapper->pkt = pack;
	q->push(wrapper, &test);
	test.global_seq_num++;

	s = 15;
	char * data2 = new char[s];
	for (int i=0; i<s; i++) {
		data2[i] = 'x';
	}
	pack = new S3TP_PACKET();
	memcpy(pack->pdu, data2, s);
	pack->hdr.pdu_length = (uint16_t)s;
	pack->hdr.setPort(20);
	pack->hdr.seq = (uint16_t )((test.global_seq_num << 8) | 1);
	pack->hdr.seq_port = 2;
	wrapper = new S3TP_PACKET_WRAPPER();
	wrapper->channel = 0;
	wrapper->pkt = pack;
	q->push(wrapper, &test);

	s = 992;
	data2 = new char[s];
	for (int i=0; i<s; i++) {
		data2[i] = 'w';
	}
	pack = new S3TP_PACKET();
	memcpy(pack->pdu, data2, s);
	pack->hdr.pdu_length = (uint16_t)s;
	pack->hdr.setPort(20);
	pack->hdr.seq = (uint16_t)((test.global_seq_num << 8) | 0);
	pack->hdr.seq_port = 1;
	wrapper = new S3TP_PACKET_WRAPPER();
	wrapper->channel = 0;
	wrapper->pkt = pack;
	q->push(wrapper, &test);
	test.global_seq_num++;

	printf("Q size: %d\n", q->getSize());
}

void queueTest() {
	MyTest test;
	PriorityQueue<S3TP_PACKET_WRAPPER*> * q = new PriorityQueue<S3TP_PACKET_WRAPPER*>();
	S3TP_PACKET * pack;
	S3TP_PACKET_WRAPPER * wrapper;
	for (uint16_t i=1050; i>750; i--) {
		pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		wrapper = new S3TP_PACKET_WRAPPER();
		wrapper->channel = 0;
		wrapper->pkt = pack;
		q->push(wrapper, &test);
	}
	//Packet to be put at end of q
	pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1060;
	wrapper = new S3TP_PACKET_WRAPPER();
	wrapper->channel = 0;
	wrapper->pkt = pack;
	q->push(wrapper, &test);

	//Packet one place before end of q
	pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1059;
	wrapper = new S3TP_PACKET_WRAPPER();
	wrapper->channel = 0;
	wrapper->pkt = pack;
	q->push(wrapper, &test);

	//Packet one place after end of q
	pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
	pack->hdr.port = 50;
	pack->hdr.seq = 1100;
	wrapper = new S3TP_PACKET_WRAPPER();
	wrapper->channel = 0;
	wrapper->pkt = pack;
	q->push(wrapper, &test);

	for (uint16_t i=1; i<=500; i++) {
		pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		wrapper = new S3TP_PACKET_WRAPPER();
		wrapper->channel = 0;
		wrapper->pkt = pack;
		q->push(wrapper, &test);
	}
	for (uint16_t i=750; i>500; i--) {
		pack = (S3TP_PACKET *) calloc(1, sizeof(S3TP_PACKET));
		pack->hdr.port = 50;
		pack->hdr.seq = i;
		wrapper = new S3TP_PACKET_WRAPPER();
		wrapper->channel = 0;
		wrapper->pkt = pack;
		q->push(wrapper, &test);
	}
	uint32_t bs = q->computeBufferSize();
	printf("Currently %d elements in queue. Buffer size: %d\n", q->getSize(), (int)bs);

	//Now emptying q
	while (q->peek() != NULL) {
		wrapper = q->pop();
		pack = wrapper->pkt;
		printf("Popped element with seq num %d\n", pack->hdr.seq);
		free(pack);
		free(wrapper);
	}
	printf("Queue is empty now!\n");
	delete q;
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
		tx->enqueuePacket(packet, 0, false, 0, default_opts);
	}
	for (uint16_t i=100; i<110; i++) {
		S3TP_PACKET * packet = new S3TP_PACKET();
		packet->hdr.port = appPort;
		printf("Enqueuing packet %d...\n", i);
		tx->enqueuePacket(packet, 0, false, 0, default_opts);
		sleep(sleepSeconds);
	}
	for (uint16_t i=110; i<120; i++) {
		S3TP_PACKET * packet = new S3TP_PACKET();
		packet->hdr.port = appPort;
		printf("Enqueuing packet %d...\n", i);
		tx->enqueuePacket(packet, 0, false, 0, default_opts);
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
	S3TP * main = (S3TP *)args;
	uint8_t appPort = (uint8_t) (rand() % DEFAULT_MAX_OUT_PORTS);
	int len, i, result;
	char * message;
	for (i=0; i<5; i++) {
		len = rand() % LEN_S3TP_PDU;
		message = new char[len];
		result = main->sendToLinkLayer(0, appPort, message, (size_t)len, default_opts);
		if (result != CODE_SUCCESS) {
			printf("Error in sending message\n");
		}
	}
	//Message to fragment
	len = (rand() % (LEN_S3TP_PDU * 2)) + 2000;
	message = new char[len];
	result = main->sendToLinkLayer(0, appPort, message, (size_t)len, default_opts);
	if (result != CODE_SUCCESS) {
		printf("Error in sending message\n");
	}
	for (i=0; i<5; i++) {
		len = rand() % LEN_S3TP_PDU;
		message = new char[len];
		result = main->sendToLinkLayer(0, appPort, message, (size_t)len, default_opts);
		if (result != CODE_SUCCESS) {
			printf("Error in sending message\n");
		}
	}
	pthread_exit(NULL);
}

void s3tpMainModuleTest() {
	S3TP main;
	//main.init();
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
	int result = -1; //testDaemon.init();
	if (result != 0) {
		LOG_DBG_S3TP("Couldn't init daemon. Exiting..\n");
	}
	testDaemon.startDaemon();
    LOG_DBG_S3TP("Daemon killed\n");
}

int main(int argc, char**argv) {
	srand(time(NULL));
	queueIntegrityTest();
	//queueTest();
	//txModuleTest();
	//simpleQueueTest();
	//s3tpMainModuleTest();
	//crcTest();
	//daemonTest();
}
