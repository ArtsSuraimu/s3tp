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
#define MAX_QUEUE_CAPACITY (1*MB) / sizeof(S3TP_PACKET)

typedef struct tag_mini_queue_root qhead_t;
typedef struct tag_mini_queue_node qnode_t;

struct tag_mini_queue_root {
	qnode_t * head;
	qnode_t * tail;
	pthread_mutex_t q_mutex;
	u16 size;
};

struct tag_mini_queue_node {
	S3TP_PACKET* payload;
	qnode_t * next;
	qnode_t * prev;
	int seq;
};

void* init_queue ();
void push (qhead_t *root, S3TP_PACKET* data);
S3TP_PACKET* pop (qhead_t* root);
S3TP_PACKET* peek (qhead_t* root);
void deinit_queue (qhead_t* root);
u32 computeBufferSize (qhead_t* root);
bool isEmpty(qhead_t * root);

#endif /* CORE_QUEUE_H_ */
