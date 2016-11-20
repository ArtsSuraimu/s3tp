//
// Created by lorenzodonini on 13.11.16.
//

#ifndef S3TP_CONNECTIONMANGER_H
#define S3TP_CONNECTIONMANGER_H

#include "Connection.h"
#include "ConnectionListener.h"
#include <map>

class ConnectionManager: public ConnectionListener {
private:
    std::map<uint8_t, std::shared_ptr<Connection>> openConnections;
    std::mutex connectionsMutex;
    InPacketListener * inListener;
    OutPacketListener * outListener;

public:
    ~ConnectionManager() {
        //Not deleting listeners upon destruction
    };

    std::shared_ptr<Connection> getConnection(uint8_t localPort);
    bool handleNewConnection(S3TP_PACKET * newConnectionRequest);
    bool openConnection(uint8_t srcPort, uint8_t destPort, uint8_t virtualChannel, uint8_t options);
    bool closeConnection(uint8_t srcPort);
    int openConnectionsCount();
    void setInPacketListener(InPacketListener * listener);
    void setOutPacketListener(OutPacketListener * listener);

    //Connection Listener callbacks
    void onConnectionStatusChanged(Connection& connection);
    void onConnectionOutOfBandRequested(S3TP_PACKET * pkt);
    void onConnectionError(Connection& connection, std::string error);
    void onNewOutPacket(Connection& connection);
    void onNewInPacket(Connection& connection);

    class OutPacketListener {
    public:
        virtual void onNewOutPacket(Connection& connection) = 0;
    };

    class InPacketListener {
    public:
        virtual void onNewInPacket(Connection& connection) = 0;
    };
};


#endif //S3TP_CONNECTIONMANGER_H
