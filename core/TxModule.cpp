//
// Created by Lorenzo Donini on 23/08/16.
//

#include "TxModule.h"

TxModule* TxModule::instance = NULL;

//Ctor
TxModule::TxModule() {
    state = WAITING;
    pthread_mutex_init(&tx_mutex, NULL);
    printf("Created Tx Module\n");
}

//Dtor
TxModule::~TxModule() {
    pthread_mutex_lock(&tx_mutex);
    pthread_mutex_destroy(&tx_mutex);
    printf("Destroyed Tx Module\n");
}

//Private methods
void TxModule::txRoutine() {
    pthread_mutex_init(&tx_mutex, NULL);
    pthread_mutex_lock(&tx_mutex);
    while(active) {
        while(!outBuffer->packetsAvailable()) {
            state = WAITING;
            pthread_cond_wait(&outBuffer->new_content_cond, &tx_mutex);
        }
        state = RUNNING;
        S3TP_PACKET * packet = outBuffer->getNextAvailablePacket();
        if (packet == NULL) {
            continue;
        }
        printf("Consumed packet %d from port %d\n", packet->hdr.seq, packet->hdr.port);
        //TODO: send packet to SPI interface
        delete packet;
        /*if (spi_interface == NULL) {
            state = BLOCKED;
        }*/
    }
    pthread_mutex_unlock(&tx_mutex);

    pthread_exit(NULL);
}

void * TxModule::staticTxRoutine(void * args) {
    static_cast<TxModule*>(args)->txRoutine();
    return NULL;
}

//Public methods
void TxModule::startRoutine(Buffer * buffer) {
    pthread_mutex_lock(&tx_mutex);
    outBuffer = buffer;
    active = true;
    pthread_create(&tx_thread, NULL, &TxModule::staticTxRoutine, this);
    pthread_mutex_unlock(&tx_mutex);
    printf("Started Tx Module\n");
}

void TxModule::stopRoutine() {
    pthread_mutex_lock(&tx_mutex);
    active = false;
    pthread_mutex_unlock(&tx_mutex);
    printf("Stopped Tx Module\n");
}

TxModule::STATE TxModule::getCurrentState() {
    pthread_mutex_lock(&tx_mutex);
    STATE current = state;
    pthread_mutex_unlock(&tx_mutex);
    return current;
}

TxModule * TxModule::Instance() {
    if (instance == NULL) {
        instance = new TxModule();
    }
    return instance;
}

