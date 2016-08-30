//
// Created by Lorenzo Donini on 25/08/16.
//

#include "s3tp_main.h"

s3tp_main::s3tp_main() {
    fragmentation_q = new SimpleQueue<RawData>(SIMPLE_QUEUE_DEFAULT_CAPACITY, SIMPLE_QUEUE_BLOCKING);
}

s3tp_main::~s3tp_main() {
    pthread_mutex_lock(&s3tp_mutex);
    delete fragmentation_q;
    pthread_mutex_unlock(&s3tp_mutex);
    pthread_mutex_destroy(&s3tp_mutex);
}

int s3tp_main::init() {
    pthread_mutex_init(&s3tp_mutex, NULL);
    pthread_mutex_lock(&s3tp_mutex);
    active = true;
    pthread_cond_init(&frag_cond, NULL);
    pthread_create(&fragmentation_thread, NULL, &s3tp_main::staticFragmentationRoutine, this);
    printf("Fragmentation Thread: START\n");
    //TODO: implement assembly thread as well
    pthread_mutex_unlock(&s3tp_mutex);

    return CODE_SUCCESS;
}

int s3tp_main::stop() {
    //Destroying fragmentation thread first
    pthread_mutex_lock(&s3tp_mutex);
    active = false;
    pthread_cond_signal(&frag_cond); //Notifying fragmentation thread, in case it's idly waiting for something
    pthread_mutex_unlock(&s3tp_mutex);
    pthread_join(fragmentation_thread, NULL);
    //Quit the Tx thread
    pthread_mutex_lock(&s3tp_mutex);
    tx.stopRoutine();
    pthread_mutex_unlock(&s3tp_mutex);

    return CODE_SUCCESS;
}

bool s3tp_main::isActive() {
    pthread_mutex_lock(&s3tp_mutex);
    bool result = active;
    pthread_mutex_unlock(&s3tp_mutex);
    return result;
}

int s3tp_main::send(u8 channel, u8 port, void * data, size_t len) {
    /* As messages should still be sent out sequentially,
     * we're putting all of them into the fragmentation queue.
     * Fragmentation thread will then be in charge of checking
     * whether the message needs fragmentation or not */
    if (isActive()) {
        if (len > DEFAULT_MAX_PDU_LENGTH) {
            //Drop packet and return error
            return CODE_ERROR_MAX_MESSAGE_SIZE;
        } else {
            //Put into fragmentation queue
            int result = fragmentation_q->push(RawData(data, len, port, channel));
            pthread_cond_signal(&frag_cond);
            return result;
        }
    }
    return CODE_INTERNAL_ERROR;
}

/*
 * Fragmentation Thread logic
 */
void s3tp_main::fragmentationRoutine() {
    S3TP_PACKET * packet;

    pthread_mutex_lock(&s3tp_mutex);
    while (active) {
        if (fragmentation_q->isEmpty()) {
            pthread_cond_wait(&frag_cond, &s3tp_mutex);
            continue;
        }
        pthread_mutex_unlock(&s3tp_mutex);
        RawData data = fragmentation_q->pop();
        printf("Fragmentation Thread: processing next message (%d bytes) for port %d\n", (int)data.len, data.port);
        if (data.len > LEN_S3TP_PDU) {
            //Need to fragment
            int written = 0;
            int fragment = 0;
            bool moreFragments = true;
            u8 * dataPtr = (u8 *) data.data;
            while (written < data.len) {
                packet = new S3TP_PACKET();
                packet->hdr.port = data.port;
                if (written + LEN_S3TP_PDU > data.len) {
                    //We are at the last packet, so don't need to write max payload
                    memcpy(packet->pdu, dataPtr, data.len - written);
                    packet->hdr.pdu_length = (u16) (data.len - written);
                } else {
                    //Filling packet payload with max permitted payload
                    memcpy(packet->pdu, dataPtr, LEN_S3TP_PDU);
                    packet->hdr.pdu_length = (u16) LEN_S3TP_PDU;
                }
                written += packet->hdr.pdu_length;
                if (written >= data.len) {
                    moreFragments = false;
                }
                //Send to Tx Module
                pthread_mutex_lock(&s3tp_mutex);
                tx.enqueuePacket(packet, fragment, moreFragments, data.channel);
                pthread_mutex_unlock(&s3tp_mutex);
                //Increasing current fragment number for next iteration
                fragment++;
            }
        } else {
            //Send to Tx Module without fragmenting
            packet = new S3TP_PACKET();
            memcpy(packet->pdu, data.data, data.len);
            packet->hdr.port = data.port;
            packet->hdr.pdu_length = (u16)data.len;
            pthread_mutex_lock(&s3tp_mutex);
            tx.enqueuePacket(packet, 0, false, data.channel);
            pthread_mutex_unlock(&s3tp_mutex);
        }
        pthread_mutex_lock(&s3tp_mutex);
    }
    pthread_mutex_unlock(&s3tp_mutex);
    printf("Fragmentation Thread: STOP\n");
    pthread_exit(NULL);
}

void * s3tp_main::staticFragmentationRoutine(void * args) {
    static_cast<s3tp_main*>(args)->fragmentationRoutine();
    return NULL;
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