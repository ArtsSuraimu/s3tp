//
// Created by Lorenzo Donini on 02/09/16.
//

#include "RxModule.h"

RxModule::RxModule() {
    to_consume_global_seq = 0;
    received_packets = 0;
    pthread_mutex_init(&rx_mutex, NULL);
    pthread_cond_init(&available_msg_cond, NULL);
    inBuffer = new Buffer(this);
    statusInterface = NULL;
}

RxModule::~RxModule() {
    stopModule();
    pthread_mutex_lock(&rx_mutex);
    delete inBuffer;
    pthread_cond_destroy(&available_msg_cond);

    pthread_mutex_unlock(&rx_mutex);
    pthread_mutex_destroy(&rx_mutex);
    //TODO: implement. Also remember to close all ports
}

void RxModule::reset() {
    pthread_mutex_lock(&rx_mutex);
    received_packets = 0;
    to_consume_global_seq = 0;
    inBuffer->clear();
    current_port_sequence.clear();
    available_messages.clear();
    open_ports.clear();
    pthread_mutex_unlock(&rx_mutex);
}

void RxModule::setStatusInterface(StatusInterface * statusInterface) {
    pthread_mutex_lock(&rx_mutex);
    this->statusInterface = statusInterface;
    pthread_mutex_unlock(&rx_mutex);
}

void RxModule::startModule() {
    pthread_mutex_lock(&rx_mutex);
    received_packets = 0;
    active = true;
    pthread_mutex_unlock(&rx_mutex);
}

void RxModule::stopModule() {
    pthread_mutex_lock(&rx_mutex);
    active = false;
    //Signaling any thread that is currently waiting for an incoming message.
    // Such thread should then check the status of the module, in order to avoid waiting forever.
    pthread_cond_signal(&available_msg_cond);
    pthread_mutex_unlock(&rx_mutex);
}

bool RxModule::isActive() {
    pthread_mutex_lock(&rx_mutex);
    bool result = active;
    pthread_mutex_unlock(&rx_mutex);
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
    pthread_mutex_lock(&rx_mutex);
    LOG_DEBUG("Link status changed");
    if (statusInterface != NULL) {
        statusInterface->onLinkStatusChanged(linkStatus);
    }
    pthread_mutex_unlock(&rx_mutex);
}

void RxModule::handleBufferEmpty(int channel) {
    //The channel queue is not full anymore, so we can start writing on it again
    LOG_DEBUG(std::string("Channel is now empty again: " + std::to_string(channel)));
    //TODO: implement
}

int RxModule::openPort(uint8_t port) {
    pthread_mutex_lock(&rx_mutex);
    if (!active) {
        pthread_mutex_unlock(&rx_mutex);
        return MODULE_INACTIVE;
    }
    if (open_ports.find(port) != open_ports.end()) {
        //Port is already open
        pthread_mutex_unlock(&rx_mutex);
        return PORT_ALREADY_OPEN;
    }
    open_ports[port] = 1;
    pthread_mutex_unlock(&rx_mutex);

    return CODE_SUCCESS;
}

int RxModule::closePort(uint8_t port) {
    pthread_mutex_lock(&rx_mutex);
    if (!active) {
        pthread_mutex_unlock(&rx_mutex);
        return MODULE_INACTIVE;
    }
    if (open_ports.find(port) != open_ports.end()) {
        open_ports.erase(port);
        pthread_mutex_unlock(&rx_mutex);
        return CODE_SUCCESS;
    }

    pthread_mutex_unlock(&rx_mutex);
    return PORT_ALREADY_CLOSED;
}

