//
// Created by Lorenzo Donini on 31/08/16.
//

#include "s3tp_daemon.h"


int s3tp_daemon::init() {
    if ((server = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        printf("Error creating socket\n");
        return CODE_ERROR_SOCKET_CREATE;
    }
    unlink(socket_path);
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, socket_path);

    if (bind(server, (struct sockaddr *) &address, sizeof (address)) != 0) {
        printf("Error binding socket\n");
        return CODE_ERROR_SOCKET_BIND;
    }

    s3tp.init();

    return CODE_SUCCESS;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void s3tp_daemon::startDaemon() {
    sockaddr_un cl_address;
    socklen_t addrlen;
    SOCKET new_socket;
    S3TP_CONFIG config;
    ssize_t rd, wr;
    int commCode;

    //Initializing struct for Client handshake timeout
    struct timeval tv;

    tv.tv_sec = 2;  /* 30 Secs Timeout */
    tv.tv_usec = 0;  // Initialize to 0

    listen(server, 5);
    addrlen = sizeof (struct sockaddr_in);

    //Ignore sigpipe signal in case a thread receives a forced disconnection
    signal(SIGPIPE, SIG_IGN);
    printf("Listening...\n");

    //Start s3tp_daemon service
    while (true) {
        new_socket = accept(server, (struct sockaddr *)&cl_address, &addrlen);
        if (new_socket < 0) {
            printf("Error connecting to new Client\n");
            continue;
        }

        printf("Connected to new Client %d\n", new_socket);
        tv.tv_sec = 2;
        setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
        //Receive Client configuration
        rd = read(new_socket, &config, sizeof(S3TP_CONFIG));
        if (rd <= 0) {
            printf("Error reading from new connection\n");
            close(new_socket);
            continue;
        }

        printf("Received following data from Client %d: port %d, channel %d\n", new_socket, config.port, config.channel);
        tv.tv_sec = 0;
        setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
        if (s3tp.getClientConnectedToPort(config.port) != NULL) {
            //A Client is already registered to this port
            commCode = CODE_SERVER_PORT_BUSY;
            if (write(new_socket, &commCode, sizeof(commCode)) != 0) {
                close(new_socket);
            }
            printf("Refused Client %d as port %d is currently busy\n", new_socket, config.port);
        } else {
            commCode = CODE_SERVER_ACCEPT;
            wr = write(new_socket, &commCode, sizeof(commCode));
            if (wr == 0) {
                printf("Client %d disconnected\n", new_socket);
                continue;
            } else if (wr < 0) {
                printf("Connection error with Client %d\n", new_socket);
                close(new_socket);
                continue;
            }
            //Create new Client with app_port. Client automatically starts working in background (on a different thread)
            Client * cli = new Client(new_socket, config, &s3tp);
            //S3tp module gets notified automatically of the new connection through the Client interface callback
        }
    }
}
#pragma clang diagnostic pop
