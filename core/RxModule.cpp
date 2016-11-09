//
// Created by Lorenzo Donini on 02/09/16.
//

#include "RxModule.h"

RxModule::RxModule() {
    to_consume_global_seq = 0;
    expectedSequence = 0;
    inBuffer = new Buffer(this);
    statusInterface = nullptr;
    transportInterface = nullptr;
}

RxModule::~RxModule() {
    stopModule();
    rxMutex.lock();
    delete inBuffer;

    rxMutex.unlock();
    //TODO: implement. Also remember to close all ports
}

void RxModule::reset() {
    rxMutex.lock();
    expectedSequence = 0;
    to_consume_global_seq = 0;
    inBuffer->clear();
    current_port_sequence.clear();
    available_messages.clear();
    open_ports.clear();
    rxMutex.unlock();
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
    rxMutex.lock();
    active = true;
    rxMutex.unlock();
}

void RxModule::stopModule() {
    rxMutex.lock();
    active = false;
    //Signaling any thread that is currently waiting for an incoming message.
    // Such thread should then check the status of the module, in order to avoid waiting forever.
    availableMsgCond.notify_all();
    rxMutex.unlock();
}

bool RxModule::isActive() {
    rxMutex.lock();
    bool result = active;
    rxMutex.unlock();
    return result;
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

void RxModule::handleBufferEmpty(int channel) {
    //The channel queue is not full anymore, so we can start writing on it again
    rxMutex.lock();
    if (statusInterface != NULL) {
        statusInterface->onChannelStatusChanged((uint8_t)channel, true);
    }
    rxMutex.unlock();
}

int RxModule::openPort(uint8_t port) {
    rxMutex.lock();
    if (!active) {
        rxMutex.unlock();
        return MODULE_INACTIVE;
    }
    if (open_ports.find(port) != open_ports.end()) {
        //Port is already open
        rxMutex.unlock();
        return PORT_ALREADY_OPEN;
    }
    open_ports[port] = 1;
    rxMutex.unlock();

    return CODE_SUCCESS;
}

int RxModule::closePort(uint8_t port) {
    rxMutex.lock();
    if (!active) {
        rxMutex.unlock();
        return MODULE_INACTIVE;
    }
    if (open_ports.find(port) != open_ports.end()) {
        open_ports.erase(port);
        rxMutex.unlock();
        return CODE_SUCCESS;
    }

    rxMutex.unlock();
    return PORT_ALREADY_CLOSED;
}

bool RxModule::isPortOpen(uint8_t port) {
    rxMutex.lock();
    bool result = (open_ports.find(port) != open_ports.end() && active);
    rxMutex.unlock();
    return result;
}

bool RxModule::updateInternalSequence(uint16_t sequence, bool moreFragments) {
    if (sequence == expectedSequence) {
        if (moreFragments) {
            expectedSequence = ++sequence;
        } else {
            expectedSequence = (uint16_t )((sequence & 0xFF00) + (1 << 8));
        }
        return true;
    } else {
        return false;
    }
}

int RxModule::handleReceivedPacket(S3TP_PACKET * packet) {
    if (!isActive()) {
        return MODULE_INACTIVE;
    }

    //Checking CRC
    S3TP_HEADER * hdr = packet->getHeader();
    //TODO: check if pdu length + header size > total length (there might've been an error), otherwise buffer overflow
    if (!verify_checksum(packet->getPayload(), hdr->getPduLength(), hdr->crc)) {
        LOG_WARN(std::string("Wrong CRC for packet " + std::to_string((int)hdr->getGlobalSequence())));
        return CODE_ERROR_CRC_INVALID;
    }

    uint8_t flags = hdr->getFlags();
    if (flags & S3TP_FLAG_CTRL) {
        S3TP_CONTROL * control = (S3TP_CONTROL *)packet->getPayload();
        if (!updateInternalSequence(hdr->seq, hdr->moreFragments())) {
            // Returning immediately. Packet sequence is wrong, hence it will be dropped right away
            // Sending ACK for previous sequence to trigger retransmission
            transportInterface->onReceivedPacket(expectedSequence);
            return CODE_ERROR_INCONSISTENT_STATE;
        }
        // In case ACK flag is set as well, it will be handled by handleControlPacket
        return handleControlPacket(hdr, control);
    }
    if (flags & S3TP_FLAG_ACK) {
        LOG_DEBUG("RX: ----------- Ack received -----------");
        handleAcknowledgement(hdr->ack);
    }
    if (!(flags & S3TP_FLAG_DATA)) {
        return CODE_SUCCESS;
    }

    //No need for locking
    if (inBuffer->getSizeOfQueue(hdr->getPort()) >= MAX_QUEUE_SIZE) {
        //Dropping packet right away since queue is full anyway and force sender to retransmit
        transportInterface->onReceivedPacket(expectedSequence);
        return CODE_SERVER_QUEUE_FULL;
    }

    if (updateInternalSequence(hdr->seq, hdr->moreFragments())) {
        //We got the next expected packet. Sending ACK
        transportInterface->onReceivedPacket(expectedSequence);
    } else {
        //Dropping packet right away and sending ACK for previous message to trigger retransmission
        transportInterface->onReceivedPacket(expectedSequence);
        return CODE_ERROR_INCONSISTENT_STATE;
    }

    if (!isPortOpen(hdr->getPort())) {
        //Dropping packet, as there is no application to receive it
        LOG_INFO(std::string("Incoming packet " + std::to_string(hdr->getGlobalSequence())
                             + "for port " + std::to_string(hdr->getPort())
                             + " was dropped because port is closed"));
        return CODE_ERROR_PORT_CLOSED;
    } else {
        int result = inBuffer->write(packet);
        if (result != CODE_SUCCESS) {
            //Something bad happened, couldn't put packet in buffer
            LOG_ERROR(std::string("Could not put packet " + std::to_string((int)hdr->seq) + " in buffer"));
            return result;
        }

        LOG_DEBUG(std::string("RX: Packet received from SPI to port "
                              + std::to_string((int)hdr->getPort())
                              + " -> glob_seq " + std::to_string((int)hdr->getGlobalSequence())
                              + ", sub_seq " + std::to_string((int)hdr->getSubSequence())
                              + ", port_seq " + std::to_string((int)hdr->seq_port)));
    }

    rxMutex.lock();
    if (isCompleteMessageForPortAvailable(hdr->getPort())) {
        //New message is available, notify
        available_messages[hdr->getPort()] = 1;
        availableMsgCond.notify_all();
    }
    rxMutex.unlock();

    return CODE_SUCCESS;
}

int RxModule::handleControlPacket(S3TP_HEADER * hdr, S3TP_CONTROL * control) {
    uint16_t sequenceToAck;
    uint8_t flags = hdr->getFlags();
    //Handling control message
    switch (control->type) {
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
    }

    return CODE_SUCCESS;
}

/**
 * Handling received ack. Based on this, Tx Module will either know that a packet has successfully
 * been received, or schedule a retransmission.
 *
 * @param ackNumber  Complete sequence number of the message
 */
void RxModule::handleAcknowledgement(uint16_t ackNumber) {
    transportInterface->onAcknowledgement(ackNumber);
}

bool RxModule::isCompleteMessageForPortAvailable(int port) {
    //Method is called internally, no need for locks
    PriorityQueue<S3TP_PACKET*> * q = inBuffer->getQueue(port);
    q->lock();
    PriorityQueue_node<S3TP_PACKET*> * node = q->getHead();
    uint8_t fragment = 0;
    while (node != NULL) {
        S3TP_PACKET * pkt = node->element;
        S3TP_HEADER * hdr = pkt->getHeader();
        if (hdr->seq_port != (current_port_sequence[port] + fragment)) {
            //Packet in queue is not the one with highest priority
            break; //Will return false
        } else if (hdr->moreFragments() && hdr->getSubSequence() != fragment) {
            //Current fragment number is not the expected number,
            // i.e. at least one fragment is missing to complete the message
            break; //Will return false
        } else if (!hdr->moreFragments() && hdr->getSubSequence() == fragment) {
            //Packet is last fragment, message is complete
            q->unlock();
            return true;
        }
        fragment++;
        node = node->next;
    }
    //End of queue reached
    q->unlock();
    return false;
}

bool RxModule::isNewMessageAvailable() {
    rxMutex.lock();
    bool result = !available_messages.empty();
    rxMutex.unlock();

    return result;
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
    std::map<uint8_t, uint8_t>::iterator it = available_messages.begin();
    bool messageAssembled = false;
    std::vector<char> assembledData;
    while (!messageAssembled) {
        S3TP_PACKET * pkt = inBuffer->getNextPacket(it->first);
        S3TP_HEADER * hdr = pkt->getHeader();
        if (hdr->seq_port != current_port_sequence[it->first]) {
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
    }
    //Increase global sequence
    rxMutex.unlock();
    //Copying the entire data array, as vector memory will be released at end of function
    char * data = new char[assembledData.size()];
    memcpy(data, assembledData.data(), assembledData.size());
    return data;
}

void RxModule::flushQueues() {
    std::set<int> activeQueues = inBuffer->getActiveQueues();
    //Will flush only queues which currently hold data
    for (auto port : activeQueues) {
        if (available_messages.find(port) == available_messages.end()) {
            inBuffer->clearQueueForPort(port);
        }
    }
}

int RxModule::comparePriority(S3TP_PACKET* element1, S3TP_PACKET* element2) {
    int comp = 0;
    uint8_t seq1, seq2, offset;
    rxMutex.lock();

    offset = current_port_sequence[element1->getHeader()->getPort()];
    seq1 = element1->getHeader()->seq_port - offset;
    seq2 = element2->getHeader()->seq_port - offset;
    if (seq1 < seq2) {
        comp = -1; //Element 1 is lower, hence has higher priority
    } else if (seq1 > seq2) {
        comp = 1; //Element 2 is lower, hence has higher priority
    }
    rxMutex.unlock();
    return comp;
}

bool RxModule::isElementValid(S3TP_PACKET * element) {
    //Not needed for Rx Module. Return true by default
    //TODO: implement
    return true;
}

bool RxModule::maximumWindowExceeded(S3TP_PACKET* queueHead, S3TP_PACKET* newElement) {
    uint8_t offset, headSeq, newSeq;

    rxMutex.lock();
    offset = to_consume_global_seq;
    headSeq = queueHead->getHeader()->getGlobalSequence() - offset;
    newSeq = newElement->getHeader()->getGlobalSequence() - offset;
    rxMutex.unlock();

    return abs(newSeq - headSeq) >= MAX_TRANSMISSION_WINDOW;
}