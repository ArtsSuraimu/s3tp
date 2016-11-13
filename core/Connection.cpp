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
    currentState = CONNECTING;
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
    currentState = CONNECTING;
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
    currentState = DISCONNECTING;
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
    currentState = DISCONNECTING;
}

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

    return CODE_BUFFER_WRITE_OK;
}

int Connection::receiveInPacket(S3TP_PACKET * pkt) {
    connectionMutex.lock();

    if (currentState != CONNECTED) {
        connectionMutex.unlock();
        return CODE_CONNECTION_ERROR;
    } else if (inBuffer.getSize() + pkt->getLength() > MAX_QUEUE_SIZE) {
        //TODO: reset the connection? Drop packet?
        connectionMutex.unlock();
        return CODE_BUFFER_FULL;
    }

    //TODO: check message flags

    //TODO: use policy actor
    inBuffer.push(pkt, nullptr);

    connectionMutex.unlock();

    return CODE_BUFFER_WRITE_OK;
}

void Connection::reset() {
    //TODO: implement
}

void Connection::close() {
    connectionMutex.lock();

    if (currentState == CONNECTED) {
        _fin();
    }
    connectionMutex.unlock();
}