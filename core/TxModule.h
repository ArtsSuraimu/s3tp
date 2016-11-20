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

class TxModule : public ConnectionManager::OutPacketListener {
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
    void reset();
    void setStatusInterface(StatusInterface * statusInterface);

    //S3TP Control APIs
    void scheduleInitialSetup();
    void notifyInitialSetup();

    //Public channel and link methods
    void notifyLinkAvailability(bool available);
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
    std::queue<std::shared_ptr<S3TP_PACKET>> controlQueue; //High priority queue
    bool needsControlRetransmission;
    std::queue<std::shared_ptr<S3TP_PACKET>> controlRetransmissionQueue;
    uint8_t currentControlSequence;

    //Connection manager
    std::shared_ptr<ConnectionManager> connectionManager;
    std::queue<uint8_t> connectionRoundRobin;

    //Transmission control timer
    std::chrono::time_point<std::chrono::system_clock> start, now;
    double elapsedTime;
    int retransmissionCount;

    void txRoutine();
    int retransmitControlPackets();
    bool _sendControlPacket(std::shared_ptr<S3TP_PACKET> pkt);

    //Internal methods for accessing channels (do not use locking)
    bool _channelsAvailable();
    void _setChannelAvailable(uint8_t channel, bool available);
    bool _isChannelAvailable(uint8_t channel);

    //Connection Listener callbacks
    void onNewOutPacket(Connection& connection);
};

#endif //S3TP_TXMODULE_H
