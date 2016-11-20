//
// Created by Lorenzo Donini on 23/08/16.
//

#include "TxModule.h"

//Ctor
TxModule::TxModule() {
    state = WAITING;
    active = false;
    retransmissionRequired = false;

    //Timer setup
    elapsedTime = 0;
    retransmissionCount = 0;

    LOG_DEBUG("Created Tx Module");
}

//Dtor
TxModule::~TxModule() {
    txMutex.lock();
    state = WAITING;
    txMutex.unlock();
    LOG_DEBUG("Destroyed Tx Module");
}

void TxModule::reset() {
    txMutex.lock();
    state = WAITING;
    retransmissionRequired = false;
    txMutex.unlock();
}

void TxModule::setStatusInterface(StatusInterface * statusInterface) {
    txMutex.lock();
    this->statusInterface = statusInterface;
    txMutex.unlock();
}

//Private methods
void TxModule::txRoutine() {
    std::shared_ptr<S3TP_PACKET> packet;

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
        /*if (safeQueue.size() == MAX_TRANSMISSION_WINDOW) {
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
        }*/

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
        /*if (sendingFragments) {
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
        }*/

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
        /*if (outBuffer->packetsAvailable()) {
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
        }*/
    }
    //Thread will die on its own
}

/*
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
}*/

