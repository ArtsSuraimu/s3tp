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
    std::mutex connector_mutex;
    std::thread listener_thread;
    S3TP_CONFIG config;
    S3tpCallback * callback;

    void asyncListener();
    bool acknowledgeMessage();
};

#endif //S3TP_S3TP_CONNECTOR_H
