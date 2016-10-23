//
// Created by lorenzodonini on 04.10.16.
//

#ifndef S3TP_TRANSPORTINTERFACE_H
#define S3TP_TRANSPORTINTERFACE_H

class TransportInterface {
public:
    virtual void onSetup(bool ack, uint16_t sequenceNumber) = 0;
    virtual void onReceivedPacket(uint16_t sequenceNumber) = 0;
    virtual void onReceiveWindowFull(uint16_t lastValidSequence) = 0;
    virtual void onAcknowledgement(uint16_t sequenceAck) = 0;
    virtual void onConnectionRequest(uint8_t port, uint16_t sequenceNumber) = 0;
    virtual void onConnectionAccept(uint8_t port, uint16_t sequenceNumber) = 0;
    virtual void onConnectionClose(uint8_t port, uint16_t sequenceNumber) = 0;
    virtual void onReset(bool ack, uint16_t sequenceNumber) = 0;
};

#endif //S3TP_TRANSPORTINTERFACE_H
