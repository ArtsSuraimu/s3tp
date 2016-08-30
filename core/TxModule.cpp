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
    pthread_mutex_destroy(&tx_mutex);
    printf("Destroyed Tx Module\n");
}

//Private methods
void TxModule::txRoutine() {
    pthread_mutex_init(&tx_mutex, NULL);
    pthread_mutex_lock(&tx_mutex);
    while(active) {
        while(!outBuffer.packetsAvailable()) {
            state = WAITING;
            pthread_cond_wait(&tx_cond, &tx_mutex);
        }
        state = RUNNING;
        S3TP_PACKET_WRAPPER * wrapper = outBuffer.getNextAvailablePacket();
        pthread_mutex_unlock(&tx_mutex);
        if (wrapper == NULL) {
            continue;
        }
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
    printf("Started Tx Thread\n");
}

void TxModule::stopRoutine() {
    pthread_mutex_lock(&tx_mutex);
    active = false;
    pthread_cond_signal(&tx_cond);
    pthread_mutex_unlock(&tx_mutex);
    pthread_join(tx_thread, NULL);
    printf("Stopped Tx Thread\n");
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

    u8 global_seq = global_seq_num;
    u8 sub_seq = (u8) frag_no;
    packet->hdr.seq = (u16)((global_seq << 8) | (sub_seq & 0xFF));
    if (more_fragments) {
        packet->hdr.setMoreFragments();
    } else {
        packet->hdr.unsetMoreFragments();
        //Need to increase the current global sequence
        global_seq_num++;
    }
    //Increasing port sequence
    packet->hdr.seq_port = port_sequence[packet->hdr.port]++;
    //TODO: compute CRC
    S3TP_PACKET_WRAPPER * wrapper = new S3TP_PACKET_WRAPPER();
    wrapper->pkt = packet;
    wrapper->channel = (u8) spi_channel;
    outBuffer.write(wrapper);
    pthread_cond_signal(&tx_cond);

    pthread_mutex_unlock(&tx_mutex);

    return 0;
}

