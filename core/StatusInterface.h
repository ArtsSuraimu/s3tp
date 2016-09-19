//
// Created by lorenzodonini on 06.09.16.
//

#ifndef S3TP_LINKSTATUSINTERFACE_H
#define S3TP_LINKSTATUSINTERFACE_H

class StatusInterface {
public:
    virtual void onLinkStatusChanged(bool active) = 0;
    virtual void onError(int error, void * params) = 0;
    virtual void onSynchronization() = 0;
};

#endif //S3TP_LINKSTATUSINTERFACE_H
