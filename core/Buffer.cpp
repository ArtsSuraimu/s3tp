//
// Created by Lorenzo Donini on 24/08/16.
//

#include "Buffer.h"

//Ctor
Buffer::Buffer() {
    pthread_mutex_init(&buffer_mutex, NULL);
}

//Dtor
Buffer::~Buffer() {
    pthread_mutex_lock(&buffer_mutex);
    for (std::map<int, PriorityQueue *>::iterator it = queues.begin(); it != queues.end(); ++it) {
        delete it->second;
    }
    queues.clear();
    packet_counter.clear();
    pthread_mutex_unlock(&buffer_mutex);
    pthread_mutex_destroy(&buffer_mutex);
}

bool Buffer::packetsAvailable() {
    pthread_mutex_lock(&buffer_mutex);
    //Packets are available if message map is not empty
    bool result = !packet_counter.empty();
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
    packet_counter[port] = queue->size;
    printf("BUFFER port %d: packet %d written (%d bytes)\n", (port & 0x7F), packet->pkt->hdr.seq, packet->pkt->hdr.pdu_length);
    pthread_mutex_unlock(&buffer_mutex);

    return CODE_SUCCESS;
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
    if (packet_counter.empty()) {
        pthread_mutex_unlock(&buffer_mutex);
        return NULL;
    }
    std::map<int, int>::iterator it = packet_counter.begin();
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
        packet_counter.erase(port);
    } else {
        packet_counter[port] = queue->size;
    }
    return packet;
}