//
// Created by Lorenzo Donini on 23/08/16.
//

#include "TxModule.h"

//Ctor
TxModule::TxModule() {
    state = WAITING;
    global_seq_num = 0;
    sendingFragments = false;
    active = false;
    retransmissionRequired = false;
    scheduledAck = false;
    currentPort = 0;
    lastAcknowledgedSequence = 0;
    expectedSequence = 0;

    //Setup prototype ack packet
    ackPacket.options = S3TP_ARQ;
    ackPacket.channel = DEFAULT_RESERVED_CHANNEL;
    S3TP_HEADER * hdr = ackPacket.getHeader();
    hdr->seq_port = 0;
    hdr->setAck(true);
    hdr->unsetMoreFragments();

    //Timer setup
    ackTimer.tv_sec = ACK_WAIT_TIME;
    ackTimer.tv_nsec = 0;
    elapsedTime = 0;
    retransmissionCount = 0;

    outBuffer = new Buffer(this);
    LOG_DEBUG("Created Tx Module");
}

//Dtor
TxModule::~TxModule() {
    txMutex.lock();
    state = WAITING;
    delete outBuffer;
    txMutex.unlock();
    LOG_DEBUG("Destroyed Tx Module");
}

void TxModule::reset() {
    txMutex.lock();
    state = WAITING;
    global_seq_num = 0;
    sendingFragments = false;
    retransmissionRequired = false;
    scheduledAck = false;
    currentPort = 0;
    lastAcknowledgedSequence = 0;
    expectedSequence = 0;
    port_sequence.clear();
    to_consume_port_seq.clear();
    outBuffer->clear();
    txMutex.unlock();
}

void TxModule::sendAcknowledgement() {
    S3TP_HEADER * hdr = ackPacket.getHeader();
    hdr->ack = expectedSequence;
    hdr->crc = 0;

    bool arq = S3TP_ARQ;
    LOG_DEBUG(std::string("TX: ----------- Empty Ack Packet with sequence "
                          + std::to_string((int)expectedSequence) +" sent -----------"));
    linkInterface->sendFrame(arq, ackPacket.channel, ackPacket.packet, ackPacket.getLength());
    //TODO: check if channel is available
}

void TxModule::setStatusInterface(StatusInterface * statusInterface) {
    txMutex.lock();
    this->statusInterface = statusInterface;
    txMutex.unlock();
}

//Private methods
void TxModule::txRoutine() {
    S3TP_PACKET * packet;

    std::unique_lock<std::mutex> lock(txMutex);
    start = std::chrono::system_clock::now();
    while(active) {
        // Checking if transceiver can currently transmit
        if (!linkInterface->getLinkStatus() || !_channelsAvailable()) {
            state = BLOCKED;
            txCond.wait(lock);
            continue;
        }

        // Checking if transmission window size was reached
        if (safeQueue.size() == MAX_TRANSMISSION_WINDOW) {
            //Maximum size of window reached, need to wait for acks
            state = BLOCKED;
            while (safeQueue.size() == MAX_TRANSMISSION_WINDOW) {
                _timedWait();
                if (elapsedTime >= ACK_WAIT_TIME) {
                    retransmissionRequired = true;
                    //TODO: logic error, need to go to retransmission somehow
                    break;
                }
                //Go for another iteration
                break;
            }
            continue;
        }

        /* PRIORITY 1
         * Packets need to be retransmitted.
         * Once this starts, all packets will be sent out at once */
        if (retransmissionRequired) {
            //TODO: implement retransmission limit and queue flush
            state = RUNNING;
            retransmitPackets();
            retransmissionCount++;
            retransmissionRequired = false;
            //Updating time where we sent the last packet
            start = std::chrono::system_clock::now();
            continue;
        }

        /* PRIORITY 2
         * Fragmented messages in queue have priority over other messages,
         * as their shared global sequence must not be overridden (messages cannot be split up) */
        if (sendingFragments) {
            packet = outBuffer->getNextPacket(currentPort);
            if (packet == NULL) {
                //Channels are currently blocked and packets cannot be sent
                state = BLOCKED;
                txCond.wait(lock);
                continue;
            }
            state = RUNNING;
            _sendDataPacket(packet);
            //Updating time where we sent the last packet
            start = std::chrono::system_clock::now();
            continue;
        }

        /* PRIORITY 3
         * Control messages are sent before normal messages. */
        if (!controlQueue.empty() && _isChannelAvailable(DEFAULT_RESERVED_CHANNEL)) {
            if (linkInterface->getBufferFull(DEFAULT_RESERVED_CHANNEL)) {
                _setChannelAvailable(DEFAULT_RESERVED_CHANNEL, false);
            } else {
                packet = controlQueue.front();
                _sendControlPacket(packet);
                controlQueue.pop();
                //Updating time where we sent the last packet
                start = std::chrono::system_clock::now();
            }
            continue;
        }

        /* PRIORITY 4
         * Attempt to send out normal data. These normal messages may
         * retain acknowledgements, which are sent back in piggybacking */
        if (outBuffer->packetsAvailable()) {
            packet = outBuffer->getNextAvailablePacket();
            if (packet == NULL) {
                //Channels are currently blocked and packets cannot be sent
                state = BLOCKED;
                txCond.wait(lock);
                continue;
            }
            state = RUNNING;
            _sendDataPacket(packet);
            continue;
        } else {
            //In case no data is available, we may still want to send out an ack
            if (scheduledAck && _isChannelAvailable(DEFAULT_RESERVED_CHANNEL)) {
                if (linkInterface->getBufferFull(DEFAULT_RESERVED_CHANNEL)) {
                    _setChannelAvailable(DEFAULT_RESERVED_CHANNEL, false);
                } else {
                    sendAcknowledgement();
                    scheduledAck = false;
                }
            }
            state = WAITING;
            if (safeQueue.empty()) {
                //No more acks to send. Wait indefinitely, until a packet needs to be sent out
                txCond.wait(lock);
            } else {
                //Still waiting for acks. Need to retransmit after a certain time.
                _timedWait();
                if (elapsedTime >= ACK_WAIT_TIME) {
                    retransmissionRequired = true;
                }
            }
            continue;
        }
    }
    //Thread will die on its own
}

