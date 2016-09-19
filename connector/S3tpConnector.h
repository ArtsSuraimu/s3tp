//
// Created by Lorenzo Donini on 30/08/16.
//

#ifndef S3TP_S3TP_CONNECTOR_H
#define S3TP_S3TP_CONNECTOR_H

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>
#include "../core/S3tpShared.h"
#include "S3tpCallback.h"
#include <thread>
#include <mutex>
#include <condition_variable>

class S3tpConnector {
public:
    S3tpConnector();
    ~S3tpConnector();
    int init(S3TP_CONFIG config, S3tpCallback * callback);
    int send(const void * data, size_t len);
    char * recvRaw(size_t * len, int * error);
    int recv(void * buffer, size_t len);
    void closeConnection();
    bool isConnected();
private:
    int socketDescriptor;
    bool connected;
    bool lastMessageAck;
    std::mutex connector_mutex;
    std::thread listener_thread;
    std::condition_variable ack_cond;
    S3TP_CONFIG config;
    S3tpCallback * callback;

    void asyncListener();
    bool receiveControlMessage(S3TP_CONTROL& control);
    bool receiveDataMessage();
};

#endif //S3TP_S3TP_CONNECTOR_H
