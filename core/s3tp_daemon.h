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
#include <csignal>
#include "s3tp_main.h"

class s3tp_daemon {
private:
    sockaddr_un address;
    SOCKET server;
    s3tp_main s3tp;

public:
    int init(void * args);
    void startDaemon();
};


#endif //S3TP_S3TP_DAEMON_H