void TxModule::_timedWait() {
    //Update time to wait
    now = std::chrono::system_clock::now();
    auto difference = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
    elapsedTime = difference.count();
    if (elapsedTime >= ACK_WAIT_TIME) {
        //Timer already expired
        return;
    }
    int remainingTime = ACK_WAIT_TIME - (int)elapsedTime;
    std::chrono::milliseconds ms(remainingTime);

    //Wait
    std::unique_lock<std::mutex> lock(txMutex);
    txCond.wait_for(lock, ms);

    //Update elapsed time for control logic
    now = std::chrono::system_clock::now();
    difference = std::chrono::duration_cast<std::chrono::seconds>(now - start);
    elapsedTime = difference.count();
}

void TxModule::_sendDataPacket(S3TP_PACKET *pkt) {
    S3TP_HEADER * hdr = pkt->getHeader();
    currentPort = hdr->getPort();
    sendingFragments = hdr->moreFragments();

    hdr->setGlobalSequence(global_seq_num);
    if (!hdr->moreFragments()) {
        //Need to increase the current global sequence
        global_seq_num++;
    }
    to_consume_port_seq[hdr->getPort()]++;

    //Acks can be sent in piggybacking, only inside non ctrl packets
    if (scheduledAck && !(hdr->getFlags() & S3TP_FLAG_CTRL)) {
        hdr->setAck(true);
        hdr->ack = expectedSequence;
        scheduledAck = false;
    }

    //Saving packet inside history queue
    safeQueue.push_back(pkt);

    txMutex.unlock();

    LOG_DEBUG(std::string("TX: Data Packet sent from port " + std::to_string((int)hdr->getPort())
                          + " to Link Layer -> glob_seq: " + std::to_string((int)hdr->getGlobalSequence())
                          + ", sub_seq: " + std::to_string((int)hdr->getSubSequence())
                          + ", port_seq: " + std::to_string((int)hdr->seq_port)));

    bool arq = pkt->options & S3TP_ARQ;

    if (linkInterface->sendFrame(arq, pkt->channel, pkt->packet, pkt->getLength()) < 0) {
        //Blacklisting channel
        txMutex.lock();
        _setChannelAvailable(pkt->channel, false);
        //TODO: make this better somehow
        retransmissionRequired = true;
        txMutex.unlock();
    }

    //Since a packet was just popped from the buffer, the queue is definitely available
    if (outBuffer->getSizeOfQueue(hdr->getPort()) + 1 == MAX_QUEUE_SIZE
        && statusInterface != nullptr) {
        statusInterface->onOutputQueueAvailable(hdr->getPort());
    }

    txMutex.lock();
}

