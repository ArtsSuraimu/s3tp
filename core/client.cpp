//
// Created by Lorenzo Donini on 31/08/16.
//

#include "client.h"

client::client(SOCKET socket, S3TP_CONFIG config, s3tp_main * main) {
    this->socket = socket;
    this->app_port = config.port;
    this->main = main;
    this->virtual_channel = config.channel;
    this->options = config.options;
    this->connected = true;
    pthread_create(&client_thread, NULL, staticClientRoutine, this);
}

void client::kill() {
    if (!connected) {
        pthread_join(client_thread, NULL);
        return;
    }
    pthread_cancel(client_thread);
    connected = false;
    pthread_join(client_thread, NULL);
}

void client::clientRoutine() {
    ssize_t i = 0, rd = 0;
    size_t len = 0;
    int error = 0;

    printf("Started client thread\n");
    while (true) {
        error = read_length_safe(socket, &len);
        if (error == CODE_ERROR_SOCKET_NO_CONN) {
            printf("Client %d closed socket\n", socket);
            connected = false;
            break;
        } else if (error < 0) {
            printf("Error reading from client %d\n", socket);
            close(socket);
            connected = false;
            break;
        }

        //Length of next message received
        char * message = new char[len];
        char * currentPosition = message;

        //Read payload
        rd = 0;
        do {
            i = read(socket, currentPosition, (len - rd));
            if (i == 0) {
                printf("Client %d closed socket\n", socket);
                connected = false;
                delete [] message;
                break;
            }
            if (i <= 0) {
                printf("Error reading payload\n");
                delete [] message;
                break;
            }
            rd += i;
            currentPosition += i;
        } while(rd < len);
        //Payload received entirely
        printf("client %d on port %d sent data <%s>\n", socket, app_port, message);
        //int result = main->send(virtual_channel, app_port, message, len);
        //TODO: check result

        sleep(1);
        //Dummy echo mechanism
        write_length_safe(socket, len);
        write(socket, message, len);
    }
    printf("Closed socket %d\n", socket);
    close(socket);

    pthread_exit(NULL);
}

void * client::staticClientRoutine(void * args) {
    static_cast<client *>(args)->clientRoutine();
    return NULL;
}