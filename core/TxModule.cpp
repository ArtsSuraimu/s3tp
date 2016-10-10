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
    syncTimer.tv_sec = SYNC_WAIT_TIME;
    syncTimer.tv_nsec = 0;

    pthread_mutex_init(&tx_mutex, NULL);
    pthread_cond_init(&tx_cond, NULL);
    outBuffer = new Buffer(this);
    LOG_DEBUG("Created Tx Module");
}

//Dtor
TxModule::~TxModule() {
    pthread_mutex_lock(&tx_mutex);
    state = WAITING;
    delete outBuffer;
    pthread_mutex_unlock(&tx_mutex);
    pthread_mutex_destroy(&tx_mutex);
    LOG_DEBUG("Destroyed Tx Module");
}

void TxModule::reset() {
    pthread_mutex_lock(&tx_mutex);
    global_seq_num = 0;
    port_sequence.clear();
    to_consume_port_seq.clear();
    outBuffer->clear();
    pthread_mutex_unlock(&tx_mutex);
}

/*void TxModule::synchronizeStatus() {
    //Not locking, as the method should only be called from a synchronized code block
    S3TP_SYNC * syncStructure = (S3TP_SYNC *)syncPacket.getPayload();
    std::fill(syncStructure->port_seq, syncStructure->port_seq + DEFAULT_MAX_OUT_PORTS, 0);
    syncStructure->tx_global_seq = global_seq_num;
    for (std::map<uint8_t, uint8_t>::iterator it = port_sequence.begin(); it != port_sequence.end(); ++it) {
        S3TP_PACKET * pkt = outBuffer->peektNextPacket(it->first);
        if (pkt != NULL) {
            syncStructure->port_seq[it->first] = pkt->getHeader()->seq_port;
        } else {
            syncStructure->port_seq[it->first] = it->second;
        }
    }

    S3TP_HEADER * hdr = syncPacket.getHeader();
    uint16_t crc = calc_checksum(syncPacket.getPayload(), hdr->getPduLength());
    hdr->crc = crc;

    bool arq = S3TP_ARQ;
    LOG_DEBUG("TX: ----------- Sync Packet sent to receiver -----------");
    linkInterface->sendFrame(arq, syncPacket.channel, syncPacket.packet, syncPacket.getLength());
}*/

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
    pthread_mutex_lock(&tx_mutex);
    this->statusInterface = statusInterface;
    pthread_mutex_unlock(&tx_mutex);
}

