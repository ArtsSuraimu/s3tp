/*
 * queue.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#include "PriorityQueue.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>


PriorityQueue * init_queue () {
	PriorityQueue* q = (PriorityQueue*) calloc(1, sizeof(PriorityQueue));
	if (pthread_mutex_init(&q->q_mutex, NULL) != 0)
	{
		printf("\n mutex init failed\n");
		return NULL;
	}
	return q;
}

int push (PriorityQueue* root, S3TP_PACKET_WRAPPER* packet) {
	PriorityQueue_node *ref, *newNode, *swap;
	S3TP_PACKET * data = packet->pkt;

	//Enter critical section
	pthread_mutex_lock(&root->q_mutex);

	if (root->size >= MAX_QUEUE_CAPACITY) {
		//Queue is full, dropping new packet
		printf("Queue is full, dropping packet with sequence number %d\n", data->hdr.seq);
		//Exit critical section
		pthread_mutex_unlock(&root->q_mutex);
		return QUEUE_FULL;
	}

	//Creating new node
	newNode = (PriorityQueue_node*) calloc(1, sizeof(PriorityQueue_node));
	newNode->payload = packet;

	//Inserting new node inside the priority queue
	ref = root->tail;
	while (1) {
		if (ref == NULL) {
			//We are at the head of the queue. This is due to the queue being empty.
			root->head = newNode;
			root->tail = newNode;
			break;
		}
			//TODO: implement properly, with correct seq check
		else if (ref->payload->pkt->hdr.seq < data->hdr.seq) {
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

	return 0;
}

S3TP_PACKET_WRAPPER* peek (PriorityQueue* root) {
	S3TP_PACKET_WRAPPER * pack = NULL;

	pthread_mutex_lock(&root->q_mutex);
	PriorityQueue_node * head = root->head;
	if (head != NULL) {
		pack = head->payload;
	}
	pthread_mutex_unlock(&root->q_mutex);

	//Returning payload if head is not null, otherwise return null
	return pack;
}

S3TP_PACKET_WRAPPER* pop (PriorityQueue* root) {
	PriorityQueue_node* ref;
	S3TP_PACKET_WRAPPER* pack;

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

void deinit_queue(PriorityQueue* root) {
	PriorityQueue_node* ref = root->head;
	while (ref != NULL) {
		root->head = ref->next;
		free(ref);
		ref = root->head;
	}
	free(root);
}

u32 computeBufferSize(PriorityQueue* root) {
	return root->size * sizeof(S3TP_PACKET);
}

bool isEmpty(PriorityQueue * root) {
	bool result;
	pthread_mutex_lock(&root->q_mutex);
	result = root->size == 0;
	pthread_mutex_unlock(&root->q_mutex);
	return result;
}
