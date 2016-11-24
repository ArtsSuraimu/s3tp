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
#include "Connection.h"

class Client : public ConnectionListener {
private:
    std::thread clientThread;
    std::mutex clientMutex;
    ClientInterface * client_if;
    std::shared_ptr<Connection> connection;

    //Binding to port and unix socket
    Socket socket;
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

    int send(const void * data, size_t len);
    void clientRoutine();
    int handleControlMessage();
public:
    Client(Socket socket, S3TP_CONFIG config, ClientInterface * listener);
    uint8_t getAppPort();
    uint8_t getVirtualChannel();
    uint8_t getOptions();
    std::shared_ptr<Connection> getConnection();
    void setConnection(std::shared_ptr<Connection> connection);
    int sendControlMessage(S3TP_CONNECTOR_CONTROL message);

    void kill();
    bool acceptConnect();
    void failedConnect();
    bool isConnected();
    bool isListening();
    void closeConnection();

    //Connection Listener callbacks
    void onConnectionStatusChanged(Connection& connection);
    void onConnectionOutOfBandRequested(S3TP_PACKET * pkt);
    void onConnectionError(Connection& connection, std::string error);
    void onNewOutPacket(Connection& connection);
    void onNewInPacket(Connection& connection);
};

#endif //S3TP_S3TP_CLIENT_H
