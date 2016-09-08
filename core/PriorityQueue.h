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

#include "CommonTypes.h"
#include "PriorityComparator.h"
#include <pthread.h>
#include <assert.h>

#define MB 1 << 20
#define MAX_QUEUE_SIZE (1*MB)
#define MAX_QUEUE_CAPACITY (MAX_QUEUE_SIZE / sizeof(S3TP_PACKET))
#define QUEUE_FULL -1

template <typename T>
struct PriorityQueue_node {
	T element;
	PriorityQueue_node<T> * next;
	PriorityQueue_node<T> * prev;
};

template <typename T>
struct PriorityQueue {
public:
	PriorityQueue();
	~PriorityQueue();
	T pop();
	T peek();
	bool isEmpty();
	int push(T element, PriorityComparator<T> * comparator);
	uint32_t computeBufferSize();
	uint16_t getSize();
	void lock();
	void unlock();
	PriorityQueue_node<T> * getHead();

private:
	PriorityQueue_node<T> * head;
	PriorityQueue_node<T> * tail;
	pthread_mutex_t q_mutex;
	uint16_t size;
};

/*PriorityQueue * init_queue ();
int push (PriorityQueue *root, S3TP_PACKET_WRAPPER* packet);
S3TP_PACKET_WRAPPER* pop (PriorityQueue* root);
S3TP_PACKET_WRAPPER* peek (PriorityQueue* root);
void deinit_queue (PriorityQueue* root);
uint32_t computeBufferSize (PriorityQueue* root);
bool isEmpty(PriorityQueue * root);*/


template <typename T>
PriorityQueue<T>::PriorityQueue() {
	size = 0;
	pthread_mutex_init(&q_mutex, NULL);
}

template <typename T>
PriorityQueue<T>::~PriorityQueue() {
	pthread_mutex_lock(&q_mutex);
	PriorityQueue_node<T> * ref = head;
	while (ref != NULL) {
		head = ref->next;
		delete ref;
		ref = head;
	}
	pthread_mutex_unlock(&q_mutex);
	pthread_mutex_destroy(&q_mutex);
}

template <typename T>
bool PriorityQueue<T>::isEmpty() {
	pthread_mutex_lock(&q_mutex);
	bool result = size == 0;
	pthread_mutex_unlock(&q_mutex);
	return result;
}

template <typename T>
T PriorityQueue<T>::peek() {
	assert(!isEmpty());
	pthread_mutex_lock(&q_mutex);
	T result = head->element;
	pthread_mutex_unlock(&q_mutex);
	return result;
}

template <typename T>
T PriorityQueue<T>::pop() {
	PriorityQueue_node<T> * ref;
	T element;

	//get the lowest seq packet and remove it from queue
	assert(!isEmpty());

    //Entering critical section
    pthread_mutex_lock(&q_mutex);
	ref = head;

	if (ref->next == NULL) {
		//This is the only element in the queue
		tail = NULL;
		head = NULL;
	} else {
		head = ref->next;
		ref->next->prev = NULL;
	}

	element = ref->element;
	delete ref;

	//Decrease current buffer size
	size -= 1;

	//Exiting critical section
	pthread_mutex_unlock(&q_mutex);

	return element;
}

template <typename T>
int PriorityQueue<T>::push(T element, PriorityComparator<T> * comparator) {
	PriorityQueue_node<T> *ref, *newNode, *swap;

	//Enter critical section
	pthread_mutex_lock(&q_mutex);

	if (size >= MAX_QUEUE_CAPACITY) {
		//Queue is full, dropping new element
		//printf("Queue is full, dropping packet with sequence number %d\n", data->hdr.seq);
		//Exit critical section
		pthread_mutex_unlock(&q_mutex);
		return QUEUE_FULL;
	}

	//Creating new node
	newNode = (PriorityQueue_node<T>*) calloc(1, sizeof(PriorityQueue_node<T>));
	newNode->element = element;

	//Inserting new node inside the priority queue
	ref = tail;
	while (1) {
		if (ref == NULL) {
			//We are at the head of the queue. This is due to the queue being empty.
			head = newNode;
			tail = newNode;
			break;
		} else if (comparator->comparePriority(ref->element, element) < 0) {
            //First element (old) has higher priority than new element -> append the new element here
			swap = ref->next;
			ref->next = newNode;
			newNode->prev = ref;
			newNode->next = swap;
			if (swap != NULL) {
				swap->prev = newNode;
			} else {
				tail = newNode;
			}
			break;
		} else if (ref->prev == NULL) {
			//We are at the head of the queue
			ref->prev = newNode;
			newNode->next = ref;
			head = newNode;
			break;
		}
		//New node has higher priority. Keep looking for right position in q
		ref = ref->prev;
	}

	//Increase current buffer size
	size += 1;

	//Exit critical section
	pthread_mutex_unlock(&q_mutex);

	return 0;
}

template <typename T>
uint32_t PriorityQueue<T>::computeBufferSize() {
	pthread_mutex_lock(&q_mutex);
	uint32_t result = size * sizeof(S3TP_PACKET);
	pthread_mutex_unlock(&q_mutex);
	return result;
}

template <typename T>
uint16_t PriorityQueue<T>::getSize() {
	pthread_mutex_lock(&q_mutex);
	uint16_t result = size;
	pthread_mutex_unlock(&q_mutex);
	return result;
}

template <typename T>
void PriorityQueue<T>::lock() {
	pthread_mutex_lock(&q_mutex);
}

template <typename T>
void PriorityQueue<T>::unlock() {
	pthread_mutex_unlock(&q_mutex);
}

template <typename T>
PriorityQueue_node<T> * PriorityQueue<T>::getHead() {
	return head;
}

/*	//TODO: implement properly, with correct seq check
		else if (ref->element->pkt->hdr.seq < data->hdr.seq) {
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
			break;*/



#endif /* CORE_QUEUE_H_ */
