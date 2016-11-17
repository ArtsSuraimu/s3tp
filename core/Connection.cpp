//
// Created by lorenzodonini on 12.11.16.
//

#include "Connection.h"

/*
 * CTOR/DTOR
 */
Connection::Connection(uint8_t srcPort, uint8_t dstPort, uint8_t virtualChannel, uint8_t options) {
    this->currentState = DISCONNECTED;
    this->srcPort = srcPort;
    this->dstPort = dstPort;
    this->virtualChannel = virtualChannel;
    this->options = options;
    this->currentOutSequence = 0;
    this->expectedInSequence = 0;

    connectionMutex.lock();
    _syn();
    connectionMutex.unlock();
}

Connection::Connection(S3TP_PACKET * synPacket) {
    connectionMutex.lock();
    this->currentState = DISCONNECTED;
    this->virtualChannel = synPacket->channel;
    this->options = synPacket->options;
    S3TP_HEADER * hdr = synPacket->getHeader();
    this->srcPort = hdr->destPort;
    this->dstPort = hdr->srcPort;
    this->expectedInSequence = hdr->seq;
    this->expectedInSequence += 1;
    this->currentOutSequence = 0;

    _syncAck();
    connectionMutex.unlock();
}

Connection::~Connection() {
    connectionMutex.lock();
    currentState = DISCONNECTED;
    connectionMutex.unlock();

    S3TP_PACKET * packet;
    //Clear output queue
    outBuffer.lock();
    while (!outBuffer.isEmpty()) {
        packet = outBuffer.pop();
        delete packet;
    }
    outBuffer.unlock();

    //Clear input queue
    inBuffer.lock();
    while (!inBuffer.isEmpty()) {
        packet = inBuffer.pop();
        delete packet;
    }
    inBuffer.unlock();
}

/*
 * Private Methods
 */
void Connection::updateState(STATE newState) {
    currentState = newState;
    if (statusInterface != nullptr) {
        statusInterface->onConnectionStatusChanged(*this);
    }
}

void Connection::_syn() {
    S3TP_PACKET * pkt = new S3TP_PACKET(emptyPdu, 0);

    pkt->options = options;
    pkt->channel = virtualChannel;
    S3TP_HEADER * hdr = pkt->getHeader();

    hdr->srcPort = srcPort;
    hdr->destPort = dstPort;
    hdr->ack = 0;
    hdr->seq = currentOutSequence++;
    hdr->setSyn(true);

    //TODO: policy actor
    outBuffer.push(pkt, nullptr);
    updateState(CONNECTING);
}

void Connection::_syncAck() {
    S3TP_PACKET * pkt = new S3TP_PACKET(emptyPdu, 0);

    pkt->options = options;
    pkt->channel = virtualChannel;

    S3TP_HEADER * hdr = pkt->getHeader();
    hdr->srcPort = srcPort;
    hdr->destPort = dstPort;
    hdr->ack = expectedInSequence;
    hdr->seq = currentOutSequence++;
    hdr->setSyn(true);
    hdr->setAck(true);

    //TODO: policy actor
    outBuffer.push(pkt, nullptr);
    updateState(CONNECTING);
}

void Connection::_fin() {
    S3TP_PACKET * pkt = new S3TP_PACKET(emptyPdu, 0);

    pkt->options = options;
    pkt->channel = virtualChannel;

    S3TP_HEADER * hdr = pkt->getHeader();
    hdr->srcPort = srcPort;
    hdr->destPort = dstPort;
    hdr->ack = 0;
    hdr->seq = currentOutSequence++;
    hdr->setFin(true);

    //TODO: policy actor
    outBuffer.push(pkt, nullptr);
    updateState(DISCONNECTING);
}

void Connection::_finAck(uint8_t sequence) {
    S3TP_PACKET * pkt = new S3TP_PACKET(emptyPdu, 0);

    pkt->options = options;
    pkt->channel = virtualChannel;

    S3TP_HEADER * hdr = pkt->getHeader();
    hdr->srcPort = srcPort;
    hdr->destPort = dstPort;
    hdr->ack = sequence;
    hdr->seq = currentOutSequence++;
    hdr->setFin(true);
    hdr->setAck(true);

    //TODO: policy actor
    outBuffer.push(pkt, nullptr);
    updateState(DISCONNECTING);
}

void Connection::_onFinAck(uint8_t sequence) {
    S3TP_PACKET * pkt = new S3TP_PACKET(emptyPdu, 0);

    pkt->options = options;
    pkt->channel = virtualChannel;

    S3TP_HEADER * hdr = pkt->getHeader();
    hdr->srcPort = srcPort;
    hdr->destPort = dstPort;
    hdr->ack = sequence;
    hdr->seq = currentOutSequence++;
    hdr->setAck(true);

    if (statusInterface != nullptr) {
        statusInterface->onConnectionOutOfBandRequested(pkt);
    }
}

void Connection::_handleAcknowledgement(uint8_t sequence) {
    //TODO: implement
}

/*
 * PUBLIC METHODS
 */
STATE Connection::getCurrentState() {
    return currentState;
}

bool Connection::outPacketsAvailable() {
    outBuffer.lock();
    bool isEmpty = outBuffer.isEmpty();
    outBuffer.unlock();

    return !isEmpty;
}

bool Connection::inPacketsAvailable() {
    //TODO: implement
    return false;
}

bool Connection::isOutBufferFull() {
    outBuffer.lock();
    bool isFull = outBuffer.getSize() >= MAX_QUEUE_SIZE;
    outBuffer.unlock();

    return isFull;
}

bool Connection::isInBufferFull() {
    inBuffer.lock();
    bool isFull = inBuffer.getSize() >= MAX_QUEUE_SIZE;
    inBuffer.unlock();

    return isFull;
}

