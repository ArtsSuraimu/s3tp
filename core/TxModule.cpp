//
// Created by Lorenzo Donini on 23/08/16.
//

#include "TxModule.h"

//Ctor
TxModule::TxModule() {
    state = WAITING;
    pthread_mutex_init(&tx_mutex, NULL);
    pthread_cond_init(&tx_cond, NULL);
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
        if(!outBuffer.packetsAvailable()) {
            state = WAITING;
            pthread_cond_wait(&tx_cond, &tx_mutex);
            continue;
        }
        state = RUNNING;
        S3TP_PACKET_WRAPPER * wrapper = outBuffer.getNextAvailablePacket();
        if (wrapper == NULL) {
            continue;
        }
        pthread_mutex_unlock(&tx_mutex);
        S3TP_PACKET * pkt = wrapper->pkt;
        printf("TX: Packet sent from port %d to SPI -> glob_seq %d; sub_seq %d; port_seq %d\n",
               pkt->hdr.port & 0x7F,
               (pkt->hdr.seq >> 8),
               (pkt->hdr.seq & 0xFF),
               pkt->hdr.seq_port);

        bool arq = true; //wrapper->options && S3TP_OPTION_ARQ;
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
    uint8_t global_seq = global_seq_num;
    uint8_t sub_seq = (uint8_t) frag_no;
    packet->hdr.seq = (uint8_t)((global_seq << 8) | (sub_seq & 0xFF));
    if (more_fragments) {
        packet->hdr.setMoreFragments();
    } else {
        packet->hdr.unsetMoreFragments();
        //Need to increase the current global sequence
        global_seq_num++;
    }
    //Increasing port sequence
    packet->hdr.seq_port = port_sequence[packet->hdr.port]++;
    pthread_mutex_unlock(&tx_mutex);

    char * dataPtr = (char *)packet->pdu;
    uint16_t crc = calc_checksum(dataPtr, packet->hdr.pdu_length);
    packet->hdr.crc = crc;

    S3TP_PACKET_WRAPPER * wrapper = new S3TP_PACKET_WRAPPER();
    wrapper->pkt = packet;
    wrapper->channel = (uint8_t) spi_channel;
    outBuffer.write(wrapper);
    pthread_cond_signal(&tx_cond);

    return CODE_SUCCESS;
}

