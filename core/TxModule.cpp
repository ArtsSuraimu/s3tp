//
// Created by Lorenzo Donini on 23/08/16.
//

#include "TxModule.h"

//Ctor
TxModule::TxModule() {
    state = WAITING;
    global_seq_num = 0;
    to_consume_global_seq = 0;
    pthread_mutex_init(&tx_mutex, NULL);
    pthread_cond_init(&tx_cond, NULL);
    outBuffer = new Buffer(this);
    printf("Created Tx Module\n");
}

//Dtor
TxModule::~TxModule() {
    pthread_mutex_lock(&tx_mutex);
    pthread_mutex_unlock(&tx_mutex);
    pthread_mutex_destroy(&tx_mutex);
    printf("Destroyed Tx Module\n");
}

//Private methods
void TxModule::txRoutine() {
    pthread_mutex_lock(&tx_mutex);
    while(active) {
        if (!linkInterface->getLinkStatus()) {
            state = BLOCKED;
            pthread_cond_wait(&tx_cond, &tx_mutex);
            continue;
        }
        if(!outBuffer->packetsAvailable()) {
            state = WAITING;
            pthread_cond_wait(&tx_cond, &tx_mutex);
            continue;
        }
        state = RUNNING;
        S3TP_PACKET_WRAPPER * wrapper = outBuffer->getNextAvailablePacket();
        if (wrapper == NULL) {
            continue;
        }
        pthread_mutex_unlock(&tx_mutex);
        S3TP_PACKET * pkt = wrapper->pkt;
        to_consume_global_seq = pkt->hdr.getGlobalSequence() + (uint8_t)1;
        to_consume_port_seq[pkt->hdr.getPort()]++;
        printf("TX: Packet sent from port %d to SPI -> glob_seq %d; sub_seq %d; port_seq %d\n",
               pkt->hdr.getPort(),
               (pkt->hdr.getGlobalSequence()),
               (pkt->hdr.getSubSequence()),
               pkt->hdr.seq_port);

        bool arq = wrapper->options && S3TP_ARQ;
        linkInterface->sendFrame(arq, wrapper->channel, pkt, sizeof(S3TP_PACKET));
        delete wrapper;
        pthread_mutex_lock(&tx_mutex);
    }
    pthread_mutex_unlock(&tx_mutex);

    pthread_exit(NULL);
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
    printf("Tx Thread (id %d): START\n", txId);
}

void TxModule::stopRoutine() {
    pthread_mutex_lock(&tx_mutex);
    active = false;
    pthread_cond_signal(&tx_cond);
    pthread_mutex_unlock(&tx_mutex);
}

TxModule::STATE TxModule::getCurrentState() {
    pthread_mutex_lock(&tx_mutex);
    STATE current = state;
    pthread_mutex_unlock(&tx_mutex);
    return current;
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
    packet->hdr.setGlobalSequence(global_seq_num);
    packet->hdr.setSubSequence(frag_no);
    if (more_fragments) {
        packet->hdr.setMoreFragments();
    } else {
        packet->hdr.unsetMoreFragments();
        //Need to increase the current global sequence
        global_seq_num++;
    }
    //Increasing port sequence
    int port = packet->hdr.getPort();
    packet->hdr.seq_port = port_sequence[port]++;
    pthread_mutex_unlock(&tx_mutex);

    uint16_t crc = calc_checksum(packet->pdu, packet->hdr.getPduLength());
    packet->hdr.crc = crc;

    S3TP_PACKET_WRAPPER * wrapper = new S3TP_PACKET_WRAPPER();
    wrapper->pkt = packet;
    wrapper->options = options;
    wrapper->channel = (uint8_t) spi_channel;
    outBuffer->write(wrapper);
    pthread_cond_signal(&tx_cond);

    return CODE_SUCCESS;
}

int TxModule::comparePriority(S3TP_PACKET_WRAPPER* element1, S3TP_PACKET_WRAPPER* element2) {
    int comp = 0;
    uint8_t seq1, seq2, offset;
    pthread_mutex_lock(&tx_mutex);
    offset = to_consume_global_seq;
    //First check global seq number for comparison
    seq1 = element1->pkt->hdr.getGlobalSequence() - offset;
    seq2 = element2->pkt->hdr.getGlobalSequence() - offset;
    if (seq1 < seq2) {
        comp = -1; //Element 1 is lower, hence has higher priority
    } else if (seq1 > seq2) {
        comp = 1; //Element 2 is lower, hence has higher priority
    }
    if (comp != 0) {
        pthread_mutex_unlock(&tx_mutex);
        return comp;
    }
    offset = to_consume_port_seq[element1->pkt->hdr.getPort()];
    seq1 = element1->pkt->hdr.seq_port - offset;
    seq2 = element2->pkt->hdr.seq_port - offset;
    if (seq1 < seq2) {
        comp = -1; //Element 1 is lower, hence has higher priority
    } else if (seq1 > seq2) {
        comp = 1; //Element 2 is lower, hence has higher priority
    }
    pthread_mutex_unlock(&tx_mutex);
    return comp;
}

