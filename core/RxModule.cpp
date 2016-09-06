//
// Created by Lorenzo Donini on 02/09/16.
//

#include <vector>
#include "RxModule.h"

RxModule::RxModule() {
    global_seq_num = 0;
    received_packets = 0;
    pthread_mutex_init(&rx_mutex, NULL);
    pthread_cond_init(&available_msg_cond, NULL);
    active = true;
}

RxModule::~RxModule() {
    //TODO: implement. Also remember to close all ports
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

void RxModule::handleFrame(bool arq, int channel, const void* data, int length) {
    if (length != MAX_LEN_S3TP_PACKET) {
        //TODO: handle error
    }
    int result = handleReceivedPacket((S3TP_PACKET *)data, (uint8_t )channel);
    //TODO: handle error
}

void RxModule::handleLinkStatus(bool linkStatus) {
    //TODO: Link status changed, what to do?
}

int RxModule::openPort(uint8_t port) {
    pthread_mutex_lock(&rx_mutex);
    if (!active) {
        pthread_mutex_unlock(&rx_mutex);
        return MODULE_INACTIVE;
    }
    if (current_port_sequence.find(port) != current_port_sequence.end()) {
        //Port is already open
        pthread_mutex_unlock(&rx_mutex);
        return PORT_ALREADY_OPEN;
    }
    current_port_sequence[port] = 0;
    pthread_mutex_unlock(&rx_mutex);

    return PORT_OPENED;
}

int RxModule::closePort(uint8_t port) {
    pthread_mutex_lock(&rx_mutex);
    if (!active) {
        pthread_mutex_unlock(&rx_mutex);
        return MODULE_INACTIVE;
    }
    if (current_port_sequence.find(port) != current_port_sequence.end()) {
        current_port_sequence.erase(port);
        pthread_mutex_unlock(&rx_mutex);
        return PORT_CLOSED;
    }

    pthread_mutex_unlock(&rx_mutex);
    return PORT_ALREADY_CLOSED;
}

bool RxModule::isPortOpen(uint8_t port) {
    pthread_mutex_lock(&rx_mutex);
    bool result = (current_port_sequence.find(port) != current_port_sequence.end()) && active;
    pthread_mutex_unlock(&rx_mutex);
    return result;
}

int RxModule::handleReceivedPacket(S3TP_PACKET * packet, uint8_t channel) {
    if (!isActive()) {
        return MODULE_INACTIVE;
    }

    //Checking CRC
    uint16_t check = calc_checksum((char *)packet->pdu, packet->hdr.pdu_length);
    if (check != packet->hdr.crc) {
        printf("Wrong CRC\n");
        return CODE_ERROR_CRC_INVALID;
    }

    //Copying packet
    S3TP_PACKET * pktCopy = new S3TP_PACKET();
    pktCopy->hdr = packet->hdr;
    memcpy(pktCopy->pdu, packet->pdu, packet->hdr.pdu_length);

    S3TP_PACKET_WRAPPER * wrapper = new S3TP_PACKET_WRAPPER();
    wrapper->channel = channel;
    wrapper->pkt = packet;

    int result = inBuffer.write(wrapper);
    if (result != CODE_SUCCESS) {
        //Something bad happened, couldn't put packet in buffer
        return result;
    }

    pthread_mutex_lock(&rx_mutex);
    if (isCompleteMessageForPortAvailable(pktCopy->hdr.port)) {
        //New message is available, notify
        available_messages[pktCopy->hdr.port] = 1;
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

bool RxModule::isCompleteMessageForPortAvailable(int port) {
    PriorityQueue * q = inBuffer.getQueue(port);
    PriorityQueue_node * node = q->head;
    uint8_t fragment = 0;
    while (node != NULL) {
        S3TP_PACKET * pkt = node->payload->pkt;
        if (pkt->hdr.seq_port != current_port_sequence[port]) {
            //Packet in queue is not the one with highest priority
            return false;
        } else if (pkt->hdr.moreFragments() && (uint8_t)pkt->hdr.seq != fragment) {
            //Current fragment number is not the expected number,
            // i.e. at least one fragment is missing to complete the message
            return false;
        } else if (!pkt->hdr.moreFragments() && (uint8_t)pkt->hdr.seq == fragment) {
            //Packet is last fragment, message is complete
            return true;
        }
        fragment++;
        node = node->next;
    }
    //End of queue reached
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
        *len = 0;
        return NULL;
    }
    if (!isNewMessageAvailable()) {
        *error = NO_MESSAGES_AVAILABLE;
        *len = 0;
        return NULL;
    }
    //TODO: locks
    std::map<uint8_t, uint8_t>::iterator it = available_messages.begin();
    bool messageAssembled = false;
    std::vector<char> assembledData;
    while (!messageAssembled) {
        S3TP_PACKET * pkt = inBuffer.getNextPacket(it->first)->pkt;
        if (pkt->hdr.seq_port != current_port_sequence[it->first]) {
            //TODO: throw some severe error
            return NULL;
        }
        assembledData.insert(assembledData.end(), (char *)pkt->pdu, (char *)pkt->pdu + (pkt->hdr.pdu_length));
        *len += pkt->hdr.pdu_length;
        current_port_sequence[it->first]++;
        if (!pkt->hdr.moreFragments()) {
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

    return assembledData.data();
}