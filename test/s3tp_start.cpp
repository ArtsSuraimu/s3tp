//
// Created by lorenzodonini on 06.09.16.
//

#include "../core/TransportDaemon.h"

int main(int argc, char ** argv) {
    if (argc != 4) {
        std::cout << "Invalid arguments. Expected unix_path, transceiver_type, start_prt" << std::endl;
        return -1;
    }
    s3tp_daemon daemon;

    TRANSCEIVER_CONFIG config;

    int argi = 1;
    int start_port = 0;

    socket_path = argv[argi++];

    //Creating spi interface
    char * transceiverType = argv[argi++];
    start_port = atoi(argv[argi++]);

    if (strcmp(transceiverType, "spi") == 0) {
        config.type = SPI;
        Transceiver::SPIDescriptor desc;
        config.descriptor.spi = "/dev/spidev1.1#P8_46";
        config.descriptor.interrupt = PinMapper::find("P8_45");
    } else if (strcmp(transceiverType, "fire") == 0) {
        config.type = FIRE;
        Transceiver::FireTcpPair pairs[S3TP_VIRTUAL_CHANNELS];
        std::cout << "Created config for [port:channel]: ";
        for (int i=0; i < S3TP_VIRTUAL_CHANNELS; i++) {
            pairs[i].port = start_port + (i*2);
            pairs[i].channel = i;
            config.mappings.push_back(pairs[i]);
            std::cout << start_port + (i*2) << ":" << i << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "Invalid parameters" << std::endl;
        return -2;
    }

    daemon.init(&config);
    daemon.startDaemon();
}
