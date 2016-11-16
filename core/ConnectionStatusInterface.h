//
// Created by lorenzodonini on 13.11.16.
//

#ifndef S3TP_CONNECTIONSTATUSINTERFACE_H
#define S3TP_CONNECTIONSTATUSINTERFACE_H

#include <string>

class Connection;

class ConnectionStatusInterface {
public:
    void onConnectionStatusChanged(Connection& connection);
    void onConnectionError(Connection& connection, std::string error);
};

#endif //S3TP_CONNECTIONSTATUSINTERFACE_H
