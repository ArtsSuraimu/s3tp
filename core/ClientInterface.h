//
// Created by Lorenzo Donini on 02/09/16.
//

#ifndef S3TP_CONNECTION_LISTENER_H
#define S3TP_CONNECTION_LISTENER_H

class ClientInterface {
public:
    virtual void onApplicationDisconnected(void *params) = 0;
    virtual void onApplicationConnected(void *params) = 0;
    virtual int onApplicationMessage(void * data, size_t len, void * params) = 0;
    virtual void onConnectToHost(void *params) = 0;
    virtual void onDisconnectFromHost(void *params) = 0;
};

#endif //S3TP_CONNECTION_LISTENER_H
