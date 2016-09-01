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

class s3tp_daemon {
private:
    sockaddr_un address;
    SOCKET server;
    std::map<uint8_t, client*> clients;
    std::map<uint8_t, pthread_cond_t> client_signals;
public:
    int init();
    void startDaemon();
};


#endif //S3TP_S3TP_DAEMON_H
