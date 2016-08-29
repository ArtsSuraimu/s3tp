//
// Created by Lorenzo Donini on 24/08/16.
//

#ifndef S3TP_BUFFER_H
#define S3TP_BUFFER_H

#include "PriorityQueue.h"
#include <map>

class Buffer {
public:
    pthread_cond_t new_content_cond;

    Buffer();
    ~Buffer();
    bool packetsAvailable();
    int write(S3TP_PACKET_WRAPPER * packet);
    PriorityQueue * getQueue(int port);
    S3TP_PACKET_WRAPPER * getNextPacket(int port);
    S3TP_PACKET_WRAPPER * getNextAvailablePacket();

private:
    std::map<int, PriorityQueue*> queues;
    std::map<int, int> message_map;

    pthread_mutex_t buffer_mutex;

    S3TP_PACKET_WRAPPER * popPacketInternal(int port);
};

#endif //S3TP_BUFFER_H
