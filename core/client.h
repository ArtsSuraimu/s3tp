//
// Created by Lorenzo Donini on 31/08/16.
//

#ifndef S3TP_S3TP_CLIENT_H
#define S3TP_S3TP_CLIENT_H

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "../connector/s3tp_shared.h"
#include "s3tp_main.h"
#include "connection_listener.h"

class client {
private:
    pthread_t client_thread;
    pthread_mutex_t client_mutex;
    SOCKET socket;
    bool connected;
    uint8_t app_port;
    uint8_t virtual_channel;
    uint8_t options;
    connection_listener * listener;
    s3tp_main * s3tp;

    bool isConnected();
    void closeConnection();
    void handleConnectionClosed();

    void clientRoutine();
    static void * staticClientRoutine(void * args);
public:
    client(SOCKET socket, S3TP_CONFIG config, s3tp_main * s3tp, connection_listener * listener);
    int send(const void * data, size_t len);
    void kill();
};

#endif //S3TP_S3TP_CLIENT_H
