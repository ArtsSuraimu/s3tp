//
// Created by lorenzodonini on 13.11.16.
//

#ifndef S3TP_CONNECTIONMANGER_H
#define S3TP_CONNECTIONMANGER_H

#include "Client.h"
#include <map>

class ConnectionManager {
private:
    std::map<uint8_t, std::shared_ptr<Connection>> openConnections;
    std::map<uint8_t, std::shared_ptr<Client>> clients;
    std::mutex connectionsMutex;
    InPacketListener * inListener;

public:
    ~ConnectionManager() {
        //Not deleting listeners upon destruction
    };

    std::shared_ptr<Client> getClient(uint8_t localPort);
    bool handleNewConnection(S3TP_PACKET * newConnectionRequest);
    bool openConnection(uint8_t srcPort, uint8_t destPort, uint8_t virtualChannel, uint8_t options);
    bool closeConnection(uint8_t srcPort);
    void closeAllConnections(bool forced);
    int openConnectionsCount();
    void setInPacketListener(InPacketListener * listener);

    class InPacketListener {
    public:
        virtual void onNewInPacket(Connection& connection) = 0;
    };
};


#endif //S3TP_CONNECTIONMANGER_H
