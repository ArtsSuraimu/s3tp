//
// Created by lorenzodonini on 04.10.16.
//

#ifndef S3TP_TRANSPORTINTERFACE_H
#define S3TP_TRANSPORTINTERFACE_H

class TransportInterface {
public:
    virtual void onSynchronization(uint8_t syncId) = 0;
    virtual void onReceivedSequence(uint16_t sequenceNumber) = 0;
    virtual void onReceiveWindowFull(uint16_t lastValidSequence) = 0;
    virtual void onAcknowledgement(uint16_t sequenceAck) = 0;
};

#endif //S3TP_TRANSPORTINTERFACE_H
