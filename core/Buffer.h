//
// Created by Lorenzo Donini on 24/08/16.
//

#ifndef S3TP_BUFFER_H
#define S3TP_BUFFER_H

#include "PriorityQueue.h"
#include <map>
#include <set>

class Buffer {
public:
    Buffer(PolicyActor<S3TP_PACKET*> * policyActor);
    ~Buffer();
    bool packetsAvailable();
    int write(S3TP_PACKET * packet);
    std::set<int> getActiveQueues();
    PriorityQueue<S3TP_PACKET *> * getQueue(int port);
    S3TP_PACKET * getNextPacket(int port);
    S3TP_PACKET * getNextAvailablePacket();
    int getSizeOfQueue(uint8_t port);
    void clear();
    void clearQueueForPort(uint8_t port);

private:
    PolicyActor<S3TP_PACKET *> * policyActor;
    std::map<int, PriorityQueue<S3TP_PACKET*>*> queues;
    std::map<int, int> packet_counter;

    pthread_mutex_t buffer_mutex;

    S3TP_PACKET * popPacketInternal(int port);
};

#endif //S3TP_BUFFER_H
