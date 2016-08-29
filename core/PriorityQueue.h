/*
 * queue.h
 *
 * Mini Priority Queue used for the reordering of incoming packets.
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#ifndef CORE_QUEUE_H_
#define CORE_QUEUE_H_

#include "s3tp_types.h"
#include <pthread.h>

#define MB 1 << 20
#define MAX_QUEUE_SIZE (1*MB)
#define MAX_QUEUE_CAPACITY (MAX_QUEUE_SIZE / sizeof(S3TP_PACKET))
#define QUEUE_FULL -1

typedef struct tag_s3tp_queue_root PriorityQueue;
typedef struct tag_s3tp_queue_node PriorityQueue_node;

struct tag_s3tp_queue_root {
	PriorityQueue_node * head;
	PriorityQueue_node * tail;
	pthread_mutex_t q_mutex;
	u16 size;
};

struct tag_s3tp_queue_node {
	S3TP_PACKET_WRAPPER * payload;
	PriorityQueue_node * next;
	PriorityQueue_node * prev;
};

PriorityQueue * init_queue ();
int push (PriorityQueue *root, S3TP_PACKET_WRAPPER* packet);
S3TP_PACKET_WRAPPER* pop (PriorityQueue* root);
S3TP_PACKET_WRAPPER* peek (PriorityQueue* root);
void deinit_queue (PriorityQueue* root);
u32 computeBufferSize (PriorityQueue* root);
bool isEmpty(PriorityQueue * root);

#endif /* CORE_QUEUE_H_ */