void TxModule::_sendControlPacket(std::shared_ptr<S3TP_PACKET> pkt) {
    txMutex.unlock();

    /*LOG_DEBUG(std::string("[TX]: Control Packet sent to Link Layer -> seq: "
                          + std::to_string((int)hdr->seq)));*/

    bool arq = pkt->options & S3TP_ARQ;

    if (linkInterface->sendFrame(arq, pkt->channel, pkt->packet, pkt->getLength()) < 0) {
        //Blacklisting channel
        txMutex.lock();
        _setChannelAvailable(pkt->channel, false);
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

/**
 * Need to perform a hard reset of the protocol status.
 * After reset request is sent out, the worker thread waits until it receives an acknowledgement.
 */

/**
 * Tells the other S3TP endpoint (if any), that this module has just been started and therefore
 * requests an initial sync. While all the sequence numbers on this side are typically set
 * to 0 at this stage, the other endpoint might have different sequences.
 *
 * When receiving this message, the other party will know that this the protocol on this
 * endpoint just started up, and will act accordingly (i.e. reset all open connections)
 *
 * @param ack  An ack flag to be set, in case we need to acknowledge a received setup packet (3-way handshake).
 */

/**
 * Send out a synchronization packet, used for telling the other endpoint that a new connection
 * is being opened. The application is attempting to synchronize the current sequence number.
 *
 * This packet is not sent over a prioritized channel, but on the channel specified by the application.
 *
 * @param port  The port that was just opened
 */

/**
 * Send out a finalization packet, used for telling the other endpoint that an existing
 * connection is being shutdown.
 * @param port  The port that was just closed
 */

/**
 * Notify TX that an ack for the passed sequence number was received,
 * hence all packets up to that point don't need to be retransmitted.
 *
 * @param ackSequence  The acknowledged sequence number (received previously)
 */

/*
 * Routine section
 */
void TxModule::retransmitPackets() {
    S3TP_HEADER * hdr;
    uint16_t relativePktSeq;

    LOG_DEBUG("TX: Starting retransmission of lost packets");

    /*for (auto const& pkt: safeQueue) {
        hdr = pkt->getHeader();
        hdr->setAck(false);
        relativePktSeq = hdr->seq - lastAcknowledgedSequence;
        if (relativePktSeq > MAX_TRANSMISSION_WINDOW) {
            //Looks like the sequence was updated
            continue;
        }

        txMutex.unlock();

        LOG_DEBUG(std::string("[TX] Data Packet sent. SRC: " + std::to_string((int)hdr->srcPort)
                              + ", DST: " + std::to_string((int)hdr->destPort)
                              + ", SEQ: " + std::to_string((int)hdr->seq)
                              + ", LEN: " + std::to_string((int)hdr->pdu_length)));

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
    }*/
}

//Public methods
void TxModule::startRoutine(Transceiver::LinkInterface * spi_if, std::shared_ptr<ConnectionManager> connectionManager) {
    txMutex.lock();
    linkInterface = spi_if;
    this->connectionManager = connectionManager;
    this->connectionManager->setOutPacketListener(this);
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

//TODO: make direct call to connection
bool TxModule::isQueueAvailable(uint8_t port, uint8_t no_packets) {
    //return outBuffer->getSizeOfQueue(port) + no_packets <= MAX_QUEUE_SIZE;
    return false;
}

void TxModule::setChannelAvailable(uint8_t channel, bool available) {
    txMutex.lock();
    if (available) {
        channelBlacklist.erase(channel);
        LOG_DEBUG(std::string("Whitelisted channel " + std::to_string((int)channel)));
        txCond.notify_all();
    } else {
        channelBlacklist.insert(channel);
        LOG_DEBUG(std::string("Blacklisted channel " + std::to_string((int)channel)));
    }
    txMutex.unlock();
}

bool TxModule::isChannelAvailable(uint8_t channel) {
    txMutex.lock();
    //result = channel is not in blacklist
    bool result = channelBlacklist.find(channel) == channelBlacklist.end();
    txMutex.unlock();
    return result;
}

/*
 * Internal channel utility methods
 */
bool TxModule::_channelsAvailable() {
    return channelBlacklist.size() < S3TP_VIRTUAL_CHANNELS;
}

void TxModule::_setChannelAvailable(uint8_t channel, bool available) {
    if (available) {
        channelBlacklist.erase(channel);
        LOG_DEBUG(std::string("Whitelisted channel " + std::to_string((int)channel)));
        txCond.notify_all();
    } else {
        channelBlacklist.insert(channel);
        LOG_DEBUG(std::string("Blacklisted channel " + std::to_string((int)channel)));
    }
}

bool TxModule::_isChannelAvailable(uint8_t channel) {
    return channelBlacklist.find(channel) == channelBlacklist.end();
}

void TxModule::notifyLinkAvailability(bool available) {
    if (available) {
        txCond.notify_all();
    }
}

/*
 * On new packet to send callback
 */
void TxModule::onNewOutPacket(Connection& connection) {
    txMutex.lock();
    if (!active) {
        //Not active, should throw an error
        txMutex.unlock();
        return;
    }

    S3TP_PACKET * pkt = connection.peekNextOutPacket();
    if (pkt != nullptr) {
        if (state == WAITING
            && linkInterface->getLinkStatus()
            && _isChannelAvailable(pkt->channel)) {
            //Attempt to forward packet to link layer
            bool arq = pkt->options & (uint8_t)S3TP_ARQ;
            if (linkInterface->sendFrame(arq, pkt->channel, pkt->packet, pkt->getLength()) < 0) {
                //Blacklisting channel
                _setChannelAvailable(pkt->channel, false);
            } else {
                //Packet forwarded successfully, popping from connection out buffer
                connection.getNextOutPacket();
                S3TP_HEADER * hdr = pkt->getHeader();
                LOG_DEBUG(std::string("[TX] Data Packet sent. SRC: " + std::to_string((int)hdr->srcPort)
                                      + ", DST: " + std::to_string((int)hdr->destPort)
                                      + ", SEQ: " + std::to_string((int)hdr->seq)
                                      + ", LEN: " + std::to_string((int)hdr->pdu_length)));
            }
        } else {
            //Scheduling async thread to send this packet in the future
            connectionRoundRobin.push(connection.getSourcePort());
        }
        txCond.notify_all();
        txMutex.unlock();
    }
}

/*
 * Policy Actor methods
 */
int TxModule::comparePriority(S3TP_PACKET* element1, S3TP_PACKET* element2) {
    int comp = 0;
    uint8_t seq1, seq2, offset;
    txMutex.lock();

    //offset = to_consume_port_seq[element1->getHeader()->srcPort];
    seq1 = element1->getHeader()->seq - offset;
    seq2 = element2->getHeader()->seq - offset;
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

