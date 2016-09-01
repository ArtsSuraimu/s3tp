//
// Created by Lorenzo Donini on 23/08/16.
//

#ifndef S3TP_TXMODULE_H
#define S3TP_TXMODULE_H

#include "constants.h"
#include "Buffer.h"
#include "utilities.h"
#include <map>

#define TX_PARAM_RECOVERY 0x01
#define TX_PARAM_CUSTOM 0x02
#define CODE_INACTIVE_ERROR -1

class TxModule {
public:
    enum STATE {
        RUNNING,
        BLOCKED,
        WAITING
    };

    TxModule();
    ~TxModule();

    STATE getCurrentState();
    void startRoutine(void * spi_if);
    void stopRoutine();
    int enqueuePacket(S3TP_PACKET * packet, int frag_no, bool more_fragments, int spi_channel);

private:
    STATE state;
    bool active;
    pthread_t tx_thread;
    pthread_mutex_t tx_mutex;
    pthread_cond_t tx_cond;
    uint8_t global_seq_num;

    std::map<uint8_t, uint8_t> port_sequence;
    Buffer outBuffer;
    void * spi_interface;

    void txRoutine();
    static void * staticTxRoutine(void * args);
};

#endif //S3TP_TXMODULE_H
