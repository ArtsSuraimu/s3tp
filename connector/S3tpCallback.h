//
// Created by lorenzodonini on 10.09.16.
//

#ifndef S3TP_S3TP_MESSAGE_LISTENER_H
#define S3TP_S3TP_MESSAGE_LISTENER_H

#include <cstdlib>

class S3tpCallback {
public:
    virtual void onConnectionUp() = 0;
    virtual void onConnectionDown(int code) = 0;
    virtual void onNewMessage(char * data, size_t len) = 0;
    virtual void onError(int code, char * error) = 0;
};

#endif //S3TP_S3TP_MESSAGE_LISTENER_H
