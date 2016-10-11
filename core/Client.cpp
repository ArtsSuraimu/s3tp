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
    this->bound = true;
    this->listening = false;
    this->connected = false;
    this->connecting = false;
    pthread_mutex_init(&client_mutex, NULL);
    pthread_create(&client_thread, NULL, staticClientRoutine, this);
    //Notify listener that Client is now connected
    client_if->onApplicationConnected(this);
}

bool Client::isBound() {
    pthread_mutex_lock(&client_mutex);
    bool result = bound;
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
 * We forcefully close/unbind the domain socket, then notify listeners that the connection was closed.
 * This operation does not affect the logical connection to the remote host,
 * hence that needs to be handled separately.
 */
void Client::unbind() {
    pthread_mutex_lock(&client_mutex);
    if (bound) {
        shutdown(socket, SHUT_RDWR);
        close(socket);

        LOG_DEBUG(std::string("Closed socket " + std::to_string(socket)));
    }
    bound = false;
    pthread_mutex_unlock(&client_mutex);
    //Notifying s3tp that a port is available again
    if (client_if != NULL) {
        client_if->onApplicationDisconnected(this);
    }
}

/**
 * During communication, domain socket was closed/unbound from the application.
 * We simply perform the necessary operations, then notify listeners that the connection was closed.
 * This operation may affect the logical connection to the remote host,
 * hence that needs to be handled separately.
 */
void Client::handleUnbind() {
    pthread_mutex_lock(&client_mutex);
    if (bound) {
        close(socket);

        LOG_DEBUG(std::string("Closed socket " + std::to_string(socket)));
    }
    bound = false;
    pthread_mutex_unlock(&client_mutex);
    //Notifying s3tp that a port is available again
    if (client_if != NULL) {
        client_if->onApplicationDisconnected(this);
    }
}

/**
 * Attempts a connection to the remote host. The underlying protocol will perform a 3-way handshake.
 * If the connection can be established, the client will be allowed to transmit data.
 */
void Client::tryConnect() {
    pthread_mutex_lock(&client_mutex);
    connecting = true;
    if (client_if != NULL) {
        client_if->onConnectToHost();
    }
    pthread_mutex_unlock(&client_mutex);
}

/**
 * Disconnects from the remote host. The disconnection is initiated by the application.
 * No acknowledgement messages to the client are required for this type of operation,
 * as it will be carried out seamlessly by s3tp.
 */
void Client::disconnect() {
    pthread_mutex_lock(&client_mutex);
    connected = false;
    if (client_if != NULL) {
        client_if->onDisconnectFromHost();
    }
    pthread_mutex_unlock(&client_mutex);
}

/**
 * Tells the client object to be ready to listen to incoming connections from the remote host.
 * If not listening, any incoming connections from the remote host will be dropped.
 */
void Client::listen() {
    pthread_mutex_lock(&client_mutex);
    listening = true;
    pthread_mutex_unlock(&client_mutex);
}

void Client::kill() {
    if (!isBound()) {
        //Just waiting for thread to finish (if not finished already)
        pthread_join(client_thread, NULL);
        return;
    }
    //Kill thread and wait for it to finish
    unbind();
    pthread_join(client_thread, NULL);
}

int Client::send(const void * data, size_t len) {
    ssize_t wr;
    AppMessageType type = APP_DATA_MESSAGE;

    //Sending message type first
    wr = write(socket, &type, sizeof(type));
    if (wr == CODE_ERROR_SOCKET_NO_CONN) {
        LOG_WARN(std::string("Connection was closed by s3tp client " + std::to_string(socket)));
        handleUnbind();
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        LOG_WARN(std::string("Error while writing on socket " + std::to_string(socket)));
        unbind();
        return CODE_ERROR_SOCKET_WRITE;
    }
    //Sending length of message
    int error = write_length_safe(socket, len);
    if (error == CODE_ERROR_SOCKET_NO_CONN) {
        LOG_WARN(std::string("Connection was closed by s3tp client " + std::to_string(socket)));
        handleUnbind();
        return error;
    } else if (error == CODE_ERROR_SOCKET_WRITE) {
        LOG_WARN(std::string("Error while writing on socket " + std::to_string(socket)));
        unbind();
        return error;
    }
    //Sending message content
    wr = write(socket, data, len);
    if (wr == 0) {
        LOG_WARN(std::string("Connection was closed by s3tp client " + std::to_string(socket)));
        handleUnbind();
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        LOG_WARN(std::string("Error while writing data on socket " + std::to_string(socket)));
        unbind();
        return CODE_ERROR_SOCKET_WRITE;
    }
    return CODE_SUCCESS;
}

int Client::sendControlMessage(S3TP_CONNECTOR_CONTROL message) {
    ssize_t wr;
    AppMessageType msgType = APP_CONTROL_MESSAGE;

    //TODO: proper locking
    //Sending message type first
    wr = write(socket, &msgType, sizeof(msgType));
    if (wr == CODE_ERROR_SOCKET_NO_CONN) {
        LOG_WARN(std::string("Connection was closed by s3tp client " + std::to_string(socket)));
        handleUnbind();
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        LOG_WARN(std::string("Error while writing on socket " + std::to_string(socket)));
        unbind();
        return CODE_ERROR_SOCKET_WRITE;
    }

    //Sending control message
    wr = write(socket, &message, sizeof(S3TP_CONNECTOR_CONTROL));
    if (wr == 0) {
        LOG_WARN(std::string("Connection was closed by s3tp client " + std::to_string(socket)));
        handleUnbind();
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        LOG_WARN(std::string("Error while writing data on socket " + std::to_string(socket)));
        unbind();
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

    //TODO: do proper locking
    LOG_DEBUG(std::string("Started client thread for socket " + std::to_string(socket)));
    while (isBound()) {
        //Checking message type first
        rd = read(socket, &type, sizeof(type));
        if (rd <= 0) {
            LOG_INFO(std::string("Client closed socket " + std::to_string(socket)));
            handleUnbind();
            break;
        }
        if (type == APP_CONTROL_MESSAGE) {
            handleControlMessage();
            //TODO: handle errors appropriately
            continue;
        }
        error = read_length_safe(socket, &len);
        if (error == CODE_ERROR_SOCKET_NO_CONN) {
            LOG_INFO(std::string("Client closed socket " + std::to_string(socket)));
            handleUnbind();
            break;
        } else if (error < 0) {
            LOG_WARN(std::string("Error while reading from client on socket " + std::to_string(socket)));
            unbind();
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
                unbind();
                delete [] message;
                //Quitting thread
                break;
            } else if (i < 0) {
                LOG_WARN(std::string("Error while reading message from client on socket " + std::to_string(socket)));
                unbind();
                delete [] message;
                //Quitting thread
                break;
            }
            rd += i;
            currentPosition += i;
        } while(rd < len);

        //Disconnected during payload transmission -> Exit while loop
        if (!isBound()) {
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
            unbind();
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

int Client::handleControlMessage() {
    ssize_t rd = 0;
    S3TP_CONNECTOR_CONTROL msg;

    rd = read(socket, &msg, sizeof(S3TP_CONNECTOR_CONTROL));
    if (rd <= 0) {
        LOG_INFO(std::string("Client closed socket " + std::to_string(socket)));
        handleUnbind();
    } else {
        switch (msg.controlMessageType) {
            case AppControlMessageType::LISTEN:
                listen();
                break;
            case AppControlMessageType::CONN_UP:
                tryConnect();
                break;
            case AppControlMessageType::CONN_DOWN:
                disconnect();
                break;
            default:
                //TODO: handle. There shouldn't be any other cases
                break;

        }
    }
    return (int)rd;
}

void * Client::staticClientRoutine(void * args) {
    static_cast<Client *>(args)->clientRoutine();
    return NULL;
}

/**
 * Accepting a remote connection request. Can only be accepted if the local host
 * is currently listening on the given port (i.e., if an application is currently
 * connected to the s3tp daemon for this port).
 *
 * @return  Returns true if the logical connection was established, false otherwise.
 */
bool Client::acceptConnect() {
    bool conn;

    pthread_mutex_lock(&client_mutex);
    if (!listening) {
        pthread_mutex_unlock(&client_mutex);
        return false;
    }
    pthread_mutex_unlock(&client_mutex);
    S3TP_CONNECTOR_CONTROL msg;
    msg.error = 0;
    msg.controlMessageType = AppControlMessageType::CONN_UP;

    conn = sendControlMessage(msg) >= 0;
    pthread_mutex_lock(&client_mutex);
    connecting = false;
    connected = conn;
    pthread_mutex_unlock(&client_mutex);
    return conn;
}

/**
 * Notifies the client that a connection request has failed,
 * hence no new connection to the remote host was established.
 */
void Client::failedConnect() {
    pthread_mutex_lock(&client_mutex);
    if (!listening) {
        pthread_mutex_unlock(&client_mutex);
        return;
    }
    connecting = false;
    connected = false;
    pthread_mutex_unlock(&client_mutex);
    S3TP_CONNECTOR_CONTROL msg;
    msg.error = 0;
    msg.controlMessageType = AppControlMessageType::CONN_DOWN;
    sendControlMessage(msg);
}

/**
 * Checks the current connection status on the given application port.
 *
 * @return  Returns true if a logical connection with the remote host is currently active, false otherwise.
 */
bool Client::isConnected() {
    pthread_mutex_lock(&client_mutex);
    bool result = connected;
    pthread_mutex_unlock(&client_mutex);
    return result;
}

/**
 * Checks whether the application is currently waiting for incoming connections.
 *
 * @return  Returns true if the application is listening on the given port, false otherwise.
 */
bool Client::isListening() {
    pthread_mutex_lock(&client_mutex);
    bool result = listening;
    pthread_mutex_unlock(&client_mutex);
    return result;
}

/**
 * Notifies the client/application that a logical connection was closed by the remote host.
 */
void Client::closeConnection() {
    pthread_mutex_lock(&client_mutex);
    if (!connected) {
        pthread_mutex_unlock(&client_mutex);
        return;
    }
    connected = false;
    pthread_mutex_unlock(&client_mutex);
    S3TP_CONNECTOR_CONTROL msg;
    msg.error = 0;
    msg.controlMessageType = AppControlMessageType::CONN_DOWN;
    sendControlMessage(msg);
}