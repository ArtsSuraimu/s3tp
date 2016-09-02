//
// Created by Lorenzo Donini on 02/09/16.
//

#ifndef S3TP_CONNECTION_LISTENER_H
#define S3TP_CONNECTION_LISTENER_H

class connection_listener {
public:
    virtual void onDisconnected(void * params) = 0;
    virtual void onConnected(void * params) = 0;
};

#endif //S3TP_CONNECTION_LISTENER_H