bool Connection::canWriteBytesOut(int bytes) {
    outBuffer.lock();
    bool result = (outBuffer.getSize() + bytes) > MAX_QUEUE_SIZE;
    outBuffer.unlock();

    return result;
}

bool Connection::canWriteBytesIn(int bytes) {
    inBuffer.lock();
    bool result = (inBuffer.getSize() + bytes) > MAX_QUEUE_SIZE;
    inBuffer.unlock();

    return result;
}

uint8_t Connection::getSourcePort() {
    connectionMutex.lock();
    uint8_t port = srcPort;
    connectionMutex.unlock();

    return port;
}

uint8_t Connection::getDestinationPort() {
    connectionMutex.lock();
    uint8_t port = dstPort;
    connectionMutex.unlock();

    return port;
}

S3TP_PACKET * Connection::peekNextOutPacket() {
    outBuffer.lock();
    S3TP_PACKET * pkt = outBuffer.peek();
    outBuffer.unlock();

    return pkt;
}

S3TP_PACKET * Connection::getNextOutPacket() {
    outBuffer.lock();
    S3TP_PACKET * pkt = outBuffer.pop();
    outBuffer.unlock();

    return pkt;
}

S3TP_PACKET * Connection::peekNextInPacket() {
    inBuffer.lock();
    S3TP_PACKET * pkt = inBuffer.peek();
    inBuffer.unlock();

    return pkt;
}

S3TP_PACKET * Connection::getNextInPacket() {
    inBuffer.lock();
    S3TP_PACKET * pkt = inBuffer.pop();
    inBuffer.unlock();

    return pkt;
}

int Connection::sendOutPacket(S3TP_PACKET * pkt) {
    connectionMutex.lock();

    if (currentState != CONNECTED) {
        connectionMutex.unlock();
        return CODE_CONNECTION_ERROR;
    } else if (outBuffer.getSize() + pkt->getLength() > MAX_QUEUE_SIZE) {
        connectionMutex.unlock();
        return CODE_BUFFER_FULL;
    }
    pkt->options = options;
    pkt->channel = virtualChannel;

    S3TP_HEADER * hdr = pkt->getHeader();
    hdr->srcPort = srcPort;
    hdr->destPort = dstPort;
    hdr->seq = currentOutSequence++;
    if (!scheduledAcknowledgements.empty()) {
        hdr->setAck(true);
        hdr->ack = scheduledAcknowledgements.front();
        scheduledAcknowledgements.pop();
    }

    outBuffer.lock();
    //TODO: use policy actor
    outBuffer.push(pkt, nullptr);
    outBuffer.unlock();

    connectionMutex.unlock();

    return CODE_OK;
}

int Connection::receiveInPacket(S3TP_PACKET * pkt) {
    connectionMutex.lock();

    if (currentState != CONNECTED && currentState != CONNECTING && currentState != DISCONNECTING) {
        connectionMutex.unlock();
        delete pkt;
        return CODE_CONNECTION_ERROR;
    } else if (inBuffer.getSize() + pkt->getLength() > MAX_QUEUE_SIZE) {
        //TODO: reset the connection? Drop packet?
        connectionMutex.unlock();
        delete pkt;
        return CODE_BUFFER_FULL;
    }

    //TODO: check message flags
    S3TP_HEADER * hdr = pkt->getHeader();
    uint8_t flags = hdr->getFlags();
    if (flags & FLAG_SYN) {
        /*
         * SYN LOGIC
         */
        if (flags & FLAG_ACK) {
            //Received a SYN ACK packet
            if (currentState != CONNECTING) {
                connectionMutex.unlock();
                delete pkt;
                return CODE_INVALID_PACKET;
            }
            _handleAcknowledgement(hdr->ack);
            scheduleAcknowledgement(hdr->seq);
            expectedInSequence += 1;
            updateState(CONNECTED);
            connectionMutex.unlock();
            delete pkt;
            return CODE_OK;
        } else if (flags & FLAG_RST) {
            //Attempted to connect but received a RST. Port is not open on other side
            updateState(DISCONNECTED);
            connectionMutex.unlock();
            delete pkt;
            return CODE_OK;
        } else {
            connectionMutex.unlock();
            delete pkt;
            return CODE_INVALID_PACKET;
        }
    } else if (flags & FLAG_FIN) {
        /*
         * FIN LOGIC
         */
        if (flags & FLAG_ACK) {
            //Received a FIN ACK message
            if (currentState != DISCONNECTING) {
                connectionMutex.unlock();
                delete pkt;
                return CODE_INVALID_PACKET;
            } else {
                _handleAcknowledgement(hdr->ack);
                _onFinAck(hdr->seq);
                updateState(DISCONNECTED);
                delete pkt;
                return CODE_OK;
            }
        } else {
            //Receiving a FIN packet
            _finAck(hdr->seq);
            connectionMutex.unlock();
            delete pkt;
            return CODE_OK;
        }
    } else if (flags & FLAG_RST) {
        /*
         * RST LOGIC
         */
        updateState(DISCONNECTED);
        connectionMutex.unlock();
        delete pkt;
        return CODE_OK;
    } else if (flags & FLAG_ACK) {
        /*
         * ACK LOGIC
         */
        _handleAcknowledgement(hdr->ack);
    }

    //TODO: use policy actor
    inBuffer.push(pkt, nullptr);

    connectionMutex.unlock();

    return CODE_OK;
}

void Connection::reset() {
    connectionMutex.lock();

    //TODO: implement logic properly
    currentOutSequence = 0;
    expectedInSequence = 0;
    updateState(RESETTING);

    connectionMutex.unlock();
}

void Connection::close() {
    connectionMutex.lock();

    if (currentState == CONNECTED) {
        _fin();
    }
    connectionMutex.unlock();
}