bool RxModule::isPortOpen(uint8_t port) {
    pthread_mutex_lock(&rx_mutex);
    bool result = (open_ports.find(port) != open_ports.end() && active);
    pthread_mutex_unlock(&rx_mutex);
    return result;
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

    S3TP_MESSAGE_TYPE type = hdr->getMessageType();
    if (type == S3TP_MSG_SYNC) {
        S3TP_SYNC * sync = (S3TP_SYNC*)packet->getPayload();
        synchronizeStatus(*sync);
        return CODE_SUCCESS;
    } else if (type != S3TP_MSG_DATA) {
        //Not recognized data message
        LOG_WARN(std::string("Unrecognized message type received: " + std::to_string((int)type)));
        return CODE_ERROR_INVALID_TYPE;
    }

    if (!isPortOpen(hdr->getPort())) {
        //Dropping packet right away
        LOG_INFO(std::string("Incoming packet " + std::to_string(hdr->getGlobalSequence())
                             + "for port " + std::to_string(hdr->getPort())
                             + " was dropped because port is closed"));
        return CODE_ERROR_PORT_CLOSED;
    }

    int result = inBuffer->write(packet);
    if (result != CODE_SUCCESS) {
        //Something bad happened, couldn't put packet in buffer
        return result;
    }

    LOG_DEBUG(std::string("RX: Packet received from SPI to port "
                          + std::to_string((int)hdr->getPort())
                          + " -> glob_seq " + std::to_string((int)hdr->getGlobalSequence())
                          + ", sub_seq " + std::to_string((int)hdr->getSubSequence())
                          + ", port_seq" + std::to_string((int)hdr->seq_port)));

    pthread_mutex_lock(&rx_mutex);
    if (isCompleteMessageForPortAvailable(hdr->getPort())) {
        //New message is available, notify
        available_messages[hdr->getPort()] = 1;
        pthread_cond_signal(&available_msg_cond);
    }
    pthread_mutex_unlock(&rx_mutex);

    //This variable doesn't need locking, as it is a purely internal counter
    received_packets++;
    if ((received_packets % 256) == 0) {
        //Reordering window reached, flush queues?!
        //TODO: check if overflowed
    }
    //TODO: copy metadata and seq numbers into respective vars and check if something becomes available

    return CODE_SUCCESS;
}

void RxModule::synchronizeStatus(S3TP_SYNC& sync) {
    to_consume_global_seq = sync.tx_global_seq;
    for (int i=0; i<DEFAULT_MAX_OUT_PORTS; i++) {
        if (sync.port_seq[i] != 0) {
            current_port_sequence[i] = sync.port_seq[i];
        }
    }
    //Notify main module
    statusInterface->onSynchronization();
}

bool RxModule::isCompleteMessageForPortAvailable(int port) {
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
    pthread_mutex_lock(&rx_mutex);
    bool result = !available_messages.empty();
    pthread_mutex_unlock(&rx_mutex);

    return result;
}

void RxModule::waitForNextAvailableMessage(pthread_mutex_t * callerMutex) {
    if (isNewMessageAvailable()) {
        return;
    }
    pthread_cond_wait(&available_msg_cond, callerMutex);
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
    //TODO: locks
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
        }
    }
    //Message was assembled correctly, checking if there are further available messages
    if (isCompleteMessageForPortAvailable(it->first)) {
        //New message is available, notify
        available_messages[it->first] = 1;
        pthread_cond_signal(&available_msg_cond);
    } else {
        available_messages.erase(it->first);
    }
    //Copying the entire data array, as vector memory will be released at end of function
    char * data = new char[assembledData.size()];
    memcpy(data, assembledData.data(), assembledData.size());
    return data;
}

int RxModule::comparePriority(S3TP_PACKET* element1, S3TP_PACKET* element2) {
    int comp = 0;
    uint8_t seq1, seq2, offset;
    pthread_mutex_lock(&rx_mutex);
    offset = to_consume_global_seq;
    //First check global seq number for comparison
    seq1 = element1->getHeader()->getGlobalSequence() - offset;
    seq2 = element2->getHeader()->getGlobalSequence() - offset;
    if (seq1 < seq2) {
        comp = -1; //Element 1 is lower, hence has higher priority
    } else if (seq1 > seq2) {
        comp = 1; //Element 2 is lower, hence has higher priority
    }
    if (comp != 0) {
        pthread_mutex_unlock(&rx_mutex);
        return comp;
    }
    offset = current_port_sequence[element1->getHeader()->getPort()];
    seq1 = element1->getHeader()->seq_port - offset;
    seq2 = element2->getHeader()->seq_port - offset;
    if (seq1 < seq2) {
        comp = -1; //Element 1 is lower, hence has higher priority
    } else if (seq1 > seq2) {
        comp = 1; //Element 2 is lower, hence has higher priority
    }
    pthread_mutex_unlock(&rx_mutex);
    return comp;
}

bool RxModule::isElementValid(S3TP_PACKET * element) {
    //Not needed for Rx Module. Return true by default
    return true;
}