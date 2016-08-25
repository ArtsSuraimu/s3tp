//
// Created by Lorenzo Donini on 24/08/16.
//

#include "Buffer.h"

//Ctor
Buffer::Buffer() {
    pthread_cond_init(&new_content_cond, NULL);
}

//Dtor
Buffer::~Buffer() {
    pthread_mutex_lock(&buffer_mutex);
    for (std::map<int, qhead_t *>::iterator it = queues.begin(); it != queues.end(); ++it) {
        delete it->second;
    }
    queues.clear();
    message_map.clear();
    pthread_cond_destroy(&new_content_cond);
    pthread_mutex_destroy(&buffer_mutex);
}

bool Buffer::packetsAvailable() {
    return !message_map.empty();
}

int Buffer::write(int port, S3TP_PACKET * data) {
    pthread_mutex_lock(&buffer_mutex);
    qhead_t * queue = queues[port];
    if (queue == NULL) {
        //Adding new queue to the internal map
        queue = new qhead_t();
        queues[port] = queue;
    }
    push(queue, data);
    message_map[port] = queue->size;
    printf("Port %d: packet %d written\n", port, data->hdr.seq);
    pthread_cond_signal(&new_content_cond);
    pthread_mutex_unlock(&buffer_mutex);
    return 0;
}

qhead_t * Buffer::getQueue(int port) {
    pthread_mutex_lock(&buffer_mutex);
    qhead_t * queue = queues[port];
    pthread_mutex_unlock(&buffer_mutex);
    return queue;
}

S3TP_PACKET * Buffer::getNextPacket(int port) {
    pthread_mutex_lock(&buffer_mutex);
    S3TP_PACKET * packet = popPacketInternal(port);
    pthread_mutex_unlock(&buffer_mutex);
    return packet;
}

S3TP_PACKET * Buffer::getNextAvailablePacket() {
    pthread_mutex_lock(&buffer_mutex);
    if (message_map.empty()) {
        pthread_mutex_unlock(&buffer_mutex);
        return NULL;
    }
    std::map<int, int>::iterator it = message_map.begin();
    S3TP_PACKET * packet = popPacketInternal(it->first);
    pthread_mutex_unlock(&buffer_mutex);
    return packet;
}

S3TP_PACKET * Buffer::popPacketInternal(int port) {
    qhead_t * queue = queues[port];
    if (queue == NULL || isEmpty(queue)) {
        return NULL;
    }
    S3TP_PACKET * packet = pop(queue);
    if (isEmpty(queue)) {
        message_map.erase(port);
    } else {
        message_map[port] = queue->size;
    }
    return packet;
}