//
// Created by Lorenzo Donini on 31/08/16.
//

#include "Client.h"

Client::Client(SOCKET socket, S3TP_CONFIG config, ClientInterface * listener) {
    this->socket = socket;
    this->app_port = config.port;
    this->virtual_channel = config.channel;
    this->options = config.options;
    this->client_if = listener;
    this->connected = true;
    pthread_mutex_init(&client_mutex, NULL);
    pthread_create(&client_thread, NULL, staticClientRoutine, this);
    //Notify listener that Client is now connected
    client_if->onConnected(this);
}

bool Client::isConnected() {
    pthread_mutex_lock(&client_mutex);
    bool result = connected;
    pthread_mutex_unlock(&client_mutex);
    return result;
}

uint8_t Client::getAppPort() {
    return app_port;
}

uint8_t Client::getVirtualChannel() {
    return virtual_channel;
}

uint8_t Client::getOptions() {
    return options;
}

void Client::closeConnection() {
    pthread_mutex_lock(&client_mutex);
    if (connected) {
        close(socket);
        printf("Closed socket %d\n", socket);
    }
    connected = false;
    pthread_mutex_unlock(&client_mutex);
    //Notifying s3tp that a port is available again
    if (client_if != NULL) {
        client_if->onDisconnected(this);
    }
}

/**
 * During communication, socket was closed from other side.
 * We simply perform the necessary operations, then notify listeners that the connection was closed.
 */
void Client::handleConnectionClosed() {
    pthread_mutex_lock(&client_mutex);
    connected = false;
    pthread_mutex_unlock(&client_mutex);
    //Notifying s3tp that a port is available again
    if (client_if != NULL) {
        client_if->onDisconnected(this);
    }
}

void Client::kill() {
    if (!isConnected()) {
        //Just waiting for thread to finish (if not finished already)
        pthread_join(client_thread, NULL);
        return;
    }
    //Kill thread and wait for it to finish
    closeConnection();
    pthread_join(client_thread, NULL);
}

int Client::send(const void * data, size_t len) {
    int error = write_length_safe(socket, len);
    if (error == CODE_ERROR_SOCKET_NO_CONN) {
        printf("Connection was closed by peer\n");
        handleConnectionClosed();
        return error;
    } else if (error == CODE_ERROR_SOCKET_WRITE) {
        printf("Error while writing on socket\n");
        closeConnection();
        return error;
    }
    ssize_t wr = write(socket, data, len);
    if (wr == 0) {
        printf("Error while writing echo response\n");
        handleConnectionClosed();
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        printf("Error while writing echo response\n");
        closeConnection();
        return CODE_ERROR_SOCKET_WRITE;
    }
    return CODE_SUCCESS;
}

void Client::clientRoutine() {
    ssize_t i = 0, rd = 0;
    size_t len = 0;
    int error = 0;

    printf("Started Client thread\n");
    while (isConnected()) {
        error = read_length_safe(socket, &len);
        if (error == CODE_ERROR_SOCKET_NO_CONN) {
            printf("Client %d closed socket\n", socket);
            handleConnectionClosed();
            break;
        } else if (error < 0) {
            printf("Error reading from Client %d\n", socket);
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
        printf("Client %d on port %d sent data <%s>\n", socket, app_port, message);
        if (client_if == NULL) {
            printf("Client interface is not connected connected. Aborting Client %d routine\n", socket);
            closeConnection();
            break;
        }
        //Forward data to s3tp module (through Client interface callback)
        int result = client_if->onApplicationMessage(message, len, this);
        if (result < 0) {
            printf("Error while communicating with s3tp module %d\n", socket);
            //TODO: kill connection?!
        } else {
            //Dummy echo mechanism
            /*sleep(1);
            error = send(message, len);
            if (error != CODE_SUCCESS) {
                break;
            }*/
        }
    }

    pthread_exit(NULL);
}

void * Client::staticClientRoutine(void * args) {
    static_cast<Client *>(args)->clientRoutine();
    return NULL;
}