//
// Created by Lorenzo Donini on 23/08/16.
//

#ifndef S3TP_TXMODULE_H
#define S3TP_TXMODULE_H

#include "constants.h"
#include "Buffer.h"
#include <map>

class TxModule {
public:
    enum STATE {
        RUNNING,
        BLOCKED,
        WAITING
    };

    STATE getCurrentState();
    static TxModule* Instance();
    void startRoutine(Buffer * buffer);
    void stopRoutine();

protected:
    ~TxModule();

private:
    static TxModule * instance;

    STATE state;
    bool active;
    pthread_t tx_thread;
    pthread_mutex_t tx_mutex;
    void * spi_interface;

    Buffer * outBuffer;

    TxModule();

    void txRoutine();
    static void * staticTxRoutine(void * args);
};

#endif //S3TP_TXMODULE_H
