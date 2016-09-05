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
        //TODO: send packet to SPI interface
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
void TxModule::startRoutine(void* spi_if) {
    pthread_mutex_lock(&tx_mutex);
    spi_interface = spi_if;
    active = true;
    pthread_create(&tx_thread, NULL, &TxModule::staticTxRoutine, this);
    pthread_mutex_unlock(&tx_mutex);
    __uint64_t txId;
    pthread_threadid_np(tx_thread, &txId);
    printf("Tx Thread (id %lld): START\n", txId);
}

void TxModule::stopRoutine() {
    pthread_mutex_lock(&tx_mutex);
    active = false;
    pthread_cond_signal(&tx_cond);
    pthread_mutex_unlock(&tx_mutex);
    __uint64_t txId;
    pthread_threadid_np(tx_thread, &txId);
    if (txId > 0) {
        pthread_join(tx_thread, NULL);
        printf("Tx Thread (id %lld): STOP\n", txId);
    }
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
int TxModule::enqueuePacket(S3TP_PACKET * packet, int frag_no, bool more_fragments, int spi_channel) {
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

    //i8 crc = calc_checksum(packet->pdu, packet->hdr.pdu_length);
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

