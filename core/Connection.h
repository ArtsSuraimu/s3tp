//
// Created by lorenzodonini on 12.11.16.
//

#ifndef S3TP_CONNECTION_H
#define S3TP_CONNECTION_H

#include "S3tpShared.h"
#include "PriorityQueue.h"
#include "ConnectionStatusInterface.h"
#include <queue>

#define CODE_OK 0
#define CODE_BUFFER_FULL -1
#define CODE_CONNECTION_ERROR -2
#define CODE_INVALID_PACKET -3

class Connection {
private:
    uint8_t virtualChannel;
    uint8_t options;
    uint8_t srcPort;
    uint8_t dstPort;
    uint8_t currentOutSequence;
    uint8_t expectedInSequence;
    PriorityQueue<S3TP_PACKET *> outBuffer;
    PriorityQueue<S3TP_PACKET *> inBuffer;
    std::mutex connectionMutex;
    STATE currentState;
    std::queue<uint8_t> scheduledAcknowledgements;
    static char emptyPdu[0];
    ConnectionStatusInterface * statusInterface;

    void _syn();
    void _syncAck();
    void _fin();
    void _finAck(uint8_t sequence);
    void _onFinAck(uint8_t sequence);
    void _handleAcknowledgement(uint8_t sequence);
    void updateState(STATE newState);
public:
    enum STATE {
        CONNECTING,
        CONNECTED,
        RESETTING,
        DISCONNECTING,
        DISCONNECTED
    };

    Connection(uint8_t srcPort, uint8_t dstPort, uint8_t virtualChannel, uint8_t options);
    Connection(S3TP_PACKET * synPacket);
    ~Connection();
    STATE getCurrentState();
    bool outPacketsAvailable();
    bool inPacketsAvailable();
    bool isOutBufferFull();
    bool canWriteBytesOut(int bytes);
    bool canWriteBytesIn(int bytes);
    bool isInBufferFull();
    uint8_t getSourcePort();
    uint8_t getDestinationPort();

    S3TP_PACKET * peekNextOutPacket();
    S3TP_PACKET * getNextOutPacket();
    S3TP_PACKET * peekNextInPacket();
    S3TP_PACKET * getNextInPacket();

    void scheduleAcknowledgement(uint8_t ackSequence);

    int sendOutPacket(S3TP_PACKET * pkt);
    int receiveInPacket(S3TP_PACKET * pkt);

    void reset();
    void close();
};


#endif //S3TP_CONNECTION_H
