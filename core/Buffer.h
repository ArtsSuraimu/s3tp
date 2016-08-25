//
// Created by Lorenzo Donini on 24/08/16.
//

#ifndef S3TP_BUFFER_H
#define S3TP_BUFFER_H

#include "queue.h"
#include <map>

class Buffer {
public:
    pthread_cond_t new_content_cond;

    Buffer();
    bool packetsAvailable();
    int write(int port, S3TP_PACKET * data);
    qhead_t * getQueue(int port);
    S3TP_PACKET * getNextPacket(int port);
    S3TP_PACKET * getNextAvailablePacket();
protected:
    ~Buffer();
private:
    std::map<int, qhead_t*> queues;
    std::map<int, int> message_map;

    pthread_mutex_t buffer_mutex;

    S3TP_PACKET * popPacketInternal(int port);
};

#endif //S3TP_BUFFER_H
