//
// Created by Lorenzo Donini on 31/08/16.
//

#include "client.h"

client::client(SOCKET socket, S3TP_CONFIG config, s3tp_main * s3tp, connection_listener * listener) {
    this->socket = socket;
    this->app_port = config.port;
    this->s3tp = s3tp;
    this->virtual_channel = config.channel;
    this->options = config.options;
    this->listener = listener;
    this->connected = true;
    pthread_mutex_init(&client_mutex, NULL);
    pthread_create(&client_thread, NULL, staticClientRoutine, this);
}

bool client::isConnected() {
    pthread_mutex_lock(&client_mutex);
    bool result = connected;
    pthread_mutex_unlock(&client_mutex);
    return result;
}

void client::closeConnection() {
    pthread_mutex_lock(&client_mutex);
    if (connected) {
        close(socket);
        printf("Closed socket %d\n", socket);
    }
    connected = false;
    pthread_mutex_unlock(&client_mutex);
    //Notifying s3tp that a port is available again
    if (listener != NULL) {
        listener->onDisconnected(&app_port);
    }
}

/**
 * During communication, socket was closed from other side.
 * We simply perform the necessary operations, then notify listeners that the connection was closed.
 */
void client::handleConnectionClosed() {
    pthread_mutex_lock(&client_mutex);
    connected = false;
    pthread_mutex_unlock(&client_mutex);
    //Notifying s3tp that a port is available again
    if (listener != NULL) {
        listener->onDisconnected(&app_port);
    }
}

void client::kill() {
    if (!isConnected()) {
        //Just waiting for thread to finish (if not finished already)
        pthread_join(client_thread, NULL);
        return;
    }
    closeConnection();
    //Kill thread and wait for it to finish
    pthread_cancel(client_thread);
    pthread_join(client_thread, NULL);
}

int client::send(const void * data, size_t len) {
    return 0;
}

void client::clientRoutine() {
    ssize_t i = 0, rd = 0, wr = 0;
    size_t len = 0;
    int error = 0;

    printf("Started client thread\n");
    while (isConnected()) {
        error = read_length_safe(socket, &len);
        if (error == CODE_ERROR_SOCKET_NO_CONN) {
            printf("Client %d closed socket\n", socket);
            handleConnectionClosed();
            break;
        } else if (error < 0) {
            printf("Error reading from client %d\n", socket);
            closeConnection();
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
                //EOF read
                printf("Client %d closed socket\n", socket);
                closeConnection();
                delete [] message;
                //Quitting thread
                break;
            } else if (i < 0) {
                printf("Error reading payload\n");
                closeConnection();
                delete [] message;
                //Quitting thread
                break;
            }
            rd += i;
            currentPosition += i;
        } while(rd < len);

        //Disconnected during payload transmission -> Exit while loop
        if (!isConnected()) {
            break;
        }

        //Payload received entirely
        printf("client %d on port %d sent data <%s>\n", socket, app_port, message);
        if (s3tp == NULL) {
            printf("S3TP module is not connected. Aborting client %d routine\n", socket);
            closeConnection();
            break;
        }
        //Forward data to s3tp module
        int result = s3tp->send(virtual_channel, app_port, message, len);
        if (result < 0) {
            printf("Error while communicating with s3tp module %d\n", socket);
            //TODO: kill connection?!
        } else {
            //Dummy echo mechanism
            sleep(1);
            error = write_length_safe(socket, len);
            if (error == CODE_ERROR_SOCKET_NO_CONN) {
                printf("Connection was closed by peer\n");
                handleConnectionClosed();
                break;
            } else if (error == CODE_ERROR_SOCKET_WRITE) {
                printf("Error while writing on socket\n");
                closeConnection();
                break;
            }
            wr = write(socket, message, len);
            if (wr == 0) {
                printf("Error while writing echo response\n");
                handleConnectionClosed();
                break;
            } else if (wr < 0) {
                printf("Error while writing echo response\n");
                closeConnection();
                break;
            }
        }
    }

    pthread_exit(NULL);
}

void * client::staticClientRoutine(void * args) {
    static_cast<client *>(args)->clientRoutine();
    return NULL;
}