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

/**
 * During communication, a socket error was encountered.
 * We forcefully close the socket, then notify listeners that the connection was closed.
 */
void Client::closeConnection() {
    pthread_mutex_lock(&client_mutex);
    if (connected) {
        shutdown(socket, SHUT_RDWR);
        close(socket);

        LOG_DEBUG(std::string("Closed socket " + std::to_string(socket)));
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
    if (connected) {
        close(socket);

        LOG_DEBUG(std::string("Closed socket " + std::to_string(socket)));
    }
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
    ssize_t wr;
    AppMessageType type = APP_DATA_MESSAGE;

    //Sending message type first
    wr = write(socket, &type, sizeof(type));
    if (wr == CODE_ERROR_SOCKET_NO_CONN) {
        LOG_WARN(std::string("Connection was closed by s3tp client " + std::to_string(socket)));
        handleConnectionClosed();
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        LOG_WARN(std::string("Error while writing on socket " + std::to_string(socket)));
        closeConnection();
        return CODE_ERROR_SOCKET_WRITE;
    }
    //Sending length of message
    int error = write_length_safe(socket, len);
    if (error == CODE_ERROR_SOCKET_NO_CONN) {
        LOG_WARN(std::string("Connection was closed by s3tp client " + std::to_string(socket)));
        handleConnectionClosed();
        return error;
    } else if (error == CODE_ERROR_SOCKET_WRITE) {
        LOG_WARN(std::string("Error while writing on socket " + std::to_string(socket)));
        closeConnection();
        return error;
    }
    //Sending message content
    wr = write(socket, data, len);
    if (wr == 0) {
        LOG_WARN(std::string("Connection was closed by s3tp client " + std::to_string(socket)));
        handleConnectionClosed();
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        LOG_WARN(std::string("Error while writing data on socket " + std::to_string(socket)));
        closeConnection();
        return CODE_ERROR_SOCKET_WRITE;
    }
    return CODE_SUCCESS;
}

int Client::sendControlMessage(S3TP_CONNECTOR_CONTROL message) {
    ssize_t wr;
    AppMessageType msgType = APP_CONTROL_MESSAGE;

    //Sending message type first
    wr = write(socket, &msgType, sizeof(msgType));
    if (wr == CODE_ERROR_SOCKET_NO_CONN) {
        LOG_WARN(std::string("Connection was closed by s3tp client " + std::to_string(socket)));
        handleConnectionClosed();
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        LOG_WARN(std::string("Error while writing on socket " + std::to_string(socket)));
        closeConnection();
        return CODE_ERROR_SOCKET_WRITE;
    }

    //Sending control message
    wr = write(socket, &message, sizeof(S3TP_CONNECTOR_CONTROL));
    if (wr == 0) {
        LOG_WARN(std::string("Connection was closed by s3tp client " + std::to_string(socket)));
        handleConnectionClosed();
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        LOG_WARN(std::string("Error while writing data on socket " + std::to_string(socket)));
        closeConnection();
        return CODE_ERROR_SOCKET_WRITE;
    }
    return CODE_SUCCESS;
}

void Client::clientRoutine() {
    ssize_t i = 0, rd = 0;
    size_t len = 0;
    int error = 0;
    AppMessageType type;
    S3TP_CONNECTOR_CONTROL control;

    LOG_DEBUG(std::string("Started client thread for socket " + std::to_string(socket)));
    while (isConnected()) {
        //Checking message type first
        rd = read(socket, &type, sizeof(type));
        if (rd <= 0) {
            LOG_INFO(std::string("Client closed socket " + std::to_string(socket)));
            handleConnectionClosed();
            break;
        }
        //TODO: handle logic for reading control messages, maybe not needed
        error = read_length_safe(socket, &len);
        if (error == CODE_ERROR_SOCKET_NO_CONN) {
            LOG_INFO(std::string("Client closed socket " + std::to_string(socket)));
            handleConnectionClosed();
            break;
        } else if (error < 0) {
            LOG_WARN(std::string("Error while reading from client on socket " + std::to_string(socket)));
            closeConnection();
            break;
        }

        //Length of next message received
        char * message = new char[len+1];
        message[len] = 0;

        char * currentPosition = message;

        //Read payload
        rd = 0;
        do {
            i = read(socket, currentPosition, (len - rd));
            if (i == 0) {
                //EOF read
                LOG_WARN(std::string("Client closed socket " + std::to_string(socket)));
                closeConnection();
                delete [] message;
                //Quitting thread
                break;
            } else if (i < 0) {
                LOG_WARN(std::string("Error while reading message from client on socket " + std::to_string(socket)));
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
        LOG_DEBUG(std::string("Received "
                              + std::to_string(len)
                              + " bytes from port "
                              + std::to_string((int)app_port)
                              + ": <" + message + ">"));
        if (client_if == NULL) {
            LOG_WARN(std::string("Client interface is not connected. Aborting client "
                                 + std::to_string(socket) + " routine"));
            closeConnection();
            break;
        }
        //Forward data to s3tp module (through Client interface callback)
        int result = client_if->onApplicationMessage(message, len, this);
        //s3tp protocol copies contents of message, so we need to free this temp buffer
        delete[] message;

        if (result != CODE_SUCCESS) {
            LOG_INFO(std::string("Cannot transmit message to port " + std::to_string((int)app_port)
                                 + ". Error code: " + std::to_string(result)));
            control.controlMessageType = NACK;
            control.error = (S3tpError) result;
        } else {
            LOG_DEBUG(std::string("Sending ack to port " + std::to_string((int)app_port)));
            control.controlMessageType = ACK;
            control.error = 0;
        }
        sendControlMessage(control);

        if (result < 0) {
            LOG_ERROR(std::string("Error while communicating with s3tp module " + std::to_string(socket)));
            //TODO: kill connection?!
        }
    }

    pthread_exit(NULL);
}

void * Client::staticClientRoutine(void * args) {
    static_cast<Client *>(args)->clientRoutine();
    return NULL;
}