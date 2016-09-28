//
// Created by Lorenzo Donini on 24/08/16.
//

#include "Buffer.h"

//Ctor
Buffer::Buffer(PolicyActor<S3TP_PACKET*> * policyActor) {
    pthread_mutex_init(&buffer_mutex, NULL);
    this->policyActor = policyActor;
}

//Dtor
Buffer::~Buffer() {
    clear();
    pthread_mutex_destroy(&buffer_mutex);
}

void Buffer::clear() {
    pthread_mutex_lock(&buffer_mutex);
    for (std::map<int, PriorityQueue<S3TP_PACKET*> *>::iterator it = queues.begin(); it != queues.end(); ++it) {
        delete it->second;
    }
    queues.clear();
    packet_counter.clear();
    pthread_mutex_unlock(&buffer_mutex);
}

void Buffer::clearQueueForPort(uint8_t port) {
    pthread_mutex_lock(&buffer_mutex);
    std::map<int, PriorityQueue<S3TP_PACKET*>*>::iterator el = queues.find(port);
    if (el != queues.end()) {
        el->second->clear();
    }
    packet_counter.erase(port);
    pthread_mutex_unlock(&buffer_mutex);
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

    if (!queue->isEmpty() && policyActor->maximumWindowExceeded(queue->peek(), packet)) {
        //Clearing queue, since maximum window was exceeded
        queue->clear();
        packet_counter.erase(port);
    }
    if (queue->push(packet, policyActor) == QUEUE_FULL) {
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

std::set<int> Buffer::getActiveQueues() {
    std::set<int> result;

    pthread_mutex_lock(&buffer_mutex);
    for (auto const &it : packet_counter) {
        if (it.second > 0) {
            result.insert(it.first);
        }
    }
    pthread_mutex_unlock(&buffer_mutex);

    return result;
}

PriorityQueue<S3TP_PACKET *> * Buffer::getQueue(int port) {
    pthread_mutex_lock(&buffer_mutex);
    PriorityQueue<S3TP_PACKET*> * queue = queues[port];
    if (queue == nullptr) {
        queue = new PriorityQueue<S3TP_PACKET *>();
        queues[port] = queue;
    }
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

    S3TP_PACKET * packet = NULL;
    for (auto const &it: packet_counter) {
        packet = popPacketInternal(it.first);
        if (packet != NULL) {
            break;
        }
    }
    pthread_mutex_unlock(&buffer_mutex);
    return packet;
}

int Buffer::getSizeOfQueue(uint8_t port) {
    pthread_mutex_lock(&buffer_mutex);
    PriorityQueue<S3TP_PACKET*> * queue = queues[port];
    if (queue == NULL) {
        pthread_mutex_unlock(&buffer_mutex);
        return 0;
    }
    int res = queue->getSize();
    pthread_mutex_unlock(&buffer_mutex);

    return res;
}

S3TP_PACKET * Buffer::popPacketInternal(int port) {
    PriorityQueue<S3TP_PACKET*> * queue = queues[port];
    if (queue == NULL || queue->isEmpty()) {
        return NULL;
    }
    S3TP_PACKET * packet = queue->peek();
    if (!policyActor->isElementValid(packet)) {
        return NULL;
    }
    packet = queue->pop();
    if (queue->isEmpty()) {
        packet_counter.erase(port);
    } else {
        packet_counter[port] = queue->getSize();
    }
    return packet;
}