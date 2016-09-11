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
#include "../core/s3tp_shared.h"
#include "S3tpCallback.h"
#include <pthread.h>

class s3tp_connector {
public:
    s3tp_connector();
    int init(S3TP_CONFIG config, S3tpCallback * callback);
    int send(const void * data, size_t len);
    char * recvRaw(size_t * len, int * error);
    int recv(void * buffer, size_t len);
    void closeConnection();
    bool isConnected();
private:
    int socketDescriptor;
    bool connected;
    pthread_mutex_t connector_mutex;
    pthread_t listener_thread;
    S3TP_CONFIG config;
    S3tpCallback * callback;

    void asyncListener();
    static void * staticAsyncListener(void * args);
};

#endif //S3TP_S3TP_CONNECTOR_H
