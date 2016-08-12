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



qnode_t * init_queue ()
{
	qnode_t* q = (qnode_t*) calloc (1, sizeof(qnode_t));
	q->q_mutex = PTHREAD_MUTEX_INITIALIZER;
	return q;
}


void push (
		qnode_t* head,
		S3TP_PACKET* data)
{
	int i;
	qnode_t *ref = head;
	qnode_t * new, tmp;

	pthread_mutex_lock(&head->q_mutex);

	while(1)
	{
		if(ref->next ==0)
		{
			//either we are at the last node;
			break;
		}else if (ref->next->seq > data->hdr->seq)
		{
			break;
			//or our seq number ist lower than the next one, reordering

		}
		ref = ref->next;
	}

	//create a new node
	new = (qnode_t*) calloc(1, sizeof(qnode_t));
	new->seq = data->hdr->seq;

	//insert here
	tmp = ref->next;
	ref->next = new;
	new->next = tmp;

	pthread_mutex_unlock(&head->q_mutex);

}

S3TP_PACKET* pop (
		qnode_t* head)
{
	qnode_t* tmp;
	S3TP_PACKET* pack;

	pthread_mutex_lock(&head->q_mutex);

	//get the lowest seq packet and remove it from queue
	tmp = head->next;
	head->next = head->next->next;
	pack = tmp->payload;

	free(tmp);

	pthread_mutex_unlock(&head->q_mutex);

	return pack;
}

void deinit_queue(
		qnode_t* head)
{
	qnode_t* ref;
	while(head!=0)
	{
		ref = head;
		head = head->next;
		free(ref);
	}
}
