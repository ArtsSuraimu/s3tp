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
#include "ConnectionManager.h"
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
                        ConnectionManager::InPacketListener {
public:
    RxModule();
    ~RxModule();

    void setStatusInterface(StatusInterface * statusInterface);
    void setTransportInterface(TransportInterface * transportInterface);
    void startModule();
    void stopModule();
    bool isActive();
    void waitForNextAvailableMessage(std::mutex * callerMutex);
    void reset();
private:
    bool active;
    std::shared_ptr<ConnectionManager> connectionManager;
    std::mutex rxMutex;

    StatusInterface * statusInterface;
    TransportInterface * transportInterface;

    //Delivery logic
    std::set<uint8_t> availableMessagesForPort;
    std::condition_variable availableMsgCond;
    std::thread deliveryThread;
    void deliveryRoutine();

    // LinkCallback
    void handleFrame(bool arq, int channel, const void* data, int length);
    int handleReceivedPacket(S3TP_PACKET * packet);
    int handleControlPacket(S3TP_HEADER * hdr, char * payload);
    void handleLinkStatus(bool linkStatus);

    void onNewInPacket(Connection& connection);
};


#endif //S3TP_RXMODULE_H
