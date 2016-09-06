//
// Created by Lorenzo Donini on 02/09/16.
//

#ifndef S3TP_CONNECTION_LISTENER_H
#define S3TP_CONNECTION_LISTENER_H

class ClientInterface {
public:
    virtual void onDisconnected(void * params) = 0;
    virtual void onConnected(void * params) = 0;
    virtual int onApplicationMessage(void * data, size_t len, void * params) = 0;
};

#endif //S3TP_CONNECTION_LISTENER_H
