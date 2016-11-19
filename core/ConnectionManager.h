//
// Created by lorenzodonini on 13.11.16.
//

#ifndef S3TP_CONNECTIONMANGER_H
#define S3TP_CONNECTIONMANGER_H

#include "Connection.h"
#include "ConnectionListener.h"
#include <map>

class ConnectionManager {
private:
    std::map<uint8_t, std::shared_ptr<Connection>> openConnections;
    std::mutex connectionsMutex;

public:
    ~ConnectionManager() {};

    std::shared_ptr<Connection> getConnection(uint8_t localPort);
    bool handleNewConnection(S3TP_PACKET * newConnectionRequest);
    bool openConnection(uint8_t srcPort, uint8_t destPort, uint8_t virtualChannel, uint8_t options);
    bool closeConnection(uint8_t srcPort);
    int openConnectionsCount();
};


#endif //S3TP_CONNECTIONMANGER_H
