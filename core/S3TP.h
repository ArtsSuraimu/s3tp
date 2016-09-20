//
// Created by Lorenzo Donini on 25/08/16.
//

#ifndef S3TP_S3TP_MAIN_H
#define S3TP_S3TP_MAIN_H

#include "TxModule.h"
#include "RxModule.h"
#include "SimpleQueue.h"
#include "ClientInterface.h"
#include "StatusInterface.h"
#include "Client.h"
#include <cstring>
#include <moveio/PinMapper.h>
#include <trctrl/BackendFactory.h>
#include <string>
#include <vector>
#include <set>

#define CODE_SUCCESS 0
#define CODE_ERROR_MAX_MESSAGE_SIZE -2
#define CODE_INTERNAL_ERROR -3
#define CODE_QUEUE_FULL -4
#define CODE_LINK_UNAVAIABLE -5
#define CODE_CHANNEL_BROKEN -6

enum TRANSCEIVER_TYPE {
    SPI,
    FIRE
};

typedef struct transceiver_factory_config {
    TRANSCEIVER_TYPE type;
    std::vector<Transceiver::FireTcpPair> mappings;
    Transceiver::SPIDescriptor descriptor;
}TRANSCEIVER_CONFIG;

class S3TP: public ClientInterface,
                 public StatusInterface {
public:
    S3TP();
    ~S3TP();
    int init(TRANSCEIVER_CONFIG * config);
    int stop();
    int sendToLinkLayer(uint8_t channel, uint8_t port, void * data, size_t len, uint8_t opts);
    Client * getClientConnectedToPort(uint8_t port);
    void cleanupClients();

private:
    pthread_t assembly_thread;
    pthread_cond_t assembly_cond;
    pthread_mutex_t s3tp_mutex;
    bool active;
    bool syncScheduled;
    Transceiver::Backend * transceiver;

    //Generic methods
    void reset();
    void synchronizeStatus();

    //TxModule
    TxModule tx;
    int fragmentPayload(uint8_t channel, uint8_t port, void * data, size_t len, uint8_t opts);
    int sendSimplePayload(uint8_t channel, uint8_t port, void * data, size_t len, uint8_t opts);
    //RxModule
    RxModule rx;
    void assemblyRoutine();
    static void * staticAssemblyRoutine(void * args);

    //Clients
    std::map<uint8_t, Client*> clients;
    pthread_mutex_t clients_mutex;
    std::set<uint8_t> channel_blacklist;
    std::vector<uint8_t> disconnectedClients;
    int checkTransmissionAvailability(uint8_t port, uint8_t channel, uint16_t msg_len);
    virtual void onDisconnected(void * params);
    virtual void onConnected(void * params);
    virtual int onApplicationMessage(void * data, size_t len, void * params);

    //Status check
    virtual void onLinkStatusChanged(bool active);
    virtual void onError(int error, void * params);
    virtual void onSynchronization();
};


#endif //S3TP_S3TP_MAIN_H