//Private methods
void TxModule::txRoutine() {
    double elapsed_time = 0;
    int milliseconds = 0;

    pthread_mutex_lock(&tx_mutex);
    while(active) {
        if (!linkInterface->getLinkStatus() || !_channelsAvailable()) {
            state = BLOCKED;
            pthread_cond_wait(&tx_cond, &tx_mutex);
            continue;
        }
        //Sync has priority over any other packet
        if (_isChannelAvailable(DEFAULT_RESERVED_CHANNEL)) {
            if (linkInterface->getBufferFull(DEFAULT_RESERVED_CHANNEL)) {
                _setChannelAvailable(DEFAULT_RESERVED_CHANNEL, false);
            } else {
                synchronizeStatus();
                //Force synchronization
                pthread_cond_wait(&tx_cond, &tx_mutex);
                continue;
            }
        }
        //Packets need to be retransmitted
        if (retransmissionRequired) {
            state = RUNNING;
            retransmitPackets();
            retransmissionRequired = false;
            continue;
        }

        //TODO: refactor this
        if (!outBuffer->packetsAvailable()) {
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
            pthread_cond_wait(&tx_cond, &tx_mutex);
            continue;
        } else if (safeQueue.size() == MAX_TRANSMISSION_WINDOW) {
            //Maximum size of window reached, need to wait for acks
            state = BLOCKED;
            std::chrono::time_point<std::chrono::system_clock> start, now;
            std::chrono::duration<double> time_difference;
            start = std::chrono::system_clock::now();

            ackTimer.tv_sec = ACK_WAIT_TIME;
            ackTimer.tv_nsec = 0;
            while (safeQueue.size() == MAX_TRANSMISSION_WINDOW) {
                pthread_cond_timedwait(&tx_cond, &tx_mutex, &ackTimer);
                now = std::chrono::system_clock::now();
                time_difference = now - start;
                elapsed_time = time_difference.count();
                if (elapsed_time >= ACK_WAIT_TIME || retransmissionRequired) {
                    //Timer should be ended
                    break;
                }
                //Filling ackTimer with new value
                ackTimer.tv_sec = ACK_WAIT_TIME - (int)elapsed_time;
                milliseconds = ((int)(elapsed_time * 1000)) % 1000;
                ackTimer.tv_nsec = milliseconds * 1000000;
            }
            continue;
        }

        //Send normal data
        state = RUNNING;

        /*
         * As buffer only returns ordered packets per queue, we cannot split up fragmented messages
         * by randomly accessing different queues. This would mess up the global sequence number.
         * Instead, if we are transmitting a fragmented message, we prioritize the queue
         * which holds that message. If the channel gets blocked, we block as well.
         */
        S3TP_PACKET * packet = (sendingFragments) ?
                               outBuffer->getNextPacket(currentPort) :
                               outBuffer->getNextAvailablePacket();

        if (packet == NULL) {
            //Channels are currently blocked and packets cannot be sent
            state = BLOCKED;
            pthread_cond_wait(&tx_cond, &tx_mutex);
            continue;
        }
        S3TP_HEADER * hdr = packet->getHeader();
        currentPort = hdr->getPort();
        sendingFragments = hdr->moreFragments();

        hdr->setGlobalSequence(global_seq_num);
        if (!hdr->moreFragments()) {
            //Need to increase the current global sequence
            global_seq_num++;
        }
        to_consume_port_seq[hdr->getPort()]++;

        //Acks can be sent in piggybacking
        if (scheduledAck) {
            hdr->setAck(true);
            hdr->ack = expectedSequence;
            scheduledAck = false;
        }

        //Saving packet inside history queue
        safeQueue.push_back(packet);

        pthread_mutex_unlock(&tx_mutex);

        LOG_DEBUG(std::string("TX: Packet sent from port " + std::to_string((int)hdr->getPort())
                              + " to Link Layer -> glob_seq: " + std::to_string((int)hdr->getGlobalSequence())
                              + ", sub_seq: " + std::to_string((int)hdr->getSubSequence())
                              + ", port_seq: " + std::to_string((int)hdr->seq_port)));

        bool arq = packet->options & S3TP_ARQ;

        if (linkInterface->sendFrame(arq, packet->channel, packet->packet, packet->getLength()) < 0) {
            //Blacklisting channel
            pthread_mutex_lock(&tx_mutex);
            _setChannelAvailable(packet->channel, false);
            //TODO: make this better somehow
            retransmissionRequired = true;
            pthread_mutex_unlock(&tx_mutex);
        }

        //Since a packet was just popped from the buffer, the queue is definitely available
        if (outBuffer->getSizeOfQueue(hdr->getPort()) + 1 == MAX_QUEUE_SIZE
            && statusInterface != nullptr) {
            statusInterface->onOutputQueueAvailable(hdr->getPort());
        }

        pthread_mutex_lock(&tx_mutex);
    }
    pthread_mutex_unlock(&tx_mutex);

    pthread_exit(NULL);
}

/*void TxModule::scheduleSync(uint8_t syncId) {
    pthread_mutex_lock(&tx_mutex);
    scheduled_sync = true;
    S3TP_SYNC * syncStructure = (S3TP_SYNC *)syncPacket.getPayload();
    syncStructure->syncId = syncId;
    //Notifying routine thread that a new sync message is waiting
    pthread_cond_signal(&tx_cond);
    pthread_mutex_unlock(&tx_mutex);
}*/

/**
 * Called whenever we receive an acknowledgment for a previously sent message.
 * @param ackSequence  The sequence number expected next by the receiver
 */
/*
void TxModule::notifyAcknowledgement(uint16_t ackSequence) {
    S3TP_PACKET * pkt;

    pthread_mutex_lock(&tx_mutex);

    uint16_t relativeOldSeq, relativePktSeq = ackSequence - lastAcknowledgedSequence;
    if (relativePktSeq >= 0 && relativePktSeq < MAX_TRANSMISSION_WINDOW) {
        //Clean output safe queue
        while (!safeQueue.empty()) {
            pkt = safeQueue.front();
            relativeOldSeq = pkt->getHeader()->seq - lastAcknowledgedSequence;
            if (relativeOldSeq < relativePktSeq) {
                safeQueue.pop_front();
                delete pkt;
            } else {
                break;
            }
        }
        lastAcknowledgedSequence = ackSequence;
        retransmissionRequired = false;
    } else if (relativePktSeq == 0) {
        //We received an ack several times. Some packets got dropped by the receiver.
        //TODO: handle
        retransmissionRequired = true;
    }
    //In case the sequence is a different number, it might be an older ACK coming in. Ignore it

    pthread_cond_signal(&tx_cond);
    pthread_mutex_unlock(&tx_mutex);
}

void TxModule::notifySynchronization(bool synchronized) {
    pthread_mutex_lock(&tx_mutex);
    this->scheduled_sync = !synchronized;
    pthread_cond_signal(&tx_cond);
    pthread_mutex_unlock(&tx_mutex);
}*/

