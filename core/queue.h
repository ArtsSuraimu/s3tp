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

typedef struct tag_mini_queue_node qnode_t;

struct tag_mini_queue_node{
	S3TP_PACKET* payload;
	qnode_t * next;
	int seq;
};


void* init_queue ();
void push (qnode_t *head, S3TP_PACKET* data);
S3TP_PACKET* pop ();
void deinit_queue(qnode_t* head);

#endif /* CORE_QUEUE_H_ */
