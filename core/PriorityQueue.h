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
#include "PolicyActor.h"
#include <pthread.h>
#include <cassert>

#define MB 1 << 20
#define MAX_QUEUE_SIZE (1*MB)
#define MAX_QUEUE_CAPACITY (MAX_QUEUE_SIZE / sizeof(S3TP_PACKET))
#define QUEUE_FULL -1

template <typename T>
struct PriorityQueue_node {
	T element;
	PriorityQueue_node<T> * next;
	PriorityQueue_node<T> * prev;

	PriorityQueue_node(T element);
};

template <typename T>
struct PriorityQueue {
public:
	PriorityQueue();
	~PriorityQueue();
	T pop();
	T peek();
	bool isEmpty();
	int push(T element, PolicyActor<T> * comparator);
	uint32_t computeBufferSize();
	uint16_t getSize();
	void clear();
	void lock();
	void unlock();
	PriorityQueue_node<T> * getHead();

private:
	PriorityQueue_node<T> * head;
	PriorityQueue_node<T> * tail;
	std::mutex qMutex;
	uint16_t size;
};


/*
 * Implementation
 */

template <typename T>
PriorityQueue_node<T>::PriorityQueue_node(T element) :
	element(element),
	next(nullptr),
	prev(nullptr)
{

}

template <typename T>
PriorityQueue<T>::PriorityQueue() :
	head(nullptr),
	tail(nullptr),
	size(0) {}

template <typename T>
PriorityQueue<T>::~PriorityQueue() {
	clear();
}

template <typename T>
bool PriorityQueue<T>::isEmpty() {
	std::unique_lock<std::mutex> lock{qMutex};

	return size == 0;
}

template <typename T>
T PriorityQueue<T>::peek() {
	assert(!isEmpty());
	std::unique_lock<std::mutex> lock{qMutex};

	return head->element;
}

template <typename T>
T PriorityQueue<T>::pop() {
	PriorityQueue_node<T> * ref;
	T element;

	//get the lowest seq packet and remove it from queue
	assert(!isEmpty());

    //Entering critical section
	std::unique_lock<std::mutex> lock{qMutex};
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

	return element;
}

template <typename T>
int PriorityQueue<T>::push(T element, PolicyActor<T> * comparator) {
	PriorityQueue_node<T> *ref, *newNode, *swap;

	//Enter critical section
	std::unique_lock<std::mutex> lock{qMutex};

	if (size >= MAX_QUEUE_CAPACITY) {
		//Queue is full, dropping new element
		//Exit critical section
		return QUEUE_FULL;
	}

	//Creating new node
	newNode = new PriorityQueue_node<T>(element);

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

	return CODE_SUCCESS;
}

template <typename T>
uint32_t PriorityQueue<T>::computeBufferSize() {
	std::unique_lock<std::mutex> lock {qMutex};

	return size * sizeof(S3TP_PACKET);
}

template <typename T>
uint16_t PriorityQueue<T>::getSize() {
	std::unique_lock<std::mutex> lock{qMutex};

	return size;
}

template <typename T>
void PriorityQueue<T>::clear() {
	std::unique_lock<std::mutex> lock{qMutex};
	PriorityQueue_node<T> * ref = head;
	while (ref != NULL) {
		head = ref->next;
		delete ref;
		//Not deleting element. Smart pointers are expected
		ref = head;
	}
	tail = head;
	size = 0;
}

template <typename T>
void PriorityQueue<T>::lock() {
	qMutex.lock();
}

template <typename T>
void PriorityQueue<T>::unlock() {
	qMutex.unlock();
}

template <typename T>
PriorityQueue_node<T> * PriorityQueue<T>::getHead() {
	return head;
}

#endif /* CORE_QUEUE_H_ */
