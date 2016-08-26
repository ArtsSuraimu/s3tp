//
// Created by Lorenzo Donini on 26/08/16.
//

#ifndef S3TP_SIMPLEQUEUE_H
#define S3TP_SIMPLEQUEUE_H

#include "s3tp_types.h"
#include <pthread.h>
#include <assert.h>

#define SIMPLE_QUEUE_DEFAULT_CAPACITY 256
#define SIMPLE_QUEUE_FULL -1
#define SIMPLE_QUEUE_PUSH_SUCCESS 0

typedef struct tag_s3tp_raw_data RawData;

struct tag_s3tp_raw_data {
    void * data;
    size_t len;
    u8 port;
    u8 channel;

    //Default ctor
    tag_s3tp_raw_data() {}

    //Custom ctor
    tag_s3tp_raw_data(void * data, size_t len, u8 port, u8 channel) {
        this->data = data;
        this->len = len;
        this->port = port;
        this->channel = channel;
    }

    //Copy ctor
    tag_s3tp_raw_data(const tag_s3tp_raw_data& data) {
        this->data = data.data;
        this->len = data.len;
        this->port = data.port;
        this->channel = data.channel;
    }
};

template <typename T>
struct tag_s3tp_simple_queue_node {
    T element;
    tag_s3tp_simple_queue_node * next;
};

template <typename T>
struct SimpleQueue {
public:
    SimpleQueue(int capacity);
    ~SimpleQueue();
    int push(const T& element);
    bool isEmpty();
    T& pop();

private:
    tag_s3tp_simple_queue_node<T> * head;
    tag_s3tp_simple_queue_node<T> * tail;
    int size;
    int capacity;
    pthread_mutex_t q_mutex;
};


/*
 * IMPLEMENTATION
 */

template <typename T>
SimpleQueue<T>::SimpleQueue(int capacity) {
    head = NULL;
    tail = NULL;
    this->capacity = (capacity > 0) ? capacity : SIMPLE_QUEUE_DEFAULT_CAPACITY;
    size = 0;
    pthread_mutex_init(&q_mutex, NULL);
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
        pthread_mutex_unlock(&q_mutex);
        return SIMPLE_QUEUE_FULL;
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

    pthread_mutex_unlock(&q_mutex);
    return SIMPLE_QUEUE_PUSH_SUCCESS;
}

template <typename T>
bool SimpleQueue<T>::isEmpty() {
    return size == 0;
}

template <typename T>
T& SimpleQueue<T>::pop() {
    assert(head != NULL);

    tag_s3tp_simple_queue_node<T> * node = head;
    head = node->next;
    T& result = node->element;
    delete node;
    size--;
    return result;
}

#endif //S3TP_SIMPLEQUEUE_H
