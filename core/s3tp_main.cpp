//
// Created by Lorenzo Donini on 25/08/16.
//

#include "s3tp_main.h"

s3tp_main::s3tp_main() {
}

s3tp_main::~s3tp_main() {
    pthread_mutex_lock(&s3tp_mutex);
    pthread_mutex_unlock(&s3tp_mutex);
    pthread_mutex_destroy(&s3tp_mutex);
}

int s3tp_main::init() {
    pthread_mutex_init(&s3tp_mutex, NULL);
    pthread_mutex_lock(&s3tp_mutex);
    active = true;
    //TODO: create/load spi interface
    tx.startRoutine(NULL);
    /*pthread_cond_init(&frag_cond, NULL);
    pthread_create(&fragmentation_thread, NULL, &s3tp_main::staticFragmentationRoutine, this);
    __uint64_t id;
    pthread_threadid_np(fragmentation_thread, &id);
    printf("Fragmentation Thread (id %lld): START\n", id);*/
    //TODO: implement assembly thread as well
    pthread_mutex_unlock(&s3tp_mutex);

    return CODE_SUCCESS;
}

int s3tp_main::stop() {
    //Deactivating module
    pthread_mutex_lock(&s3tp_mutex);
    active = false;
    //pthread_join(fragmentation_thread, NULL);
    //Quit the Tx thread
    tx.stopRoutine();
    pthread_mutex_unlock(&s3tp_mutex);

    return CODE_SUCCESS;
}

int s3tp_main::send(u8 channel, u8 port, void * data, size_t len) {
    /* As messages should still be sent out sequentially,
     * we're putting all of them into the fragmentation queue.
     * Fragmentation thread will then be in charge of checking
     * whether the message needs fragmentation or not */
    pthread_mutex_lock(&s3tp_mutex);
    bool isActive = active;
    pthread_mutex_unlock(&s3tp_mutex);
    if (isActive) {
        if (len > MAX_PDU_LENGTH) {
            //Payload exceeds maximum length: drop packet and return error
            return CODE_ERROR_MAX_MESSAGE_SIZE;
        } else if (len > LEN_S3TP_PDU) {
            //Packet needs fragmentation
            return fragmentPayload(channel, port, data, len);
        } else {
            //Payload fits into one packet
            return sendSimplePayload(channel, port, data, len);
        }
    }
    return CODE_INTERNAL_ERROR;
}

int s3tp_main::sendSimplePayload(u8 channel, u8 port, void * data, size_t len) {
    S3TP_PACKET * packet;
    int status = 0;

    //Send to Tx Module without fragmenting
    packet = new S3TP_PACKET();
    memcpy(packet->pdu, data, len);
    packet->hdr.port = port;
    packet->hdr.pdu_length = (u16)len;
    pthread_mutex_lock(&s3tp_mutex);
    status = tx.enqueuePacket(packet, 0, false, channel);
    pthread_mutex_unlock(&s3tp_mutex);

    return status;
}

int s3tp_main::fragmentPayload(u8 channel, u8 port, void * data, size_t len) {
    S3TP_PACKET * packet;

    //Need to fragment
    int written = 0;
    int status = 0;
    int fragment = 0;
    bool moreFragments = true;
    u8 * dataPtr = (u8 *) data;
    while (written < len) {
        packet = new S3TP_PACKET();
        packet->hdr.port = port;
        if (written + LEN_S3TP_PDU > len) {
            //We are at the last packet, so don't need to write max payload
            memcpy(packet->pdu, dataPtr, len - written);
            packet->hdr.pdu_length = (u16) (len - written);
        } else {
            //Filling packet payload with max permitted payload
            memcpy(packet->pdu, dataPtr, LEN_S3TP_PDU);
            packet->hdr.pdu_length = (u16) LEN_S3TP_PDU;
        }
        written += packet->hdr.pdu_length;
        if (written >= len) {
            moreFragments = false;
        }
        //Send to Tx Module
        pthread_mutex_lock(&s3tp_mutex);
        //TODO: make enqueue an atomic operation
        status = tx.enqueuePacket(packet, fragment, moreFragments, channel);
        pthread_mutex_unlock(&s3tp_mutex);
        if (status != CODE_SUCCESS) {
            return status;
        }
        //Increasing current fragment number for next iteration
        fragment++;
    }
    return written;
}

/*
 * Assembly Thread logic
 */
void s3tp_main::assemblyRoutine() {
    //TODO: implement routine
}

void * s3tp_main::staticAssemblyRoutine(void * args) {
    static_cast<s3tp_main*>(args)->assemblyRoutine();
    return NULL;
}