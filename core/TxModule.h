//
// Created by Lorenzo Donini on 23/08/16.
//

#ifndef S3TP_TXMODULE_H
#define S3TP_TXMODULE_H

#include "Constants.h"
#include "utilities.h"
#include "StatusInterface.h"
#include "ConnectionManager.h"
#include <map>
#include <trctrl/LinkInterface.h>
#include <set>
#include <queue>
#include <sys/time.h>
#include <chrono>

#define TX_PARAM_RECOVERY 0x01
#define TX_PARAM_CUSTOM 0x02
#define CODE_INACTIVE_ERROR -1

#define DEFAULT_RESERVED_CHANNEL 0

#define ACK_WAIT_TIME 10000 //Milliseconds
#define MAX_RETRANSMISSION_COUNT 2

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
    void startRoutine(Transceiver::LinkInterface * spi_if, std::shared_ptr<ConnectionManager> connectionManager);
    void stopRoutine();
    int enqueuePacket(S3TP_PACKET * packet, uint8_t frag_no, bool more_fragments, uint8_t spi_channel, uint8_t options);
    void reset();
    void setStatusInterface(StatusInterface * statusInterface);

    //S3TP Control APIs
    void scheduleAcknowledgement(uint16_t ackSequence);
    void scheduleReset(bool ack, uint16_t ackSequence);
    void scheduleSetup(bool ack, uint16_t ackSequence);
    void scheduleSync(uint8_t port, uint8_t channel, uint8_t options, bool ack, uint16_t ackSequence);
    void scheduleFin(uint8_t port, uint8_t channel, uint8_t options, bool ack, uint16_t ackSequence);
    void notifyAcknowledgement(uint16_t ackSequence);

    //Public channel and link methods
    void notifyLinkAvailability(bool available);
    bool isQueueAvailable(uint8_t port, uint8_t no_packets);
    void setChannelAvailable(uint8_t channel, bool available);
    bool isChannelAvailable(uint8_t channel);
private:
    STATE state;
    bool active;
    std::thread txThread;
    std::mutex txMutex;
    std::condition_variable txCond;
    std::set<uint8_t> channelBlacklist;
    Transceiver::LinkInterface * linkInterface;
    StatusInterface * statusInterface;

    //Control variables
    std::queue<S3TP_PACKET *> controlQueue; //High priority queue

    //Ack variables
    bool scheduledAck;
    uint16_t expectedSequence;

    //Connection manager
    std::shared_ptr<ConnectionManager> connectionManager;

    //Safe output buffer
    bool retransmissionRequired;

    //Transmission control timer
    std::chrono::time_point<std::chrono::system_clock> start, now;
    double elapsedTime;
    int retransmissionCount;

    void txRoutine();
    void sendAcknowledgement();
    void retransmitPackets();
    void _sendDataPacket(S3TP_PACKET *pkt);
    void _sendControlPacket(S3TP_PACKET *pkt);
    void _timedWait();

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
