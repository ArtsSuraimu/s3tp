//
// Created by Lorenzo Donini on 23/08/16.
//

#ifndef S3TP_TXMODULE_H
#define S3TP_TXMODULE_H

#include "Constants.h"
#include "Buffer.h"
#include "utilities.h"
#include <map>
#include <trctrl/LinkInterface.h>
#include <set>

#define TX_PARAM_RECOVERY 0x01
#define TX_PARAM_CUSTOM 0x02
#define CODE_INACTIVE_ERROR -1

#define DEFAULT_SYNC_CHANNEL 0

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

    //Sync variables
    bool scheduled_sync;
    S3TP_SYNC prototypeSync = S3TP_SYNC(); //Used only for initialization. Never afterwards
    S3TP_PACKET syncPacket = S3TP_PACKET((char *)&prototypeSync, sizeof(S3TP_SYNC));

    //Buffer and port sequences
    std::map<uint8_t, uint8_t> to_consume_port_seq;
    std::map<uint8_t, uint8_t> port_sequence;
    uint8_t global_seq_num;
    Buffer * outBuffer;

    void txRoutine();
    static void * staticTxRoutine(void * args);
    void synchronizeStatus();

    //Internal methods for accessing channels (do not use locking)
    bool _channelsAvailable();
    void _setChannelAvailable(uint8_t channel, bool available);
    bool _isChannelAvailable(uint8_t channel);

    //Policy Actor implementation
    virtual int comparePriority(S3TP_PACKET* element1, S3TP_PACKET* element2);
    virtual bool isElementValid(S3TP_PACKET * element);
};

#endif //S3TP_TXMODULE_H