void TxModule::_sendControlPacket(S3TP_PACKET *pkt) {
    S3TP_HEADER * hdr = pkt->getHeader();
    hdr->setGlobalSequence(global_seq_num++);

    safeQueue.push_back(pkt);
    txMutex.unlock();

    LOG_DEBUG(std::string("TX: Control Packet sent to Link Layer -> glob_seq: "
                          + std::to_string((int)hdr->getGlobalSequence())
                          + ", sub_seq: " + std::to_string((int)hdr->getSubSequence())));

    bool arq = pkt->options & S3TP_ARQ;

    if (linkInterface->sendFrame(arq, pkt->channel, pkt->packet, pkt->getLength()) < 0) {
        //Blacklisting channel
        txMutex.lock();
        _setChannelAvailable(pkt->channel, false);
        //TODO: make this better somehow
        retransmissionRequired = true;
    } else {
        txMutex.lock();
    }
}

/**
 * Called usually after receiving a packet.
 * After scheduling an ack, the TX worker thread will either send out a dedicated ack packet
 * or simply put the scheduled acknowledgement in piggybacking to another packet.
 *
 * @param ackSequence  The next expected sequence, to be sent to the receiver
 * @param control  Flag indicating whether the ack is for a control message or not
 */
void TxModule::scheduleAcknowledgement(uint16_t ackSequence) {
    txMutex.lock();
    //TODO: handle control ack?!
    scheduledAck = true;
    expectedSequence = ackSequence;
    //Notifying routine thread that a new ack message needs to be sent out
    txCond.notify_all();
    txMutex.unlock();
}

/**
 * Need to perform a hard reset of the protocol status.
 * After reset request is sent out, the worker thread waits until it receives an acknowledgement.
 */
void TxModule::scheduleReset(bool ack, uint16_t ackSequence) {
    S3TP_CONTROL control;
    control.type = CONTROL_TYPE::RESET;
    S3TP_PACKET * packet = new S3TP_PACKET((char *)&control, sizeof(S3TP_CONTROL));
    packet->channel = DEFAULT_RESERVED_CHANNEL;
    packet->options = S3TP_ARQ;
    S3TP_HEADER * hdr = packet->getHeader();
    hdr->seq = 0;
    hdr->seq_port = 0;
    hdr->setPort(0);
    hdr->setAck(ack);
    hdr->setCtrl(true);
    hdr->ack = ackSequence;
    hdr->crc = calc_checksum((char *)&control, sizeof(S3TP_CONTROL));

    txMutex.lock();
    controlQueue.push(packet);
    txMutex.unlock();
    LOG_DEBUG("TX: Scheduled RESET Packet");
}

/**
 * Tells the other S3TP endpoint (if any), that this module has just been started and therefore
 * requests an initial sync. While all the sequence numbers on this side are typically set
 * to 0 at this stage, the other endpoint might have different sequences.
 * Upon receiving a response, the sequences expected by the other endpoint will be known.
 *
 * @param ack  An ack flag to be set, in case we need to acknowledge a received setup packet (3-way handshake).
 */
void TxModule::scheduleSetup(bool ack, uint16_t ackSequence) {
    S3TP_CONTROL control;
    control.type = CONTROL_TYPE::SETUP;
    S3TP_PACKET * packet = new S3TP_PACKET((char *)&control, sizeof(S3TP_CONTROL));
    packet->channel = DEFAULT_RESERVED_CHANNEL;
    packet->options = S3TP_ARQ;
    S3TP_HEADER * hdr = packet->getHeader();
    hdr->seq = 0;
    hdr->seq_port = 0;
    hdr->setPort(0);
    hdr->setAck(ack);
    hdr->setCtrl(true);
    hdr->ack = ackSequence;
    hdr->crc = calc_checksum((char *)&control, sizeof(S3TP_CONTROL));

    txMutex.lock();
    controlQueue.push(packet);
    txCond.notify_all();
    txMutex.unlock();
    LOG_DEBUG("TX: Scheduled SETUP Packet");
}

/**
 * Send out a synchronization packet, used for telling the other endpoint that a new connection
 * is being opened. The application is attempting to synchronize the current sequence number.
 *
 * This packet is not sent over a prioritized channel, but on the channel specified by the application.
 *
 * @param port  The port that was just opened
 */
void TxModule::scheduleSync(uint8_t port, uint8_t channel, uint8_t options, bool ack, uint16_t ackSequence) {
    S3TP_CONTROL control;
    control.type = CONTROL_TYPE::SYNC;
    S3TP_PACKET * packet = new S3TP_PACKET((char *)&control, sizeof(S3TP_CONTROL));
    packet->channel = channel;
    packet->options = options;
    S3TP_HEADER * hdr = packet->getHeader();
    hdr->setAck(ack);
    hdr->setCtrl(true);
    hdr->ack = ackSequence;
    hdr->setPort(port);

    enqueuePacket(packet, 0, false, channel, options);
    LOG_DEBUG(std::string("TX: Scheduled SYNC Packet for port " + std::to_string((int)port)));
}

