//
// Created by Lorenzo Donini on 24/08/16.
//

#include "Buffer.h"

//Ctor
Buffer::Buffer(PriorityComparator<S3TP_PACKET*> * comparator) {
    pthread_mutex_init(&buffer_mutex, NULL);
    this->comparator = comparator;
}

//Dtor
Buffer::~Buffer() {
    pthread_mutex_lock(&buffer_mutex);
    for (std::map<int, PriorityQueue<S3TP_PACKET*> *>::iterator it = queues.begin(); it != queues.end(); ++it) {
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

int Buffer::write(S3TP_PACKET * packet) {
    pthread_mutex_lock(&buffer_mutex);
    S3TP_HEADER * hdr = packet->getHeader();
    int port = hdr->getPort();

    PriorityQueue<S3TP_PACKET*> * queue = queues[port];
    if (queue == NULL) {
        //Adding new queue to the internal map
        queue = new PriorityQueue<S3TP_PACKET*>();
        queues[port] = queue;
    }
    if (queue->push(packet, comparator) == QUEUE_FULL) {
        LOG_INFO(std::string("Queue " + std::to_string(port)
                              + " full. Dropped packet with sequence number "
                              + std::to_string((int)hdr->seq_port)));
    } else {
        packet_counter[port] = queue->getSize();
        LOG_DEBUG(std::string("Queue " + std::to_string(port)
                              + ": packet "
                              + std::to_string((int)hdr->seq_port) + " written ("
                              + std::to_string((int)hdr->getPduLength()) + " bytes)"));
    }

    pthread_mutex_unlock(&buffer_mutex);

    return CODE_SUCCESS;
}

PriorityQueue<S3TP_PACKET *> * Buffer::getQueue(int port) {
    pthread_mutex_lock(&buffer_mutex);
    PriorityQueue<S3TP_PACKET*> * queue = queues[port];
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
    if (packet_counter.empty()) {
        pthread_mutex_unlock(&buffer_mutex);
        return NULL;
    }
    std::map<int, int>::iterator it = packet_counter.begin();
    S3TP_PACKET * packet = popPacketInternal(it->first);
    pthread_mutex_unlock(&buffer_mutex);
    return packet;
}

S3TP_PACKET * Buffer::popPacketInternal(int port) {
    PriorityQueue<S3TP_PACKET*> * queue = queues[port];
    if (queue == NULL || queue->isEmpty()) {
        return NULL;
    }
    S3TP_PACKET * packet = queue->pop();
    if (queue->isEmpty()) {
        packet_counter.erase(port);
    } else {
        packet_counter[port] = queue->getSize();
    }
    return packet;
}