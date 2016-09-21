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

#define DEFAULT_SYNC_CHANNEL 3

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
    void notifyLinkAvailability(bool available);
    bool isQueueAvailable(uint8_t port, uint8_t no_packets);
    void setChannelAvailable(uint8_t channel, bool available);
    bool isChannelAvailable(uint8_t channel);
    void reset();
    void scheduleSync();
private:
    STATE state;
    bool active;
    pthread_t tx_thread;
    pthread_mutex_t tx_mutex;
    pthread_mutex_t channel_mutex;
    pthread_cond_t tx_cond;
    std::set<uint8_t> channel_blacklist;
    bool sendingFragments;
    uint8_t currentPort;
    uint8_t global_seq_num;
    Transceiver::LinkInterface * linkInterface;
    bool scheduled_sync;
    S3TP_SYNC syncStructure;

    std::map<uint8_t, uint8_t> to_consume_port_seq;
    std::map<uint8_t, uint8_t> port_sequence;
    Buffer * outBuffer;

    void txRoutine();
    static void * staticTxRoutine(void * args);
    bool channelsAvailable();
    void synchronizeStatus();
    virtual int comparePriority(S3TP_PACKET* element1, S3TP_PACKET* element2);
    virtual bool isElementValid(S3TP_PACKET * element);
};

#endif //S3TP_TXMODULE_H