/**
 * Send out a finalization packet, used for telling the other endpoint that an existing
 * connection is being shutdown.
 * @param port  The port that was just closed
 */
void TxModule::scheduleFin(uint8_t port, uint8_t channel, uint8_t options, bool ack, uint16_t ackSequence) {
    S3TP_CONTROL control;
    control.type = CONTROL_TYPE::FIN;
    S3TP_PACKET * packet = new S3TP_PACKET((char *)&control, sizeof(S3TP_CONTROL));
    packet->channel = channel;
    packet->options = options;
    S3TP_HEADER * hdr = packet->getHeader();
    hdr->setAck(ack);
    hdr->setCtrl(true);
    hdr->ack = ackSequence;
    hdr->setPort(port);

    enqueuePacket(packet, 0, false, channel, options);
    LOG_DEBUG(std::string("TX: Scheduled FIN Packet for port " + std::to_string((int)port)));
}

/**
 * Notify TX that an ack for the passed sequence number was received,
 * hence all packets up to that point don't need to be retransmitted.
 *
 * @param ackSequence  The acknowledged sequence number (received previously)
 */
void TxModule::notifyAcknowledgement(uint16_t ackSequence) {
    S3TP_PACKET * pkt;
    uint16_t ackRelativeSeq = 0, currentSeq = 0;

    txMutex.lock();

    if (safeQueue.empty()) {
        //Received a spurious (probably an older ACK coming in. Ignore it). Out queue is empty anyway.
        txMutex.unlock();
        return;
    }

    ackRelativeSeq = ackSequence - lastAcknowledgedSequence;
    if (ackRelativeSeq > 0 && ackRelativeSeq < MAX_TRANSMISSION_WINDOW) {
        //Clean output safe queue
        while (!safeQueue.empty()) {
            pkt = safeQueue.front();
            currentSeq = pkt->getHeader()->seq - lastAcknowledgedSequence;
            if (currentSeq < ackRelativeSeq) {
                safeQueue.pop_front();
                delete pkt;
            } else {
                break;
            }
        }
        retransmissionRequired = false;
        retransmissionCount = 0;
        lastAcknowledgedSequence = ackSequence;
    } else if (ackSequence == lastAcknowledgedSequence) {
        //We received the same ack several times. Some packets got dropped by the receiver therefore.
        retransmissionRequired = true;
    }

    txCond.notify_all();
    txMutex.unlock();
}

/*
 * Routine section
 */
void TxModule::retransmitPackets() {
    S3TP_HEADER * hdr;
    uint16_t relativePktSeq;

    LOG_DEBUG("TX: Starting retransmission of lost packets");

    for (auto const& pkt: safeQueue) {
        hdr = pkt->getHeader();
        hdr->setAck(false);
        relativePktSeq = hdr->seq - lastAcknowledgedSequence;
        if (relativePktSeq > MAX_TRANSMISSION_WINDOW) {
            //Looks like the sequence was updated
            continue;
        }

        txMutex.unlock();

        LOG_DEBUG(std::string("TX: Packet sent from port " + std::to_string((int)hdr->getPort())
                              + " to Link Layer -> glob_seq: " + std::to_string((int)hdr->getGlobalSequence())
                              + ", sub_seq: " + std::to_string((int)hdr->getSubSequence())
                              + ", port_seq: " + std::to_string((int)hdr->seq_port)));

        bool arq = pkt->options & S3TP_ARQ;

        while (linkInterface->getBufferFull(pkt->channel)) {
            std::unique_lock<std::mutex> lock(txMutex);
            txCond.wait(lock);
            if (linkInterface->sendFrame(arq, pkt->channel, pkt->packet, pkt->getLength()) < 0) {
                //Send frame failed. Either underlying buffer is full or channel is broken
                //TODO: implement safety mechanism
                continue;
            }
        }
    }
}

//Public methods
void TxModule::startRoutine(Transceiver::LinkInterface * spi_if) {
    txMutex.lock();
    linkInterface = spi_if;
    active = true;
    txThread = std::thread(&TxModule::txRoutine, this);
    txMutex.unlock();

    LOG_DEBUG(std::string("TX Thread: START"));
}

