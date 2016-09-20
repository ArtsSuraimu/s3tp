//
// Created by Lorenzo Donini on 31/08/16.
//

#include "TransportDaemon.h"

int s3tp_daemon::init(void * args) {
    if ((server = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        LOG_ERROR("Error creating daemon socket");
        return CODE_ERROR_SOCKET_CREATE;
    }
    unlink(socket_path);
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, socket_path);

    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    if (bind(server, (struct sockaddr *) &address, sizeof (address)) != 0) {
        LOG_ERROR("Error binding daemon socket");
        return CODE_ERROR_SOCKET_BIND;
    }

    TRANSCEIVER_CONFIG * config = (TRANSCEIVER_CONFIG *)args;

    s3tp.init(config);

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
    LOG_INFO("Daemon started listening...");

    //Start s3tp_daemon service
    while (true) {
        new_socket = accept(server, (struct sockaddr *)&cl_address, &addrlen);
        if (new_socket < 0) {
            LOG_ERROR("Error connecting to new client");
            continue;
        }

        LOG_INFO(std::string("Connected to new client on socket " + std::to_string(new_socket)));

        //Remove disconnected clients from structure
        s3tp.cleanupClients();

        tv.tv_sec = 2;
        setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
        //Receive Client configuration
        rd = read(new_socket, &config, sizeof(S3TP_CONFIG));
        if (rd <= 0) {
            LOG_WARN("Error reading from new connection to client. Closing socket");
            close(new_socket);
            continue;
        }

        LOG_DEBUG(std::string("Received configuration from new client on socket "
                              + std::to_string(new_socket)
                              + ": port " + std::to_string((int)config.port)
                              + ", channel: " + std::to_string((int)config.channel)));

        tv.tv_sec = 0;
        setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
        if (s3tp.getClientConnectedToPort(config.port) != NULL) {
            //A Client is already registered to this port
            commCode = CODE_SERVER_PORT_BUSY;
            if (write(new_socket, &commCode, sizeof(commCode)) != 0) {
                close(new_socket);
            }
            LOG_INFO(std::string("Refused client " + std::to_string(new_socket)
                                 + " as port " + std::to_string((int)config.port) + " is currently busy"));

        } else {
            commCode = CODE_SERVER_ACCEPT;
            wr = write(new_socket, &commCode, sizeof(commCode));
            if (wr == 0) {
                LOG_WARN(std::string("Client " + std::to_string(new_socket) + " disconnected unexpectedly"));
                continue;
            } else if (wr < 0) {
                LOG_WARN(std::string("Communication error with client " + std::to_string(new_socket)
                                     + ". Closing socket"));
                close(new_socket);
                continue;
            }
            /*
             * Create new Client with app_port.
             * Client automatically starts working in background (on a different thread)
             * S3tp module gets notified automatically of the new connection through the Client interface callback
             */
            new Client(new_socket, config, &s3tp);
        }
    }
}
#pragma clang diagnostic pop
