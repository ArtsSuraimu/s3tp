//
// Created by lorenzodonini on 06.09.16.
//

#ifndef S3TP_LINKSTATUSINTERFACE_H
#define S3TP_LINKSTATUSINTERFACE_H

class S3tpStatusInterface {
public:
    virtual void onLinkStatusChanged(bool active) = 0;
};

#endif //S3TP_LINKSTATUSINTERFACE_H
