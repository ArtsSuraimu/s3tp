//
// Created by lorenzodonini on 06.09.16.
//

#include "../core/s3tp_daemon.h"

int main(int argc, char ** argv) {
    if (argc != 1) {
        printf("Invalid arguments\n");
        return -1;
    }
    s3tp_daemon daemon;

    TRANSCEIVER_CONFIG config;

    //Creating spi interface
    char * transceiverType = argv[0];
    if (strcmp(transceiverType, "spi") == 0) {
        config.type = SPI;
        Transceiver::SPIDescriptor desc;
        config.descriptor.spi = "/dev/spidev1.1#P8_46";
        config.descriptor.interrupt = PinMapper::find("P8_45");
    } else if (strcmp(transceiverType, "fire") == 0) {
        config.type = FIRE;
        Transceiver::FireTcpPair pair;
        pair.port = 2000;
        pair.channel = 0;
        config.mappings.push_back(pair);
    }

    daemon.init(&config);
    daemon.startDaemon();
}
