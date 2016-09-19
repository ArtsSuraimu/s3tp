//
// Created by Lorenzo Donini on 24/08/16.
//

#ifndef S3TP_BUFFER_H
#define S3TP_BUFFER_H

#include "PriorityQueue.h"
#include <map>

class Buffer {
public:
    Buffer(PriorityComparator<S3TP_PACKET*> * comparator);
    ~Buffer();
    bool packetsAvailable();
    int write(S3TP_PACKET * packet);
    PriorityQueue<S3TP_PACKET *> * getQueue(int port);
    S3TP_PACKET * getNextPacket(int port);
    S3TP_PACKET * getNextAvailablePacket();
    void clear();

private:
    PriorityComparator<S3TP_PACKET *> * comparator;
    std::map<int, PriorityQueue<S3TP_PACKET*>*> queues;
    std::map<int, int> packet_counter;

    pthread_mutex_t buffer_mutex;

    S3TP_PACKET * popPacketInternal(int port);
};

#endif //S3TP_BUFFER_H
