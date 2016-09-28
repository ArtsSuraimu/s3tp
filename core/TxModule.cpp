//
// Created by Lorenzo Donini on 23/08/16.
//

#include "TxModule.h"

//Ctor
TxModule::TxModule() {
    state = WAITING;
    global_seq_num = 0;
    scheduled_sync = false;
    sendingFragments = false;
    active = false;
    currentPort = 0;

    //Setting up unique sync packet
    syncPacket.channel = DEFAULT_SYNC_CHANNEL;
    syncPacket.options = S3TP_OPTION_ARQ;
    S3TP_HEADER * hdr = syncPacket.getHeader();
    hdr->setMessageType(S3TP_MSG_SYNC);
    hdr->setPort(0);
    hdr->setGlobalSequence(0);
    hdr->setSubSequence(0);
    hdr->unsetMoreFragments();
    hdr->seq_port = 0;

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

void TxModule::synchronizeStatus() {
    //Not locking, as the method should only be called from a synchronized code block
    S3TP_SYNC * syncStructure = (S3TP_SYNC *)syncPacket.getPayload();
    std::fill(syncStructure->port_seq, syncStructure->port_seq + DEFAULT_MAX_OUT_PORTS, 0);
    syncStructure->tx_global_seq = global_seq_num;
    for (std::map<uint8_t, uint8_t>::iterator it = port_sequence.begin(); it != port_sequence.end(); ++it) {
        syncStructure->port_seq[it->first] = it->second;
    }

    S3TP_HEADER * hdr = syncPacket.getHeader();
    uint16_t crc = calc_checksum(syncPacket.getPayload(), hdr->getPduLength());
    hdr->crc = crc;

    bool arq = S3TP_ARQ;
    LOG_DEBUG("TX: Sync Packet sent to receiver");
    linkInterface->sendFrame(arq, syncPacket.channel, syncPacket.packet, syncPacket.getLength());
}

void TxModule::setStatusInterface(StatusInterface * statusInterface) {
    pthread_mutex_lock(&tx_mutex);
    this->statusInterface = statusInterface;
    pthread_mutex_unlock(&tx_mutex);
}

//Private methods
void TxModule::txRoutine() {
    pthread_mutex_lock(&tx_mutex);
    while(active) {
        if (!linkInterface->getLinkStatus() || !_channelsAvailable()) {
            state = BLOCKED;
            pthread_cond_wait(&tx_cond, &tx_mutex);
            continue;
        }
        //Sync has priority over any other packet
        if (scheduled_sync && _isChannelAvailable(DEFAULT_SYNC_CHANNEL)) {
            if (linkInterface->getBufferFull(DEFAULT_SYNC_CHANNEL)) {
                _setChannelAvailable(DEFAULT_SYNC_CHANNEL, false);
            } else {
                synchronizeStatus();
                scheduled_sync = false;
            }
        }
        if(!outBuffer->packetsAvailable()) {
            state = WAITING;
            pthread_cond_wait(&tx_cond, &tx_mutex);
            continue;
        }
        state = RUNNING;

        /*
         * As buffer only returns ordered packets per queue, we cannot split up fragmented messages
         * by randomly accessing different queues. This would mess up the global sequence number.
         * Instead, if we are transmitting a fragmented message, we prioritize the queue
         * which holds that message.
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
        pthread_mutex_unlock(&tx_mutex);

        LOG_DEBUG(std::string("TX: Packet sent from port " + std::to_string((int)hdr->getPort())
                              + " to Link Layer -> glob_seq: " + std::to_string((int)hdr->getGlobalSequence())
                              + ", sub_seq: " + std::to_string((int)hdr->getSubSequence())
                              + ", port_seq: " + std::to_string((int)hdr->seq_port)));

        bool arq = packet->options & S3TP_ARQ;

        //TODO: check if sendFrame failed. If yes, need to blacklist channel
        linkInterface->sendFrame(arq, packet->channel, packet->packet, packet->getLength());
        //TODO: save in history queue (once implemented)

        //Since a packet was just popped from the buffer, the queue is definitely available
        if (outBuffer->getSizeOfQueue(hdr->getPort()) + 1 == MAX_QUEUE_SIZE
            && statusInterface != nullptr) {
            statusInterface->onOutputQueueAvailable(hdr->getPort());
        }

        pthread_mutex_lock(&tx_mutex);

        delete packet;
    }
    pthread_mutex_unlock(&tx_mutex);

    pthread_exit(NULL);
}

void TxModule::scheduleSync(uint8_t syncId) {
    pthread_mutex_lock(&tx_mutex);
    scheduled_sync = true;
    S3TP_SYNC * syncStructure = (S3TP_SYNC *)syncPacket.getPayload();
    syncStructure->syncId = syncId;
    //Notifying routine thread that a new sync message is waiting
    pthread_cond_signal(&tx_cond);
    pthread_mutex_unlock(&tx_mutex);
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
    hdr->setMessageType(S3TP_MSG_DATA);
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

