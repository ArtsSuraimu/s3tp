//
// Created by Lorenzo Donini on 26/08/16.
//

#ifndef S3TP_SIMPLEQUEUE_H
#define S3TP_SIMPLEQUEUE_H

#include "s3tp_types.h"
#include <pthread.h>
#include <assert.h>

#define SIMPLE_QUEUE_DEFAULT_CAPACITY 256

#define SIMPLE_QUEUE_BLOCKING 1
#define SIMPLE_QUEUE_NON_BLOCKING 0

#define SIMPLE_QUEUE_FULL -1
#define SIMPLE_QUEUE_PUSH_SUCCESS 0

/*
 * STRUCT DEFINITION
 */
template <typename T>
struct tag_s3tp_simple_queue_node {
    T element;
    tag_s3tp_simple_queue_node * next;
};

template <typename T>
struct SimpleQueue {
public:
    SimpleQueue(int capacity, int blocking);
    ~SimpleQueue();
    int push(const T& element);
    bool isEmpty();
    T& pop();

private:
    tag_s3tp_simple_queue_node<T> * head;
    tag_s3tp_simple_queue_node<T> * tail;
    int size;
    int capacity;
    int mode;
    pthread_mutex_t q_mutex;
    pthread_cond_t content_add_cond;
    pthread_cond_t content_rmv_cond;
};


/*
 * IMPLEMENTATION
 */

template <typename T>
SimpleQueue<T>::SimpleQueue(int capacity, int mode) {
    head = NULL;
    tail = NULL;
    this->capacity = (capacity > 0) ? capacity : SIMPLE_QUEUE_DEFAULT_CAPACITY;
    size = 0;
    this->mode = mode;
    pthread_mutex_init(&q_mutex, NULL);
    pthread_cond_init(&content_add_cond, NULL);
    pthread_cond_init(&content_rmv_cond, NULL);
}

template <typename T>
SimpleQueue<T>::~SimpleQueue() {
    pthread_mutex_lock(&q_mutex);
    tag_s3tp_simple_queue_node<T> * node = head;
    while (node != NULL) {
        head = node->next;
        delete node;
        node = head;
    }
    pthread_mutex_destroy(&q_mutex);
}

template <typename T>
int SimpleQueue<T>::push(const T &element) {
    pthread_mutex_lock(&q_mutex);
    if (size == capacity) {
        if (mode == SIMPLE_QUEUE_BLOCKING) {
            //Wait for queue to free some space
            pthread_cond_wait(&content_rmv_cond, &q_mutex);
        } else {
            //Don't wait, just return error code
            pthread_mutex_unlock(&q_mutex);
            return SIMPLE_QUEUE_FULL;
        }
    }
    tag_s3tp_simple_queue_node<T> * node = new tag_s3tp_simple_queue_node<T>();
    node->element = element;
    if (head == NULL) {
        head = node;
    } else {
        tail->next = node;
    }
    tail = node;
    size++;

    pthread_cond_signal(&content_add_cond);
    pthread_mutex_unlock(&q_mutex);
    return SIMPLE_QUEUE_PUSH_SUCCESS;
}

template <typename T>
bool SimpleQueue<T>::isEmpty() {
    pthread_mutex_lock(&q_mutex);
    bool result = size == 0;
    pthread_mutex_unlock(&q_mutex);
    return result;
}

template <typename T>
T& SimpleQueue<T>::pop() {
    pthread_mutex_lock(&q_mutex);
    if (size == 0) {
        if (mode == SIMPLE_QUEUE_BLOCKING) {
            pthread_cond_wait(&content_add_cond, &q_mutex);
        } else {
            assert(head != NULL);
        }
    }

    tag_s3tp_simple_queue_node<T> * node = head;
    head = node->next;
    T& result = node->element;
    delete node;
    size--;
    pthread_cond_signal(&content_rmv_cond);
    pthread_mutex_unlock(&q_mutex);
    return result;
}

#endif //S3TP_SIMPLEQUEUE_H
