//
// Created by Lorenzo Donini on 25/08/16.
//

#ifndef S3TP_S3TP_MAIN_H
#define S3TP_S3TP_MAIN_H

#include "TxModule.h"
#include "RxModule.h"
#include "SimpleQueue.h"
#include "ClientInterface.h"
#include "Client.h"
#include <cstring>
#include <moveio/PinMapper.h>
#include <trctrl/BackendFactory.h>

#define CODE_SUCCESS 0
#define CODE_ERROR_MAX_MESSAGE_SIZE -2
#define CODE_INTERNAL_ERROR -3

class s3tp_main: public ClientInterface {
public:
    s3tp_main();
    ~s3tp_main();
    int init();
    int stop();
    int sendToLinkLayer(uint8_t channel, uint8_t port, void * data, size_t len);
    Client * getClientConnectedToPort(uint8_t port);

private:
    pthread_t assembly_thread;
    pthread_cond_t assembly_cond;
    pthread_mutex_t s3tp_mutex;
    bool active;
    Transceiver::Backend * transceiver;

    //TxModule
    TxModule tx;
    int fragmentPayload(uint8_t channel, uint8_t port, void * data, size_t len);
    int sendSimplePayload(uint8_t channel, uint8_t port, void * data, size_t len);
    //RxModule
    RxModule rx;
    void assemblyRoutine();
    static void * staticAssemblyRoutine(void * args);

    //Clients
    std::map<uint8_t, Client*> clients;
    pthread_mutex_t clients_mutex;
    virtual void onDisconnected(void * params);
    virtual void onConnected(void * params);
    virtual int onApplicationMessage(uint8_t channel, uint8_t port, void * data, size_t len);
};


#endif //S3TP_S3TP_MAIN_H
