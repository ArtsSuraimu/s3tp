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
    availableMessages.clear();
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
        if (availableMessages.empty()) {
            availableMsgCond.wait(lock);
            continue;
        }
        uint8_t port = *availableMessages.begin();
        std::shared_ptr<Connection> connection = connectionManager->getConnection(port);
        if (connection == nullptr || !connection->inPacketsAvailable()) {
            availableMessages.erase(port);
            continue;
        }
        S3TP_PACKET * pkt = connection->getNextInPacket();
        //TODO: Deliver packet to client
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
    std::shared_ptr<Connection> connection = connectionManager->getConnection(hdr->destPort);
    if (connection == nullptr) {
        LOG_INFO(std::string("Incoming packet " + std::to_string(hdr->seq)
                             + "for port " + std::to_string(hdr->destPort)
                             + " was dropped because port is closed"));
        //TODO: schedule out of band error
        return CODE_ERROR_PORT_CLOSED;
    }

    uint8_t flags = hdr->getFlags();
    if (flags & FLAG_CTRL) {
        //TODO: handle control message, only if not sack
        handleControlPacket(nullptr, nullptr);
    } else {
        connection->receiveInPacket(packet);
        LOG_DEBUG(std::string("[RX] Packet received. SRC: " + std::to_string((int)hdr->srcPort)
                              + ", DST: " + std::to_string((int)hdr->destPort)
                              + ", SEQ: " + std::to_string((int)hdr->seq)
                              + ", LEN: " + std::to_string((int)hdr->pdu_length)));
    }
    //TODO: check if pdu length + header size > total length (there might've been an error), otherwise buffer overflow


    /*rxMutex.lock();
    if (isCompleteMessageForPortAvailable(hdr->destPort)) {
        //New message is available, notify
        available_messages[hdr->destPort] = 1;
        availableMsgCond.notify_all();
    }
    rxMutex.unlock();*/

    return CODE_SUCCESS;
}

int RxModule::handleControlPacket(S3TP_HEADER * hdr, S3TP_CONTROL * control) {
    uint16_t sequenceToAck;
    uint8_t flags = hdr->getFlags();
    //Handling control message
    //TODO: reimplement properly
    /*switch (control->type) {
        case CONTROL_TYPE::SETUP:
            LOG_DEBUG("RX: ----------- Setup Packet received -----------");
            // Forcefully clearing everything that was in queue up until now
            // and setting the new expected sequence number
            flushQueues();
            rxMutex.lock();
            expectedSequence = (uint16_t)(hdr->seq + 1); //Increasing directly to next expected sequence
            sequenceToAck = expectedSequence;
            rxMutex.unlock();
            transportInterface->onSetup((bool)(flags & S3TP_FLAG_ACK), sequenceToAck);
            // No need to set all port sequences to 0, as each current
            // port sequence will be received upon creating a connection
            break;
        case CONTROL_TYPE::SYNC:
            LOG_DEBUG("RX: ----------- Sync Packet received -----------");
            // Notify s3tp that a new connection is being established
            rxMutex.lock();
            current_port_sequence[hdr->getPort()] = hdr->seq_port;
            sequenceToAck = expectedSequence;
            rxMutex.unlock();
            LOG_DEBUG(std::string("Sequence on port " + std::to_string(hdr->getPort())
                                  + " synchronized. New expected value: " + std::to_string(hdr->seq_port)));
            if (flags & S3TP_FLAG_ACK) {
                //This is a connection accept message
                transportInterface->onConnectionAccept(hdr->getPort(), sequenceToAck);
            } else {
                transportInterface->onConnectionRequest(hdr->getPort(), sequenceToAck);
            }
            break;
        case CONTROL_TYPE::FIN:
            LOG_DEBUG("RX: ----------- Fin Packet received -----------");
            // Notify s3tp that a connection is being closed
            transportInterface->onConnectionClose(hdr->getPort(), hdr->seq);
            break;
        case CONTROL_TYPE::RESET:
            LOG_DEBUG("RX: ----------- Reset Packet received -----------");
            // Hard reset of the whole internal status will be handled by s3tp module
            reset();
            rxMutex.lock();
            updateInternalSequence(hdr->seq, false);
            sequenceToAck = expectedSequence;
            rxMutex.unlock();
            transportInterface->onReset((bool)(flags & S3TP_FLAG_ACK), sequenceToAck);
            break;
    }*/

    return CODE_SUCCESS;
}

bool RxModule::isCompleteMessageForPortAvailable(uint8_t port) {
    std::unique_lock<std::mutex> lock{rxMutex};
    std::shared_ptr<Connection> connection = connectionManager->getConnection(port);
    if (connection != nullptr) {
        return connection->inPacketsAvailable();
    }

    return false;
}

bool RxModule::isNewMessageAvailable() {
    rxMutex.lock();
    //bool result = !available_messages.empty();
    rxMutex.unlock();

    return true;
}

void RxModule::waitForNextAvailableMessage(std::mutex * callerMutex) {
    if (isNewMessageAvailable()) {
        return;
    }
    std::unique_lock<std::mutex> lock(*callerMutex);

    availableMsgCond.wait(lock);
}

char * RxModule::getNextCompleteMessage(uint16_t * len, int * error, uint8_t * port) {
    *len = 0;
    *port = 0;
    *error = CODE_SUCCESS;
    if (!isActive()) {
        *error = MODULE_INACTIVE;
        LOG_WARN("RX: Module currently inactive, cannot consume messages");
        *len = 0;
        return NULL;
    }
    if (!isNewMessageAvailable()) {
        *error = CODE_NO_MESSAGES_AVAILABLE;
        LOG_WARN("RX: Trying to consume message, although no new messages are available");
        *len = 0;
        return NULL;
    }

    rxMutex.lock();
    /*std::map<uint8_t, uint8_t>::iterator it = available_messages.begin();
    bool messageAssembled = false;
    std::vector<char> assembledData;
    while (!messageAssembled) {
        S3TP_PACKET * pkt = inBuffer->getNextPacket(it->first);
        S3TP_HEADER * hdr = pkt->getHeader();
        if (hdr->seq != current_port_sequence[it->first]) {
            *error = CODE_ERROR_INCONSISTENT_STATE;
            LOG_ERROR("RX: inconsistency between packet sequence port and expected sequence port");
            return NULL;
        }
        char * end = pkt->getPayload() + (sizeof(char) * hdr->getPduLength());
        assembledData.insert(assembledData.end(), pkt->getPayload(), end);
        *len += hdr->getPduLength();
        current_port_sequence[it->first]++;
        if (!hdr->moreFragments()) {
            *port = it->first;
            messageAssembled = true;
            // Not updating the global sequence right away,
            // as this will be done after the recv window has been filled
        }
    }
    //Message was assembled correctly, checking if there are further available messages
    if (isCompleteMessageForPortAvailable(it->first)) {
        //New message is available, notify
        available_messages[it->first] = 1;
        availableMsgCond.notify_all();
    } else {
        available_messages.erase(it->first);
    }*/
    //Increase global sequence
    rxMutex.unlock();
    //Copying the entire data array, as vector memory will be released at end of function
    /*char * data = new char[assembledData.size()];
    memcpy(data, assembledData.data(), assembledData.size());
    return data;*/
    return nullptr;
}

void RxModule::onNewInPacket(Connection& connection) {

}