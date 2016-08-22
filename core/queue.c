/*
 * queue.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#include "queue.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>



void * init_queue () {
	qhead_t* q = (qhead_t*) calloc(1, sizeof(qhead_t));
	if (pthread_mutex_init(&q->q_mutex, NULL) != 0)
	{
		printf("\n mutex init failed\n");
		return NULL;
	}
	return q;
}

void push (qhead_t* root, S3TP_PACKET* data) {
	qnode_t *ref, *newNode, *swap;

	//Enter critical section
	pthread_mutex_lock(&root->q_mutex);

	if (root->size >= MAX_QUEUE_CAPACITY) {
		//Queue is full, dropping new packet
		printf("Queue is full, dropping packet with sequence number %d\n", data->hdr.seq);
		//Exit critical section
		pthread_mutex_unlock(&root->q_mutex);
		return;
	}

	//Creating new node
	newNode = (qnode_t*) calloc(1, sizeof(qnode_t));
	newNode->seq = data->hdr.seq;
	newNode->payload = data;

	//Inserting new node inside the priority queue
	ref = root->tail;
	while (1) {
		if (ref == NULL) {
			//We are at the head of the queue. This is due to the queue being empty.
			root->head = newNode;
			root->tail = newNode;
			break;
		}
		else if (ref->seq < data->hdr.seq) {
			//New node has higher sequence number than current element. New node has lower priority -> append it here
			swap = ref->next;
			ref->next = newNode;
			newNode->prev = ref;
			newNode->next = swap;
			if (swap != NULL) {
				swap->prev = newNode;
			} else {
				root->tail = newNode;
			}
			break;
		} else if (ref->prev == NULL) {
			//We are at the head of the queue
			ref->prev = newNode;
			newNode->next = ref;
			root->head = newNode;
			break;
		}
		//Current sequence number is higher than this one. New node has higher priority
		ref = ref->prev;
	}

	//Increase current buffer size
	root->size += 1;

	//Exit critical section
	pthread_mutex_unlock(&root->q_mutex);
}

S3TP_PACKET* peek (qhead_t* root) {
	qnode_t * head = root->head;
	//Returning payload if head is not null, otherwise return null
	return (head != NULL) ? head->payload : NULL;
}

S3TP_PACKET* pop (qhead_t* root) {
	qnode_t* ref;
	S3TP_PACKET* pack;

	//Entering critical section
	pthread_mutex_lock(&root->q_mutex);

	//get the lowest seq packet and remove it from queue
	ref = root->head;
	if (ref == NULL) {
		return NULL;
	}

	if (ref->next == NULL) {
		//This is the only element in the queue
		root->tail = NULL;
		root->head = NULL;
	} else {
		root->head = ref->next;
		ref->next->prev = NULL;
	}

	pack = ref->payload;
	free(ref);

	//Decrease current buffer size
	root->size -= 1;

	//Exiting critical section
	pthread_mutex_unlock(&root->q_mutex);

	return pack;
}

void deinit_queue(qhead_t* root)
{
	qnode_t* ref = root->head;
	while (ref != NULL) {
		root->head = ref->next;
		free(ref);
		ref = root->head;
	}
	free(root);
}

u32 computeBufferSize(qhead_t* root) {
	return root->size * sizeof(S3TP_PACKET);
}
