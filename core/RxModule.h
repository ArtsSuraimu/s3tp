//
// Created by Lorenzo Donini on 02/09/16.
//

#ifndef S3TP_RXMODULE_H
#define S3TP_RXMODULE_H

#include "Buffer.h"
#include "constants.h"
#include "utilities.h"
#include <cstring>
#include <map>
#include <trctrl/LinkCallback.h>

#define PORT_ALREADY_OPEN -1
#define PORT_OPENED 0
#define PORT_ALREADY_CLOSED -1
#define PORT_CLOSED 0
#define MODULE_INACTIVE -2
#define CODE_ERROR_CRC_INVALID -3

#define MAX_REORDERING_WINDOW 256

class RxModule: public Transceiver::LinkCallback {
public:
    RxModule();
    ~RxModule();

    void stopModule();
    int openPort(uint8_t port);
    int closePort(uint8_t port);
    bool isActive();
    bool isNewMessageAvailable();
    S3TP_PACKET_WRAPPER * consumeNextAvailableMessage();
private:
    bool active;
    Buffer inBuffer;
    uint8_t global_seq_num;
    uint64_t received_packets;
    pthread_mutex_t rx_mutex;
    pthread_cond_t available_msg_cond;

    std::map<uint8_t, uint8_t> current_port_sequence;

    // LinkCallback
    void handleFrame(bool arq, int channel, const void* data, int length);
    int handleReceivedPacket(S3TP_PACKET * packet, uint8_t channel);
    void handleLinkStatus(bool linkStatus);
    bool isPortOpen(uint8_t port);
    //void consumeQueue(uint8_t port);
};


#endif //S3TP_RXMODULE_H
