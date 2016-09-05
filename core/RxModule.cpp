//
// Created by Lorenzo Donini on 02/09/16.
//

#include "RxModule.h"

RxModule::RxModule() {
    global_seq_num = 0;
    received_packets = 0;
    pthread_mutex_init(&rx_mutex, NULL);
    pthread_cond_init(&available_msg_cond, NULL);
    active = true;
}

RxModule::~RxModule() {
}

void RxModule::stopModule() {
    pthread_mutex_lock(&rx_mutex);
    active = false;
    pthread_mutex_unlock(&rx_mutex);
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

int RxModule::receivePacket(S3TP_PACKET * packet, uint8_t channel) {
    pthread_mutex_lock(&rx_mutex);
    if (!active) {
        pthread_mutex_unlock(&rx_mutex);
        return MODULE_INACTIVE;
    }
    pthread_mutex_unlock(&rx_mutex);

    //Checking CRC
    uint16_t check = calc_checksum((char *)packet->pdu, packet->hdr.pdu_length);
    if (check != packet->hdr.crc) {
        printf("Wrong CRC\n");
        return CODE_ERROR_CRC_INVALID;
    }

    //Copying packet
    S3TP_PACKET * pktCopy = new S3TP_PACKET();
    pktCopy->hdr = packet->hdr;
    memccpy(pktCopy->pdu, packet->pdu, packet->hdr.pdu_length, sizeof(uint8_t));

    S3TP_PACKET_WRAPPER * wrapper = new S3TP_PACKET_WRAPPER();
    wrapper->channel = channel;
    wrapper->pkt = packet;

    int result = inBuffer.write(wrapper);
    if (result != CODE_SUCCESS) {
        //Something bad happened, couldn't put packet in buffer
        return result;
    }
    pthread_mutex_lock(&rx_mutex);

    received_packets++;
    if ((received_packets % 256) == 0) {
        //Reordering window reached, flush queues?!
    }
    //TODO: check if overflowed
    pthread_mutex_unlock(&rx_mutex);
    //TODO: copy metadata and seq numbers into respective vars and check if something becomes available


}

bool RxModule::isNewMessageAvailable() {

}

S3TP_PACKET_WRAPPER * RxModule::consumeNextAvailableMessage() {

}