void TxModule::stopRoutine() {
    txMutex.lock();
    active = false;
    txCond.notify_all();
    txMutex.unlock();
    if (txThread.joinable()) {
        txThread.join();
    }
    LOG_DEBUG("TX Thread: STOP");
}

TxModule::STATE TxModule::getCurrentState() {
    txMutex.lock();
    STATE current = state;
    txMutex.unlock();
    return current;
}

bool TxModule::isQueueAvailable(uint8_t port, uint8_t no_packets) {
    return outBuffer->getSizeOfQueue(port) + no_packets <= MAX_QUEUE_SIZE;
}

void TxModule::setChannelAvailable(uint8_t channel, bool available) {
    txMutex.lock();
    if (available) {
        channel_blacklist.erase(channel);
        LOG_DEBUG(std::string("Whitelisted channel " + std::to_string((int)channel)));
        txCond.notify_all();
    } else {
        channel_blacklist.insert(channel);
        LOG_DEBUG(std::string("Blacklisted channel " + std::to_string((int)channel)));
    }
    txMutex.unlock();
}

bool TxModule::isChannelAvailable(uint8_t channel) {
    txMutex.lock();
    //result = channel is not in blacklist
    bool result = channel_blacklist.find(channel) == channel_blacklist.end();
    txMutex.unlock();
    return result;
}

/*
 * Internal channel utility methods
 */
bool TxModule::_channelsAvailable() {
    return channel_blacklist.size() < S3TP_VIRTUAL_CHANNELS;
}

void TxModule::_setChannelAvailable(uint8_t channel, bool available) {
    if (available) {
        channel_blacklist.erase(channel);
        LOG_DEBUG(std::string("Whitelisted channel " + std::to_string((int)channel)));
        txCond.notify_all();
    } else {
        channel_blacklist.insert(channel);
        LOG_DEBUG(std::string("Blacklisted channel " + std::to_string((int)channel)));
    }
}

bool TxModule::_isChannelAvailable(uint8_t channel) {
    return channel_blacklist.find(channel) == channel_blacklist.end();
}

void TxModule::notifyLinkAvailability(bool available) {
    if (available) {
        txCond.notify_all();
    }
}

/**
 * This method is supposed to be called with an already well-formed S3TP packet.
 * The header fields will be filled within this method, but the length of the payload must be already set.
 * Header fields that will be filled automatically include:
 * - global sequence number;
 * - sub-sequence number (used for fragmentation);
 * - port sequence;
 * - CRC.
 */
int TxModule::enqueuePacket(S3TP_PACKET * packet,
                            uint8_t frag_no,
                            bool more_fragments,
                            uint8_t spi_channel,
                            uint8_t options) {
    txMutex.lock();
    if (!active) {
        //If is not active, do not attempt to enqueue something
        txMutex.unlock();
        return CODE_INACTIVE_ERROR;
    }
    S3TP_HEADER * hdr = packet->getHeader();
    hdr->setData(true);
    //Not setting global seq, as it will be set by transmission thread, when actually sending the packet to L2
    hdr->setSubSequence(frag_no);
    if (more_fragments) {
        hdr->setMoreFragments();
    } else {
        hdr->unsetMoreFragments();
    }
    //Increasing port sequence
    int port = hdr->getPort();
    hdr->seq_port = port_sequence[port]++;
    txMutex.unlock();

    uint16_t crc = calc_checksum(packet->getPayload(), hdr->getPduLength());
    hdr->crc = crc;

    outBuffer->write(packet);
    txCond.notify_all();

    return CODE_SUCCESS;
}

int TxModule::comparePriority(S3TP_PACKET* element1, S3TP_PACKET* element2) {
    int comp = 0;
    uint8_t seq1, seq2, offset;
    txMutex.lock();

    offset = to_consume_port_seq[element1->getHeader()->getPort()];
    seq1 = element1->getHeader()->seq_port - offset;
    seq2 = element2->getHeader()->seq_port - offset;
    if (seq1 < seq2) {
        comp = -1; //Element 1 is lower, hence has higher priority
    } else if (seq1 > seq2) {
        comp = 1; //Element 2 is lower, hence has higher priority
    }
    txMutex.unlock();
    return comp;
}

bool TxModule::isElementValid(S3TP_PACKET * element) {
    return _isChannelAvailable(element->channel);
}

bool TxModule::maximumWindowExceeded(S3TP_PACKET* queueHead, S3TP_PACKET* newElement) {
    //Implementation not needed inside Tx Module
    return false;
}

