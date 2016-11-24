//
// Created by lorenzodonini on 12.11.16.
//

#include "Connection.h"
#include "utilities.h"

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
    this->needsRetransmission = false;

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
    this->needsRetransmission = false;

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
    if (connectionListener != nullptr) {
        connectionListener->onConnectionStatusChanged(*this);
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

    outBuffer.push(pkt, this);
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

    outBuffer.push(pkt, this);
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

    outBuffer.push(pkt, this);
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

    outBuffer.push(pkt, this);
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

    if (connectionListener != nullptr) {
        connectionListener->onConnectionOutOfBandRequested(pkt);
    }
}

void Connection::_sack() {
    //Computing ranges
    uint8_t count = 0, i = expectedInSequence;
    std::vector<uint8_t> ranges;
    NumericRange currentRange;
    bool rangeStarted = false;
    while ((i - expectedInSequence) <= MAX_TRANSMISSION_WINDOW && count < numberOfScheduledAcknowledgements) {
        if (scheduledAcknowledgements[i]) {
            if (!rangeStarted) {
                rangeStarted = true;
                currentRange.start = i;
            }
            currentRange.end = i;
            scheduledAcknowledgements[i] = false;
            count += 1;
        } else if (rangeStarted) {
            ranges.emplace_back(currentRange.start);
            ranges.emplace_back(currentRange.end);
            rangeStarted = false;
        }
        i++;
    }
    numberOfScheduledAcknowledgements -= count;
    needsSelectiveAcknowledgement = false;

    //Putting ranges as payload of the control message
    uint16_t len = sizeof(char) * (ranges.size() + 1);
    char * pdu = new char[len];
    uint16_t j = 0;
    pdu[j++] = CTRL_SACK;
    for (auto range : ranges) {
        pdu[j++] = range;
    }
    S3TP_PACKET * pkt = new S3TP_PACKET(pdu, len);

    pkt->options = options;
    pkt->channel = virtualChannel;

    S3TP_HEADER * hdr = pkt->getHeader();
    hdr->srcPort = srcPort;
    hdr->destPort = dstPort;
    hdr->ack = expectedInSequence; //Everything before this sequence was received correctly
    hdr->seq = currentOutSequence++;
    hdr->setAck(true);
    hdr->setCtrl(true); //Special message used for transmitting selective acks

    //TODO: policy actor
    outBuffer.push(pkt, nullptr);
}

void Connection::_handleAcknowledgement(uint8_t sequence) {
    std::deque<std::shared_ptr<S3TP_PACKET>>::iterator it = retransmissionQueue.begin();
    while (it != retransmissionQueue.end()) {
        if (it->get()->getHeader()->seq == sequence) {
            retransmissionQueue.erase(it);
            break;
        }
        ++it;
    }
}

void Connection::_scheduleAcknowledgement(uint8_t ackSequence) {
    uint8_t offset = ackSequence - expectedInSequence;
    if (offset > MAX_TRANSMISSION_WINDOW) {
        //Old or invalid acknowledgement. Ignore
        return;
    }
    //Not handling double acknowledgements separately
    scheduledAcknowledgements[ackSequence] = true;
    numberOfScheduledAcknowledgements += 1;
    _updateCumulativeAcknowledgement(ackSequence);
}

//TODO: call function correctly upon receiving a message
void Connection::_updateCumulativeAcknowledgement(uint8_t sequence) {
    uint8_t i = expectedInSequence;
    uint8_t count = 0;
    while ((i - expectedInSequence) <= MAX_TRANSMISSION_WINDOW && count < numberOfScheduledAcknowledgements) {
        if (scheduledAcknowledgements[i]) {
            scheduledAcknowledgements[i] = false; //Clearing ack
            expectedInSequence += 1;
            i = expectedInSequence;
            count += 1;
        } else {
            needsSelectiveAcknowledgement = true;
            numberOfScheduledAcknowledgements -= count;
            return;
        }
    }
    numberOfScheduledAcknowledgements -= count;
    //If we got so far, it means that all packets were received correctly
    needsSelectiveAcknowledgement = false;
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
    inBuffer.lock();
    bool isAvailable = inBuffer.peek()->getHeader()->seq == expectedInSequence;
    inBuffer.unlock();

    return isAvailable;
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
    std::unique_lock<std::mutex> lock{connectionMutex};
    return srcPort;
}

uint8_t Connection::getDestinationPort() {
    std::unique_lock<std::mutex> lock{connectionMutex};
    return dstPort;
}

void Connection::waitForAvailableInMessage() {
    std::unique_lock<std::mutex> lock{connectionMutex};
    connectionCondition.wait(lock);
}

S3TP_PACKET * Connection::peekNextOutPacket() {
    S3TP_PACKET * pkt = outBuffer.peek();

    return pkt;
}

S3TP_PACKET * Connection::getNextOutPacket() {
    S3TP_PACKET * pkt = outBuffer.pop();

    return pkt;
}

S3TP_PACKET * Connection::peekNextInPacket() {
    S3TP_PACKET * pkt = inBuffer.peek();

    return pkt;
}

S3TP_PACKET * Connection::getNextInPacket() {
    return inBuffer.pop();
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
    if (needsSelectiveAcknowledgement) {
        _sack();
    } else {
        //Sending cumulative ack by default, even if it is redundant
        hdr->setAck(true);
        hdr->ack = expectedInSequence;
    }

    outBuffer.push(pkt, this);
    if (connectionListener != nullptr) {
        connectionListener->onNewOutPacket(*this);
    }

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
            _scheduleAcknowledgement(hdr->seq);
            expectedInSequence += 1; //TODO: needed?!
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

    //TODO: races?! make this better
    inBuffer.push(pkt, this);
    _scheduleAcknowledgement(hdr->seq);
    if (connectionListener != nullptr) {
        connectionListener->onNewInPacket(*this);
    }

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

/*
 * Policy Actor methods
 */
int Connection::comparePriority(S3TP_PACKET* element1, S3TP_PACKET* element2) {
    int comp = 0;
    uint8_t seq1, seq2;
    uint8_t offset;
    if (element1->getHeader()->srcPort == srcPort) {
        offset = currentOutSequence;
    } else {
        offset = expectedInSequence;
    }

    seq1 = element1->getHeader()->seq - offset;
    seq2 = element2->getHeader()->seq - offset;
    if (seq1 < seq2) {
        comp = -1; //Element 1 is lower, hence has higher priority
    } else if (seq1 > seq2) {
        comp = 1; //Element 2 is lower, hence has higher priority
    }

    return comp;
}

bool Connection::isElementValid(S3TP_PACKET * element) {
    //TODO: needed?!
    return true;
}

bool Connection::maximumWindowExceeded(S3TP_PACKET* queueHead, S3TP_PACKET* newElement) {
    //Implementation not needed inside Tx Module
    //TODO: needed?!
    return false;
}