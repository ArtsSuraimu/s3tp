//
// Created by Lorenzo Donini on 23/08/16.
//

#include "TxModule.h"

//Ctor
TxModule::TxModule() {
    state = WAITING;
    active = false;

    currentControlSequence = 0;

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
    currentControlSequence = 0;
    txMutex.unlock();
}

void TxModule::setStatusInterface(StatusInterface * statusInterface) {
    txMutex.lock();
    this->statusInterface = statusInterface;
    txMutex.unlock();
}

//Private methods
void TxModule::txRoutine() {
    std::shared_ptr<S3TP_PACKET> pkt;
    std::shared_ptr<Connection> connection;

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

        state = RUNNING;

        /* PRIORITY 1
         * Control packets need to be retransmitted */
        if (needsControlRetransmission && _isChannelAvailable(DEFAULT_RESERVED_CHANNEL)) {
            if (linkInterface->getBufferFull(DEFAULT_RESERVED_CHANNEL)) {
                _setChannelAvailable(DEFAULT_RESERVED_CHANNEL, false);
            } else {
                int retransmitted = retransmitControlPackets();
                if (retransmitted > 0) {
                    start = std::chrono::system_clock::now();
                }
                if (retransmitted == controlRetransmissionQueue.size()) {
                    needsControlRetransmission = false;
                }
                //TODO: adjust logic

                continue;
            }
        }

        /* PRIORITY 2
         * Control packets need to be sent */
        if (!controlQueue.empty() && _isChannelAvailable(DEFAULT_RESERVED_CHANNEL)) {
            if (linkInterface->getBufferFull(DEFAULT_RESERVED_CHANNEL)) {
                _setChannelAvailable(DEFAULT_RESERVED_CHANNEL, false);
            } else {
                pkt = controlQueue.front();
                if (_sendControlPacket(pkt)) {
                    controlQueue.pop();
                    controlRetransmissionQueue.push(pkt);
                    start = std::chrono::system_clock::now();
                }
                continue;
            }
        }

        /* PRIORITY 3
         * Normal flow, send out a message from a queue,
         * which couldn't previously be sent right away */
        if (!connectionRoundRobin.empty()) {
            //Attempt first request in queue
            uint8_t port = connectionRoundRobin.front();
            connectionRoundRobin.pop();
            connection = connectionManager->getConnection(port);
            if (connection != nullptr) {
                pkt = connection->peekNextOutPacket();
                if (pkt != nullptr && _isChannelAvailable(pkt->channel)) {
                    //Attempt to forward packet to link layer
                    bool arq = pkt->options & (uint8_t)S3TP_ARQ;
                    if (linkInterface->sendFrame(arq, pkt->channel, pkt->packet, pkt->getLength()) < 0) {
                        //Blacklisting channel
                        _setChannelAvailable(pkt->channel, false);
                        //Will retry this message later
                        connectionRoundRobin.push(port);
                    } else {
                        //Packet forwarded successfully, popping from connection out buffer
                        connection->getNextOutPacket();
                        S3TP_HEADER * hdr = pkt->getHeader();
                        LOG_DEBUG(std::string("[TX] Data Packet sent. SRC: " + std::to_string((int)hdr->srcPort)
                                              + ", DST: " + std::to_string((int)hdr->destPort)
                                              + ", SEQ: " + std::to_string((int)hdr->seq)
                                              + ", LEN: " + std::to_string((int)hdr->pdu_length)));
                    }
                } else {
                    //Will retry this message later
                    connectionRoundRobin.push(port);
                }
            }
            continue;
        } else {
            state = WAITING;
            txCond.wait(lock);
        }
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

bool TxModule::_sendControlPacket(std::shared_ptr<S3TP_PACKET> pkt) {
    S3TP_HEADER * hdr = pkt->getHeader();
    LOG_DEBUG(std::string("[TX]: Control Packet sent to Link Layer -> seq: "
                          + std::to_string((int)hdr->seq)));

    bool arq = pkt->options & (uint8_t)S3TP_ARQ;

    if (linkInterface->sendFrame(arq, pkt->channel, pkt->packet, pkt->getLength()) < 0) {
        //Blacklisting channel
        _setChannelAvailable(pkt->channel, false);
        return false;
    }
    return true;
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
int TxModule::retransmitControlPackets() {
    //TODO: reimplement
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
    return 0;
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

/*
 * S3TP Control APIs
 */
void TxModule::scheduleInitialSetup() {
    char pdu [1];
    pdu[0] = CTRL_SETUP;
    std::shared_ptr<S3TP_PACKET> pkt = new S3TP_PACKET(pdu, sizeof(char) * sizeof(pdu));

    S3TP_HEADER * hdr = pkt->getHeader();
    hdr->seq = currentControlSequence++;
    //Ports are not important for this out-of-band packet
    hdr->srcPort = 0;
    hdr->destPort = 0;
    hdr->setCtrl(true);

    controlQueue.push(pkt);
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
        txCond.notify_all(); //TODO: needed?!
        txMutex.unlock();
    }
}
