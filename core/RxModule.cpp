//
// Created by Lorenzo Donini on 02/09/16.
//

#include "RxModule.h"

RxModule::RxModule() : statusInterface{nullptr}, transportInterface{nullptr} {}

RxModule::~RxModule() {
    stopModule();
    reset();
    if (deliveryThread.joinable()) {
        deliveryThread.join();
    }
}

void RxModule::reset() {
    std::unique_lock<std::mutex> lock{rxMutex};
    availableMessagesForPort.clear();
    //TODO: reimplement. Close all ports
}

void RxModule::setStatusInterface(StatusInterface * statusInterface) {
    rxMutex.lock();
    this->statusInterface = statusInterface;
    rxMutex.unlock();
}

void RxModule::setTransportInterface(TransportInterface * transportInterface) {
    rxMutex.lock();
    this->transportInterface = transportInterface;
    rxMutex.unlock();
}

void RxModule::startModule() {
    std::unique_lock<std::mutex> lock{rxMutex};
    active = true;
    deliveryThread = std::thread(&RxModule::deliveryRoutine, this);
}

void RxModule::stopModule() {
    std::unique_lock<std::mutex> lock{rxMutex};
    active = false;
    //Signaling the delivery thread which might be currently waiting for an incoming message.
    //Such thread should then check the status of the module, in order to avoid waiting forever.
    availableMsgCond.notify_all();
}

bool RxModule::isActive() {
    std::unique_lock<std::mutex> lock{rxMutex};
    bool result = active;
    return result;
}

/*
 * Delivery thread logic
 */
void RxModule::deliveryRoutine() {
    std::unique_lock<std::mutex> lock{rxMutex};

    while (active) {
        if (availableMessagesForPort.empty()) {
            availableMsgCond.wait(lock);
            continue;
        }
        uint8_t port = *availableMessagesForPort.begin();
        /*std::shared_ptr<Connection> connection = connectionManager->getConnection(port);
        if (connection == nullptr || !connection->inPacketsAvailable()) {
            availableMessages.erase(port);
            continue;
        }
        S3TP_PACKET * pkt = connection->getNextInPacket();*/
        //TODO: decide whether to actually deliver via thread or not
    }
}

/*
 * Callback implementation
 */
void RxModule::handleFrame(bool arq, int channel, const void* data, int length) {
    if (length > MAX_LEN_S3TP_PACKET) {
        //TODO: handle error
    }
    //Copying packet. Data argument is not needed anymore afterwards
    S3TP_PACKET * packet = new S3TP_PACKET((char *)data, length, (uint8_t)channel);
    int result = handleReceivedPacket(packet);
    //TODO: handle error
}

void RxModule::handleLinkStatus(bool linkStatus) {
    rxMutex.lock();
    LOG_DEBUG("Link status changed");
    if (statusInterface != NULL) {
        statusInterface->onLinkStatusChanged(linkStatus);
    }
    rxMutex.unlock();
}

int RxModule::handleReceivedPacket(S3TP_PACKET * packet) {
    if (!isActive()) {
        return MODULE_INACTIVE;
    }

    std::unique_lock<std::mutex> lock{rxMutex};

    //Checking CRC
    S3TP_HEADER * hdr = packet->getHeader();
    if (hdr->getPduLength() + LEN_S3TP_HDR > MAX_LEN_S3TP_PACKET
        || hdr->getPduLength() + LEN_S3TP_HDR != packet->getLength()) {
        //TODO: throw an error, due to buffer overflow
    }

    std::shared_ptr<Client> client = connectionManager->getClient(hdr->destPort);
    std::shared_ptr<Connection> connection;
    if (client == nullptr || (connection = client->getConnection()) == nullptr) {
        LOG_INFO(std::string("Incoming packet " + std::to_string(hdr->seq)
                             + "for port " + std::to_string(hdr->destPort)
                             + " was dropped because port is closed"));
        //TODO: schedule out of band error
        return CODE_ERROR_PORT_CLOSED;
    }

    uint8_t flags = hdr->getFlags();
    if (flags & FLAG_CTRL) {
        handleControlPacket(hdr, (uint8_t)packet->getPayload());
    } else {
        connection->receiveInPacket(packet);
        LOG_DEBUG(std::string("[RX] Packet received. SRC: " + std::to_string((int) hdr->srcPort)
                              + ", DST: " + std::to_string((int) hdr->destPort)
                              + ", SEQ: " + std::to_string((int) hdr->seq)
                              + ", LEN: " + std::to_string((int) hdr->pdu_length)));
    }

    return CODE_SUCCESS;
}

int RxModule::handleControlPacket(S3TP_HEADER * hdr, char * payload) {
    uint8_t controlType = (uint8_t)*payload;
    //Handling control message
    if (controlType == CONTROL_SETUP) {
        if (hdr->getFlags() & FLAG_ACK) {
            //TODO: notify of successful setup
        } else {
            connectionManager->closeAllConnections(true);
            //TODO: schedule out of band ack
        }
    }

    return CODE_SUCCESS;
}

void RxModule::waitForNextAvailableMessage(std::mutex * callerMutex) {
    std::unique_lock<std::mutex> lock(*callerMutex);
    if (availableMessagesForPort.empty()) {
        return;
    }

    availableMsgCond.wait(lock);
}

void RxModule::onNewInPacket(Connection& connection) {
    std::unique_lock<std::mutex> lock{rxMutex};

    availableMessagesForPort.insert(connection.getSourcePort());
    availableMsgCond.notify_all();
}