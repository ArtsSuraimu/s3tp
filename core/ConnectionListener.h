//
// Created by lorenzodonini on 13.11.16.
//

#ifndef S3TP_CONNECTIONSTATUSINTERFACE_H
#define S3TP_CONNECTIONSTATUSINTERFACE_H

#include <string>

class Connection;

class ConnectionListener {
public:
    virtual void onConnectionStatusChanged(Connection& connection) = 0;
    virtual void onConnectionOutOfBandRequested(S3TP_PACKET * pkt) = 0;
    virtual void onConnectionError(Connection& connection, std::string error) = 0;
    virtual void onNewOutPacket(Connection& connection) = 0;
    virtual void onNewInPacket(Connection& connection) = 0;
};

#endif //S3TP_CONNECTIONSTATUSINTERFACE_H
