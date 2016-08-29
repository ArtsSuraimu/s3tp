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
    for (std::map<int, PriorityQueue *>::iterator it = queues.begin(); it != queues.end(); ++it) {
        delete it->second;
    }
    queues.clear();
    message_map.clear();
    pthread_cond_destroy(&new_content_cond);
    pthread_mutex_destroy(&buffer_mutex);
}

bool Buffer::packetsAvailable() {
    pthread_mutex_lock(&buffer_mutex);
    bool result = message_map.empty();
    pthread_mutex_unlock(&buffer_mutex);
    return result;
}

int Buffer::write(S3TP_PACKET_WRAPPER * packet) {
    pthread_mutex_lock(&buffer_mutex);
    int port = packet->pkt->hdr.port;

    PriorityQueue * queue = queues[port];
    if (queue == NULL) {
        //Adding new queue to the internal map
        queue = init_queue();
        queues[port] = queue;
    }
    push(queue, packet);
    message_map[port] = queue->size;
    printf("Port %d: packet %d written\n", port, packet->pkt->hdr.seq);
    pthread_cond_signal(&new_content_cond);
    pthread_mutex_unlock(&buffer_mutex);
    return 0;
}

PriorityQueue * Buffer::getQueue(int port) {
    pthread_mutex_lock(&buffer_mutex);
    PriorityQueue * queue = queues[port];
    pthread_mutex_unlock(&buffer_mutex);
    return queue;
}

S3TP_PACKET_WRAPPER * Buffer::getNextPacket(int port) {
    pthread_mutex_lock(&buffer_mutex);
    S3TP_PACKET_WRAPPER * packet = popPacketInternal(port);
    pthread_mutex_unlock(&buffer_mutex);
    return packet;
}

S3TP_PACKET_WRAPPER * Buffer::getNextAvailablePacket() {
    pthread_mutex_lock(&buffer_mutex);
    if (message_map.empty()) {
        pthread_mutex_unlock(&buffer_mutex);
        return NULL;
    }
    std::map<int, int>::iterator it = message_map.begin();
    S3TP_PACKET_WRAPPER * packet = popPacketInternal(it->first);
    pthread_mutex_unlock(&buffer_mutex);
    return packet;
}

S3TP_PACKET_WRAPPER * Buffer::popPacketInternal(int port) {
    PriorityQueue * queue = queues[port];
    if (queue == NULL || isEmpty(queue)) {
        return NULL;
    }
    S3TP_PACKET_WRAPPER * packet = pop(queue);
    if (isEmpty(queue)) {
        message_map.erase(port);
    } else {
        message_map[port] = queue->size;
    }
    return packet;
}