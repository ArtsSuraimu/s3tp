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
#include "S3tpShared.h"
#include "ClientInterface.h"

class Client {
private:
    pthread_t client_thread;
    pthread_mutex_t client_mutex;
    ClientInterface * client_if;

    //Binding to port and unix socket
    SOCKET socket;
    bool bound;
    uint8_t app_port;
    uint8_t virtual_channel;
    uint8_t options;
    bool isBound();
    void unbind();
    void handleUnbind();

    //Actual connection to remote socket
    bool listening;
    bool connected;
    bool connecting;
    void tryConnect();
    void disconnect();
    void listen();

    void clientRoutine();
    static void * staticClientRoutine(void * args);
    int handleControlMessage();
public:
    Client(SOCKET socket, S3TP_CONFIG config, ClientInterface * listener);
    uint8_t getAppPort();
    uint8_t getVirtualChannel();
    uint8_t getOptions();
    int send(const void * data, size_t len);
    int sendControlMessage(S3TP_CONNECTOR_CONTROL message);
    void kill();
    bool acceptConnect();
    void failedConnect();
    bool isConnected();
    bool isListening();
    void closeConnection();
};

#endif //S3TP_S3TP_CLIENT_H
