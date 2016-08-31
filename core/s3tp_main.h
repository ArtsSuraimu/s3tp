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
    pthread_t fragmentation_thread;
    pthread_t assembly_thread;
    pthread_mutex_t s3tp_mutex;

    bool active;

    SimpleQueue<RawData> * fragmentation_q;
    pthread_cond_t frag_cond;
    //TxModule
    TxModule tx;
    void fragmentationRoutine();
    static void * staticFragmentationRoutine(void * args);
    //RxModule
    void assemblyRoutine();
    static void * staticAssemblyRoutine(void * args);
};


#endif //S3TP_S3TP_MAIN_H
