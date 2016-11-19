//
// Created by lorenzodonini on 13.11.16.
//

#ifndef S3TP_CONNECTIONSTATUSINTERFACE_H
#define S3TP_CONNECTIONSTATUSINTERFACE_H

#include <string>

class Connection;

class ConnectionListener {
public:
    void onConnectionStatusChanged(Connection& connection);
    void onConnectionOutOfBandRequested(S3TP_PACKET * pkt);
    void onConnectionError(Connection& connection, std::string error);
    void onNewOutPacket(Connection& connection);
    void onNewInPacket(Connection& connection);
};

#endif //S3TP_CONNECTIONSTATUSINTERFACE_H
