//
// Created by lorenzodonini on 06.09.16.
//

#include "../core/TransportDaemon.h"

int main(int argc, char ** argv) {
    if (argc != 3) {
        printf("Invalid arguments\n");
        return -1;
    }
    s3tp_daemon daemon;

    TRANSCEIVER_CONFIG config;

    int argi = 1;

    socket_path = argv[argi++];

    //Creating spi interface
    char * transceiverType = argv[argi++];
    if (strcmp(transceiverType, "spi") == 0) {
        config.type = SPI;
        Transceiver::SPIDescriptor desc;
        config.descriptor.spi = "/dev/spidev1.1#P8_46";
        config.descriptor.interrupt = PinMapper::find("P8_45");
    } else if (strcmp(transceiverType, "fire") == 0) {
        config.type = FIRE;
        Transceiver::FireTcpPair pair;
        printf ("Enter fire port: ");
        scanf("%d", &pair.port);
        pair.channel = 3;
        config.mappings.push_back(pair);
    } else {
        printf("Invalid parameters\n");
        return -2;
    }

    daemon.init(&config);
    daemon.startDaemon();
}
