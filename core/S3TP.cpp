//
// Created by Lorenzo Donini on 25/08/16.
//

#include "S3TP.h"

S3TP::S3TP() {
}

S3TP::~S3TP() {
    pthread_mutex_lock(&s3tp_mutex);
    pthread_mutex_unlock(&s3tp_mutex);
    pthread_mutex_destroy(&s3tp_mutex);
}

int S3TP::init(TRANSCEIVER_CONFIG * config) {
    pthread_mutex_init(&clients_mutex, NULL);
    pthread_mutex_init(&s3tp_mutex, NULL);
    pthread_mutex_lock(&s3tp_mutex);
    active = true;

    if (config->type == SPI) {
        transceiver = Transceiver::BackendFactory::fromSPI(config->descriptor, rx);
    } else if (config->type == FIRE) {
        transceiver = Transceiver::BackendFactory::fromFireTcp(config->mappings, rx);
    }
    transceiver->start();
    rx.startModule(this);
    tx.startRoutine(rx.link);

    int id = pthread_create(&assembly_thread, NULL, &staticAssemblyRoutine, this);
    printf("Assembly Thread (id %d): START\n", id);
    pthread_mutex_unlock(&s3tp_mutex);

    return CODE_SUCCESS;
}

int S3TP::stop() {
    //Deactivating module
    pthread_mutex_lock(&s3tp_mutex);
    active = false;

    //Stop modules connected to Link Layer
    tx.stopRoutine();
    rx.stopModule();
    pthread_mutex_unlock(&s3tp_mutex);

    //Wait for assembly thread to finish
    pthread_join(assembly_thread, NULL);

    //Stop the transceiver instance
    pthread_mutex_lock(&s3tp_mutex);
    transceiver->stop();
    delete transceiver;
    pthread_mutex_unlock(&s3tp_mutex);

    return CODE_SUCCESS;
}

Client * S3TP::getClientConnectedToPort(uint8_t port) {
    pthread_mutex_lock(&clients_mutex);
    std::map<uint8_t, Client *>::iterator it = clients.find(port);
    if (it == clients.end()) {
        pthread_mutex_unlock(&clients_mutex);
        return NULL;
    }
    Client * cli = it->second;
    pthread_mutex_unlock(&clients_mutex);
    return cli;
}

int S3TP::sendToLinkLayer(uint8_t channel, uint8_t port, void * data, size_t len, uint8_t opts) {
    /* As messages should still be sent out sequentially,
     * we're putting all of them into the fragmentation queue.
     * Fragmentation thread will then be in charge of checking
     * whether the message needs fragmentation or not */
    pthread_mutex_lock(&s3tp_mutex);
    bool isActive = active;
    pthread_mutex_unlock(&s3tp_mutex);
    if (isActive) {
        if (len > MAX_PDU_LENGTH) {
            //Payload exceeds maximum length: drop packet and return error
            return CODE_ERROR_MAX_MESSAGE_SIZE;
        } else if (len > LEN_S3TP_PDU) {
            //Packet needs fragmentation
            return fragmentPayload(channel, port, data, len, opts);
        } else {
            //Payload fits into one packet
            return sendSimplePayload(channel, port, data, len, opts);
        }
    }
    return CODE_INTERNAL_ERROR;
}

int S3TP::sendSimplePayload(uint8_t channel, uint8_t port, void * data, size_t len, uint8_t opts) {
    S3TP_PACKET * packet;
    int status = 0;

    //Send to Tx Module without fragmenting
    packet = new S3TP_PACKET();
    memcpy(packet->pdu, data, len);
    packet->hdr.port = port;
    packet->hdr.pdu_length = (uint16_t)len;
    pthread_mutex_lock(&s3tp_mutex);
    status = tx.enqueuePacket(packet, 0, false, channel, opts);
    pthread_mutex_unlock(&s3tp_mutex);

    return status;
}

int S3TP::fragmentPayload(uint8_t channel, uint8_t port, void * data, size_t len, uint8_t opts) {
    S3TP_PACKET * packet;

    //Need to fragment
    int written = 0;
    int status = 0;
    int fragment = 0;
    bool moreFragments = true;
    uint8_t * dataPtr = (uint8_t *) data;
    while (written < len) {
        packet = new S3TP_PACKET();
        packet->hdr.port = port;
        if (written + LEN_S3TP_PDU > len) {
            //We are at the last packet, so don't need to write max payload
            memcpy(packet->pdu, dataPtr, len - written);
            packet->hdr.pdu_length = (uint16_t) (len - written);
        } else {
            //Filling packet payload with max permitted payload
            memcpy(packet->pdu, dataPtr, LEN_S3TP_PDU);
            packet->hdr.pdu_length = (uint16_t) LEN_S3TP_PDU;
        }
        written += packet->hdr.pdu_length;
        if (written >= len) {
            moreFragments = false;
        }
        //Send to Tx Module
        pthread_mutex_lock(&s3tp_mutex);
        //TODO: make enqueue an atomic operation
        status = tx.enqueuePacket(packet, fragment, moreFragments, channel, opts);
        pthread_mutex_unlock(&s3tp_mutex);
        if (status != CODE_SUCCESS) {
            return status;
        }
        //Increasing current fragment number for next iteration
        fragment++;
    }
    return written;
}

/*
 * Assembly Thread logic
 */
void S3TP::assemblyRoutine() {
    uint16_t len;
    int error;
    char * data;
    Client * cli;
    uint8_t  port;

    //TODO: implement routine
    pthread_mutex_lock(&s3tp_mutex);
    while(active && rx.isActive()) {
        if (!rx.isNewMessageAvailable()) {
            rx.waitForNextAvailableMessage(&s3tp_mutex);
            continue;
        }
        pthread_mutex_unlock(&s3tp_mutex);
        data = rx.getNextCompleteMessage(&len, &error, &port);
        if (error != CODE_SUCCESS) {
            LOG_DBG_S3TP("Error while trying to consume message\n");
        }
        pthread_mutex_lock(&clients_mutex);
        cli = clients[port];
        cli->send(data, len);
        pthread_mutex_unlock(&clients_mutex);

        pthread_mutex_lock(&s3tp_mutex);
    }
    //S3TP (or Rx module) was deactivated
    pthread_mutex_unlock(&s3tp_mutex);

    pthread_exit(NULL);
}

void * S3TP::staticAssemblyRoutine(void * args) {
    static_cast<S3TP*>(args)->assemblyRoutine();
    return NULL;
}

/*
 * Client Interface logic
 */
void S3TP::onDisconnected(void * params) {
    Client * cli = (Client *)params;
    //Client disconnected from port. Mark that port as available again.
    if (this->getClientConnectedToPort(cli->getAppPort()) == cli) {
        pthread_mutex_lock(&clients_mutex);
        clients.erase(cli->getAppPort());
        pthread_mutex_unlock(&clients_mutex);
        delete cli;
    }
}

void S3TP::onConnected(void * params) {
    Client * cli = (Client * )params;
    pthread_mutex_lock(&clients_mutex);
    clients[cli->getAppPort()] = cli;
    pthread_mutex_unlock(&clients_mutex);
    rx.openPort(cli->getAppPort());
}

int S3TP::onApplicationMessage(void * data, size_t len, void * params) {
    Client * cli = (Client *)params;
    return sendToLinkLayer(cli->getVirtualChannel(), cli->getAppPort(), data, len, cli->getOptions());
}

/*
 * Status callbacks
 */
void S3TP::onLinkStatusChanged(bool active) {
    tx.notifyLinkAvailability(active);
}