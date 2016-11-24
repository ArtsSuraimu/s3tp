//
// Created by Lorenzo Donini on 25/08/16.
//

#include "S3TP.h"

S3TP::S3TP() {
    reset();
    resetting = false;
}

S3TP::~S3TP() {
    s3tpMutex.lock();
    bool toStop = active;
    s3tpMutex.unlock();
    if (toStop) {
        stop();
    }
}

void S3TP::reset() {
    rx.reset();
    tx.reset();
    setupPerformed = false;
    setupInitiated = false;
    resetting = true;
}

int S3TP::init(TRANSCEIVER_CONFIG * config) {
    s3tpMutex.lock();

    active = true;

    if (config->type == SPI) {
        transceiver = Transceiver::BackendFactory::fromSPI(config->descriptor, rx);
    } else if (config->type == FIRE) {
        transceiver = Transceiver::BackendFactory::fromFireTcp(config->mappings, rx);
    }

    rx.setStatusInterface(this);
    rx.setTransportInterface(this);
    s3tpMutex.unlock();

    transceiver->start();

    s3tpMutex.lock();
    rx.startModule();
    tx.startRoutine(rx.link, connectionManager);

    LOG_DEBUG(std::string("Assembly Thread: START"));

    s3tpMutex.unlock();

    return CODE_SUCCESS;
}

int S3TP::stop() {
    //Deactivating module
    s3tpMutex.lock();
    active = false;

    //Stop modules connected to Link Layer
    tx.stopRoutine();
    rx.stopModule();
    s3tpMutex.unlock();

    //Stop the transceiver instance
    s3tpMutex.lock();
    transceiver->stop();
    delete transceiver;
    s3tpMutex.unlock();

    return CODE_SUCCESS;
}

std::shared_ptr<Client> S3TP::getClientConnectedToPort(uint8_t port) {
    std::unique_lock<std::mutex> lock{clientsMutex};
    return connectionManager->getClient(port);
}

void S3TP::cleanupClients() {
    std::unique_lock<std::mutex> lock{clientsMutex};
    for (auto const& port: disconnectedClients) {
        std::shared_ptr<Client> cli = connectionManager->getClient(port);
        //Client disconnected from port. Mark that port as available again.
        if (cli != nullptr) {
            //TODO: remove client from connection manager somehow
            cli->kill();
        }
    }
    disconnectedClients.clear();
}

/* As messages should still be sent out sequentially.
     * There is not need for a separate fragmentation thread, as the
     * job will simply be done by the calling client thread.
     * We first check whether the message can actually be enqueued.
     * If queue is full or link is not active, the message is not accepted and an error is returned.
     * */
/*int S3TP::sendToLinkLayer(uint8_t channel, uint8_t port, void * data, size_t len, uint8_t opts) {
    int availability;

    s3tpMutex.lock();
    bool isActive = active;
    s3tpMutex.unlock();
    if (isActive) {
        availability = checkTransmissionAvailability(port, channel, (uint16_t)len);
        if (availability != CODE_SUCCESS) {
            return availability;
        }
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

    //Send to Tx Module without fragmenting
    packet = new S3TP_PACKET((char *)data, (uint16_t) len);
    packet->channel = channel;
    packet->options = opts;
    packet->getHeader()->srcPort = port;

    return 0;
    //return tx.enqueuePacket(packet, 0, false, channel, opts);
}

int S3TP::fragmentPayload(uint8_t channel, uint8_t port, void * data, size_t len, uint8_t opts) {
    S3TP_PACKET * packet;

    //Need to fragment
    int written = 0;
    int status = CODE_SUCCESS;
    uint8_t fragment = 0;
    bool moreFragments = true;
    char * dataPtr = (char *) data;

    while (written < len) {
        if (written + LEN_S3TP_PDU > len) {
            //We are at the last packet, so don't need to write max payload
            packet = new S3TP_PACKET(dataPtr, (uint16_t)(len - written));
        } else {
            //Filling packet payload with max permitted payload
            packet = new S3TP_PACKET(dataPtr, LEN_S3TP_PDU);
        }
        packet->getHeader()->srcPort = port;
        packet->options = opts;
        packet->channel = channel;
        written += packet->getHeader()->getPduLength();
        dataPtr += written;

        if (written >= len) {
            moreFragments = false;
        }
        //Send to Tx Module
        // = tx.enqueuePacket(packet, fragment, moreFragments, channel, opts);
        status = 0;
        if (status != CODE_SUCCESS) {
            return status;
        }
        //Increasing current fragment number for next iteration
        fragment++;
    }

    return status;
}*/

/*
 * Client Interface logic
 */
void S3TP::onApplicationDisconnected(void *params) {
    Client * cli = (Client *)params;
    clientsMutex.lock();
    disconnectedClients.push_back(cli->getAppPort());
    clientsMutex.unlock();
}

void S3TP::onApplicationConnected(void *params) {
    Client * cli = (Client * )params;
    clientsMutex.lock();
    //clients[cli->getAppPort()] = cli;
    clientsMutex.unlock();
    //Start setup if first time starting the protocol
    s3tpMutex.lock();
    if (!setupPerformed) {
        tx.scheduleInitialSetup();
    }
    s3tpMutex.unlock();
}

int S3TP::onApplicationMessage(void * data, size_t len, void * params) {
    Client * cli = (Client *)params;
    //return sendToLinkLayer(cli->getVirtualChannel(), cli->getAppPort(), data, len, cli->getOptions());
    return 0;
}