/**
 * Called usually after receiving a packet.
 * After scheduling an ack, the TX worker thread will either send out a dedicated ack packet
 * or simply put the scheduled acknowledgement in piggybacking to another packet.
 *
 * @param ackSequence  The next expected sequence, to be sent to the receiver
 * @param control  Flag indicating whether the ack is for a control message or not
 */
void TxModule::scheduleAcknowledgement(uint16_t ackSequence, bool control) {
    pthread_mutex_lock(&tx_mutex);
    //TODO: handle control
    scheduledAck = true;
    expectedSequence = ackSequence;
    //Notifying routine thread that a new ack message needs to be sent out
    pthread_cond_signal(&tx_cond);
    pthread_mutex_unlock(&tx_mutex);
}

/**
 * Need to perform a hard reset of the protocol status.
 * After reset request is sent out, the worker thread waits until it receives an acknowledgement.
 */
void TxModule::scheduleReset(bool ack) {
    S3TP_CONTROL control;
    control.type = CONTROL_TYPE::RESET;
    control.syncSequence = 0;
    S3TP_PACKET * packet = new S3TP_PACKET((char *)&control, sizeof(S3TP_CONTROL));
    S3TP_HEADER * hdr = packet->getHeader();
    hdr->seq = 0;
    hdr->setPort(0);
    hdr->setAck(ack);
    hdr->setCtrl(true);

    pthread_mutex_lock(&tx_mutex);
    controlQueue.push(packet);
    pthread_mutex_unlock(&tx_mutex);
}

/**
 * Tells the other S3TP endpoint (if any), that this module has just been started and therefore
 * requests an initial sync. While all the sequence numbers on this side are typically set
 * to 0 at this stage, the other endpoint might have different sequences.
 * Upon receiving a response, the sequences expected by the other endpoint will be known.
 *
 * @param ack  An ack flag to be set, in case we need to acknowledge a received setup packet (3-way handshake).
 */
void TxModule::scheduleSetup(bool ack) {
    S3TP_CONTROL control;
    control.type = CONTROL_TYPE::SETUP;
    control.syncSequence = 0;
    S3TP_PACKET * packet = new S3TP_PACKET((char *)&control, sizeof(S3TP_CONTROL));
    S3TP_HEADER * hdr = packet->getHeader();
    hdr->seq = 0;
    hdr->setPort(0);
    hdr->setAck(ack);
    hdr->setCtrl(true);

    pthread_mutex_lock(&tx_mutex);
    controlQueue.push(packet);
    pthread_mutex_unlock(&tx_mutex);
}

/**
 * Send out a synchronization packet, used for telling the other endpoint that a new connection
 * is being opened. The application is attempting to synchronize the current sequence number.
 *
 * This packet is not sent over a prioritized channel, but on the channel specified by the application.
 *
 * @param port  The port that was just opened
 */
void TxModule::scheduleSync(uint8_t port, uint8_t channel, uint8_t options) {
    S3TP_CONTROL control;
    control.type = CONTROL_TYPE::SYNC;
    control.syncSequence = 0;
    S3TP_PACKET * packet = new S3TP_PACKET((char *)&control, sizeof(S3TP_CONTROL));
    packet->getHeader()->setCtrl(true);
    packet->getHeader()->setPort(port);
    packet->channel = channel;
    packet->options = options;

    enqueuePacket(packet, 0, false, channel, options);
}

/**
 * Send out a finalization packet, used for telling the other endpoint that an existing
 * connection is being shutdown.
 * @param port  The port that was just closed
 */
void TxModule::scheduleFin(uint8_t port, uint8_t channel, uint8_t options) {
    S3TP_CONTROL control;
    control.type = CONTROL_TYPE::FIN;
    control.syncSequence = 0;
    S3TP_PACKET * packet = new S3TP_PACKET((char *)&control, sizeof(S3TP_CONTROL));
    packet->getHeader()->setCtrl(true);
    packet->getHeader()->setPort(port);
    packet->channel = channel;
    packet->options = options;

    enqueuePacket(packet, 0, false, channel, options);
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

    pthread_mutex_lock(&tx_mutex);

    if (safeQueue.empty()) {
        //Received a spurious (probably an older ACK coming in. Ignore it). Out queue is empty anyway.
        pthread_mutex_unlock(&tx_mutex);
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
        lastAcknowledgedSequence = ackSequence;
    } else if (ackSequence == lastAcknowledgedSequence) {
        //We received the same ack several times. Some packets got dropped by the receiver therefore.
        //TODO: handle
        retransmissionRequired = true;
    }

    pthread_cond_signal(&tx_cond);
    pthread_mutex_unlock(&tx_mutex);
}

/*
 * Routine section
 */
