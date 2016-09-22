//
// Created by Lorenzo Donini on 31/08/16.
//

#ifndef S3TP_S3TP_CLIENT_H
#define S3TP_S3TP_CLIENT_H

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <s3tp/core/S3tpShared.h>
#include "S3tpShared.h"
#include "ClientInterface.h"

class Client {
private:
    pthread_t client_thread;
    pthread_mutex_t client_mutex;
    SOCKET socket;
    bool connected;
    uint8_t app_port;
    uint8_t virtual_channel;
    uint8_t options;
    ClientInterface * client_if;

    bool isConnected();
    void closeConnection();
    void handleConnectionClosed();

    void clientRoutine();
    static void * staticClientRoutine(void * args);
    void acknowledgeMessage(S3TP_CONTROL ack);
public:
    Client(SOCKET socket, S3TP_CONFIG config, ClientInterface * listener);
    uint8_t getAppPort();
    uint8_t getVirtualChannel();
    uint8_t getOptions();
    int send(const void * data, size_t len);
    void kill();
};

#endif //S3TP_S3TP_CLIENT_H