void S3TP::onConnectToHost(void *params) {
    Client * cli = (Client *)params;
    //Scheduling 3-way handshake
    //tx.scheduleSync(cli->getAppPort(), cli->getVirtualChannel(), cli->getOptions(), false, 0);
}

void S3TP::onDisconnectFromHost(void *params) {
    Client * cli = (Client *)params;
    //Scheduling connection teardown
    //tx.scheduleFin(cli->getAppPort(), cli->getVirtualChannel(), cli->getOptions(), false, 0);
}

/*
 * Status callbacks
 */
void S3TP::onLinkStatusChanged(bool active) {
    tx.notifyLinkAvailability(active);
    if (active) {
        //TODO: implement sync in case needed
        connectionManager->notifyAvailabilityToClients();
    }
}

void S3TP::onChannelStatusChanged(uint8_t channel, bool active) {
    tx.setChannelAvailable(channel, active);

    S3TP_CONNECTOR_CONTROL control;
    control.controlMessageType = AVAILABLE;
    control.error = 0;
    //Notify previously blocked clients
    clientsMutex.lock();
    //TODO: move to connection manager
    /*for (auto const &it : clients) {
        if (it.second->getVirtualChannel() == channel) {
            it.second->sendControlMessage(control);
        }
    }*/
    clientsMutex.unlock();
}

void S3TP::onError(int error, void * params) {
    //TODO: implement
}

void S3TP::onOutputQueueAvailable(uint8_t port) {
    clientsMutex.lock();

    /*std::map<uint8_t, Client*>::iterator it = clients.find(port);
    if (it != clients.end()) {
        S3TP_CONNECTOR_CONTROL control;
        control.controlMessageType = AVAILABLE;
        control.error = 0;

        it->second->sendControlMessage(control);
    }*/

    clientsMutex.unlock();
}

/*
 * Transport callbacks
 */
/**
 * Received upon first initialization of remote module. Used only once.
 * The same message is used to acknowledge a remote setup.
 *
 * @param ack  Additional ack flag set in the received setup packet
 */
void S3TP::onSetup(bool ack, uint16_t sequenceNumber) {
    s3tpMutex.lock();
    if (ack) {
        // In case we received an ack for sync
        if (setupInitiated) {
            // We were initiator of setup and got an ack. We still need to ack the ack (3-way last step)
            //tx.scheduleSetup(true, sequenceNumber);
            setupPerformed = true;
            setupInitiated = false;
        } else {
            // We didn't initiate the setup and got back the last ack of the 3-way handshake.
            setupPerformed = true;
        }
    } else {
        // We weren't initiator, we need to send back an ack
        //TODO: check
        //tx.scheduleSetup(true, sequenceNumber);
        setupPerformed = false;
    }
    s3tpMutex.unlock();
}

/**
 * Received in case something went wrong on either side.
 * When received, internal state needs to be reset immediately.
 *
 * @param ack  Additional ack flag set in the received reset packet
 */
void S3TP::onReset(bool ack, uint16_t sequenceNumber) {
    s3tpMutex.lock();
    if (ack && resetting) {
        // Reset complete. We're good to go
        resetting = false;
    } else {
        // Received a force reset. Resetting everything (not rx, that was already resetted), then acknowledging it
        tx.reset();
        //tx.scheduleReset(true, sequenceNumber);
    }
    s3tpMutex.unlock();
}

void S3TP::onReceivedPacket(uint16_t sequenceNumber) {
    //Send ACK
    //tx.scheduleAcknowledgement(sequenceNumber);
}

void S3TP::onReceiveWindowFull(uint16_t lastValidSequence) {
    //tx.scheduleAcknowledgement(lastValidSequence);
}

void S3TP::onAcknowledgement(uint16_t sequenceAck) {
    //Notify TX that an ack was received
    //tx.notifyAcknowledgement(sequenceAck);
}

//TODO: implement
void S3TP::onConnectionRequest(uint8_t port, uint16_t sequenceNumber) {
    clientsMutex.lock();
    /*Client * cli = clients[port];
    if (cli == nullptr) {
        //No application listening on the given port. Respond with a NACK
        //TODO: respond with nack
        clientsMutex.unlock();
        return;
    }
    rx.openPort(port);*/
    uint8_t defaultOpts = S3TP_ARQ;
    //tx.scheduleSync(port, cli->getVirtualChannel(), defaultOpts, true, sequenceNumber);
    clientsMutex.unlock();
}

void S3TP::onConnectionAccept(uint8_t port, uint16_t sequenceNumber) {
    //tx.scheduleAcknowledgement(sequenceNumber);
    //Notify client that connection is now open
    clientsMutex.lock();
    /*Client * cli = clients[port];
    if (cli != nullptr) {
        cli->acceptConnect();
    }*/
    clientsMutex.unlock();
}

void S3TP::onConnectionClose(uint8_t port, uint16_t sequenceNumber) {
    //tx.scheduleAcknowledgement(sequenceNumber);
    //No 4-way close. We know the connection has been closed and that's it.
    //rx.closePort(port);
    //Notify client that connection is now closed
    clientsMutex.lock();
    /*Client * cli = clients[port];
    if (cli != nullptr) {
        cli->closeConnection();
    }*/
    clientsMutex.unlock();
}