void TxModule::retransmitPackets() {
    S3TP_HEADER * hdr;
    uint16_t relativePktSeq;

    for (auto const& pkt: safeQueue) {
        hdr = pkt->getHeader();
        hdr->setAck(false);
        relativePktSeq = hdr->seq - lastAcknowledgedSequence;
        if (relativePktSeq > MAX_TRANSMISSION_WINDOW) {
            //Looks like the sequence was updated
            continue;
        }

        pthread_mutex_unlock(&tx_mutex);

        LOG_DEBUG(std::string("TX: Packet sent from port " + std::to_string((int)hdr->getPort())
                              + " to Link Layer -> glob_seq: " + std::to_string((int)hdr->getGlobalSequence())
                              + ", sub_seq: " + std::to_string((int)hdr->getSubSequence())
                              + ", port_seq: " + std::to_string((int)hdr->seq_port)));

        bool arq = pkt->options & S3TP_ARQ;

        if (linkInterface->sendFrame(arq, pkt->channel, pkt->packet, pkt->getLength()) < 0) {
            //Send frame failed. Either underlying buffer is full or channel is broken
            //TODO: implement safety mechanism
        }
    }
}

void * TxModule::staticTxRoutine(void * args) {
    static_cast<TxModule*>(args)->txRoutine();
    return NULL;
}

//Public methods
void TxModule::startRoutine(Transceiver::LinkInterface * spi_if) {
    pthread_mutex_lock(&tx_mutex);
    linkInterface = spi_if;
    active = true;
    int txId = pthread_create(&tx_thread, NULL, &TxModule::staticTxRoutine, this);
    pthread_mutex_unlock(&tx_mutex);

    LOG_DEBUG(std::string("TX Thread (id " + std::to_string(txId) + "): START"));
}

void TxModule::stopRoutine() {
    pthread_mutex_lock(&tx_mutex);
    active = false;
    pthread_cond_signal(&tx_cond);
    pthread_mutex_unlock(&tx_mutex);
    pthread_join(tx_thread, NULL);
    LOG_DEBUG("TX Thread: STOP");
}

TxModule::STATE TxModule::getCurrentState() {
    pthread_mutex_lock(&tx_mutex);
    STATE current = state;
    pthread_mutex_unlock(&tx_mutex);
    return current;
}

bool TxModule::isQueueAvailable(uint8_t port, uint8_t no_packets) {
    return outBuffer->getSizeOfQueue(port) + no_packets <= MAX_QUEUE_SIZE;
}

void TxModule::setChannelAvailable(uint8_t channel, bool available) {
    pthread_mutex_lock(&tx_mutex);
    if (available) {
        channel_blacklist.erase(channel);
        LOG_DEBUG(std::string("Whitelisted channel " + std::to_string((int)channel)));
        pthread_cond_signal(&tx_cond);
    } else {
        channel_blacklist.insert(channel);
        LOG_DEBUG(std::string("Blacklisted channel " + std::to_string((int)channel)));
    }
    pthread_mutex_unlock(&tx_mutex);
}

bool TxModule::isChannelAvailable(uint8_t channel) {
    pthread_mutex_lock(&tx_mutex);
    //result = channel is not in blacklist
    bool result = channel_blacklist.find(channel) == channel_blacklist.end();
    pthread_mutex_unlock(&tx_mutex);
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
        pthread_cond_signal(&tx_cond);
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
        pthread_cond_signal(&tx_cond);
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
    pthread_mutex_lock(&tx_mutex);
    if (!active) {
        //If is not active, do not attempt to enqueue something
        pthread_mutex_unlock(&tx_mutex);
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
    pthread_mutex_unlock(&tx_mutex);

    uint16_t crc = calc_checksum(packet->getPayload(), hdr->getPduLength());
    hdr->crc = crc;

    outBuffer->write(packet);
    pthread_cond_signal(&tx_cond);

    return CODE_SUCCESS;
}

int TxModule::comparePriority(S3TP_PACKET* element1, S3TP_PACKET* element2) {
    int comp = 0;
    uint8_t seq1, seq2, offset;
    pthread_mutex_lock(&tx_mutex);

    offset = to_consume_port_seq[element1->getHeader()->getPort()];
    seq1 = element1->getHeader()->seq_port - offset;
    seq2 = element2->getHeader()->seq_port - offset;
    if (seq1 < seq2) {
        comp = -1; //Element 1 is lower, hence has higher priority
    } else if (seq1 > seq2) {
        comp = 1; //Element 2 is lower, hence has higher priority
    }
    pthread_mutex_unlock(&tx_mutex);
    return comp;
}

bool TxModule::isElementValid(S3TP_PACKET * element) {
    return _isChannelAvailable(element->channel);
}

bool TxModule::maximumWindowExceeded(S3TP_PACKET* queueHead, S3TP_PACKET* newElement) {
    //Implementation not needed inside Tx Module
    return false;
}

