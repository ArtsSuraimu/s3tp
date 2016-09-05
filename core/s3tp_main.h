//
// Created by Lorenzo Donini on 25/08/16.
//

#ifndef S3TP_S3TP_MAIN_H
#define S3TP_S3TP_MAIN_H

#include "TxModule.h"
#include "RxModule.h"
#include "SimpleQueue.h"

#define CODE_SUCCESS 0
#define CODE_ERROR_MAX_MESSAGE_SIZE -2
#define CODE_INTERNAL_ERROR -3

class s3tp_main {
public:
    s3tp_main();
    ~s3tp_main();
    int init();
    int stop();
    int send(uint8_t channel, uint8_t port, void * data, size_t len);

private:
    pthread_t assembly_thread;
    pthread_cond_t assembly_cond;
    pthread_mutex_t s3tp_mutex;

    bool active;

    //TxModule
    TxModule tx;
    int fragmentPayload(uint8_t channel, uint8_t port, void * data, size_t len);
    int sendSimplePayload(uint8_t channel, uint8_t port, void * data, size_t len);
    //RxModule
    RxModule rx;
    void assemblyRoutine();
    static void * staticAssemblyRoutine(void * args);
};


#endif //S3TP_S3TP_MAIN_H
