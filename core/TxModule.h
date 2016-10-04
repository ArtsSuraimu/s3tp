//
// Created by Lorenzo Donini on 23/08/16.
//

#ifndef S3TP_TXMODULE_H
#define S3TP_TXMODULE_H

#include "Constants.h"
#include "Buffer.h"
#include "utilities.h"
#include "StatusInterface.h"
#include <map>
#include <trctrl/LinkInterface.h>
#include <set>
#include <sys/time.h>

#define TX_PARAM_RECOVERY 0x01
#define TX_PARAM_CUSTOM 0x02
#define CODE_INACTIVE_ERROR -1

#define DEFAULT_RESERVED_CHANNEL 0

#define SYNC_WAIT_TIME 2

class TxModule : public PolicyActor<S3TP_PACKET *> {
public:
    enum STATE {
        RUNNING,
        BLOCKED,
        WAITING
    };

    TxModule();
    ~TxModule();

    STATE getCurrentState();
    void startRoutine(Transceiver::LinkInterface * spi_if);
    void stopRoutine();
    int enqueuePacket(S3TP_PACKET * packet, uint8_t frag_no, bool more_fragments, uint8_t spi_channel, uint8_t options);
    void reset();
    void scheduleSync(uint8_t syncId);
    void scheduleAcknowledgement(uint8_t ackSequence);
    void setStatusInterface(StatusInterface * statusInterface);
    void notifyAcknowledgement(uint8_t ackSequence);

    //Public channel and link methods
    void notifyLinkAvailability(bool available);
    bool isQueueAvailable(uint8_t port, uint8_t no_packets);
    void setChannelAvailable(uint8_t channel, bool available);
    bool isChannelAvailable(uint8_t channel);
private:
    STATE state;
    bool active;
    pthread_t tx_thread;
    pthread_mutex_t tx_mutex;
    pthread_cond_t tx_cond;
    std::set<uint8_t> channel_blacklist;
    bool sendingFragments;
    uint8_t currentPort;
    Transceiver::LinkInterface * linkInterface;
    StatusInterface * statusInterface;

    //Sync variables
    bool scheduled_sync;
    S3TP_SYNC prototypeSync = S3TP_SYNC(); //Used only for initialization. Never afterwards
    S3TP_PACKET syncPacket = S3TP_PACKET((char *)&prototypeSync, sizeof(S3TP_SYNC));
    //Ack variables
    bool scheduledAck;
    uint8_t sequenceAck;
    S3TP_TRANSMISSION_ACK transmissionAck = S3TP_TRANSMISSION_ACK();
    S3TP_PACKET ackPacket = S3TP_PACKET((char *)&transmissionAck, sizeof(S3TP_TRANSMISSION_ACK));

    //Buffer and port sequences
    std::map<uint8_t, uint8_t> to_consume_port_seq;
    std::map<uint8_t, uint8_t> port_sequence;
    uint8_t global_seq_num;
    Buffer * outBuffer;

    void txRoutine();
    static void * staticTxRoutine(void * args);
    void synchronizeStatus();
    void sendAcknowledgement();

    //Internal methods for accessing channels (do not use locking)
    bool _channelsAvailable();
    void _setChannelAvailable(uint8_t channel, bool available);
    bool _isChannelAvailable(uint8_t channel);

    //Policy Actor implementation
    virtual int comparePriority(S3TP_PACKET* element1, S3TP_PACKET* element2);
    virtual bool isElementValid(S3TP_PACKET * element);
    virtual bool maximumWindowExceeded(S3TP_PACKET* queueHead, S3TP_PACKET* newElement);
};

#endif //S3TP_TXMODULE_H
