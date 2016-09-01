//
// Created by Lorenzo Donini on 25/08/16.
//

#ifndef S3TP_S3TP_MAIN_H
#define S3TP_S3TP_MAIN_H

#include "TxModule.h"
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
    int send(u8 channel, u8 port, void * data, size_t len);

private:
    pthread_t assembly_thread;
    pthread_mutex_t s3tp_mutex;

    bool active;

    pthread_cond_t assembly_cond;
    //TxModule
    TxModule tx;
    int fragmentPayload(u8 channel, u8 port, void * data, size_t len);
    int sendSimplePayload(u8 channel, u8 port, void * data, size_t len);
    //RxModule
    void assemblyRoutine();
    static void * staticAssemblyRoutine(void * args);
};


#endif //S3TP_S3TP_MAIN_H
