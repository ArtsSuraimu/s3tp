//
// Created by Lorenzo Donini on 02/09/16.
//

#ifndef S3TP_RXMODULE_H
#define S3TP_RXMODULE_H

#include "Buffer.h"
#include "Constants.h"
#include "utilities.h"
#include "StatusInterface.h"
#include "TransportInterface.h"
#include <cstring>
#include <map>
#include <vector>
#include <trctrl/LinkCallback.h>

#define PORT_ALREADY_OPEN -1
#define PORT_ALREADY_CLOSED -1
#define MODULE_INACTIVE -2
#define CODE_ERROR_CRC_INVALID -3
#define CODE_NO_MESSAGES_AVAILABLE -4
#define CODE_ERROR_PORT_CLOSED -5
#define CODE_ERROR_INCONSISTENT_STATE -6

class RxModule: public Transceiver::LinkCallback,
                        PolicyActor<S3TP_PACKET*> {
public:
    RxModule();
    ~RxModule();

    void setStatusInterface(StatusInterface * statusInterface);
    void setTransportInterface(TransportInterface * transportInterface);
    void startModule();
    void stopModule();
    int openPort(uint8_t port);
    int closePort(uint8_t port);
    bool isActive();
    bool isNewMessageAvailable();
    void waitForNextAvailableMessage(pthread_mutex_t * callerMutex);
    char * getNextCompleteMessage(uint16_t * len, int * error, uint8_t * port);
    virtual int comparePriority(S3TP_PACKET* element1, S3TP_PACKET* element2);
    virtual bool isElementValid(S3TP_PACKET * element);
    virtual bool maximumWindowExceeded(S3TP_PACKET* queueHead, S3TP_PACKET* newElement);
    void reset();
private:
    bool active;
    Buffer * inBuffer;
    uint8_t to_consume_global_seq;
    uint8_t receiving_window;
    uint8_t lastReceivedGlobalSeq;
    pthread_mutex_t rx_mutex;
    pthread_cond_t available_msg_cond;

    StatusInterface * statusInterface;
    TransportInterface * transportInterface;
    std::map<uint8_t, uint8_t> open_ports;
    std::map<uint8_t, uint8_t> current_port_sequence;
    std::map<uint8_t, uint8_t> available_messages;

    // LinkCallback
    void handleFrame(bool arq, int channel, const void* data, int length);
    int handleReceivedPacket(S3TP_PACKET * packet);
    virtual void handleBufferEmpty(int channel);
    void synchronizeStatus(S3TP_SYNC& sync);
    void handleAcknowledgement(S3TP_TRANSMISSION_ACK& ack);
    void handleLinkStatus(bool linkStatus);
    bool isPortOpen(uint8_t port);
    bool isCompleteMessageForPortAvailable(int port);
    void flushQueues();

    //Utility
    uint8_t _getRelativeGlobalSequence(uint8_t target);
};


#endif //S3TP_RXMODULE_H
