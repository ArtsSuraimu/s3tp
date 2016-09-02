//
// Created by Lorenzo Donini on 31/08/16.
//

#ifndef S3TP_S3TP_DAEMON_H
#define S3TP_S3TP_DAEMON_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <map>
#include "client.h"
#include "connection_listener.h"

class s3tp_daemon: connection_listener {
private:
    sockaddr_un address;
    SOCKET server;
    s3tp_main s3tp;
    std::map<uint8_t, client*> clients;
    pthread_mutex_t clients_mutex;
public:
    int init();
    void startDaemon();

    virtual void onDisconnected(void * params);
    virtual void onConnected(void * params);
};


#endif //S3TP_S3TP_DAEMON_H
