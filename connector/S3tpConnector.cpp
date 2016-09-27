//
// Created by Lorenzo Donini on 30/08/16.
//

#include "S3tpConnector.h"

S3tpConnector::S3tpConnector() {
    lastMessageAck = false;
    s3tpBufferFull = false;
    connected = false;
}

S3tpConnector::~S3tpConnector() {
    closeConnection();

    if (listener_thread.joinable()) {
        listener_thread.join();
    }
}

bool S3tpConnector::isConnected() {
    connector_mutex.lock();
    bool result = connected;
    connector_mutex.unlock();
    return result;
}

int S3tpConnector::init(S3TP_CONFIG config, S3tpCallback * callback) {
    struct sockaddr_un addr;
    ssize_t wr, rd;
    int commCode;

    this->config = config;
    this->callback = callback;

    if ((socketDescriptor = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return CODE_ERROR_SOCKET_CREATE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socket_path);

    if (connect(socketDescriptor, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        return CODE_ERROR_SOCKET_CONNECT;
    }
    connector_mutex.lock();
    connected = true;
    connector_mutex.unlock();
    LOG_INFO("Connected to S3TP Daemon successfully");

    //Sending configuration over to server
    wr = write(socketDescriptor, &config, sizeof(S3TP_CONFIG));
    if (wr == 0) {
        LOG_WARN("Connection to S3TP was closed by server");
        connected = false;
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        LOG_ERROR("Unknown error occurred while sending configuration to server");
        closeConnection();
        return CODE_ERROR_SOCKET_CONFIG;
    }

    LOG_DEBUG("Configuration sent to S3TP server");
    rd = read(socketDescriptor, &commCode, sizeof(int));
    if (rd == 0) {
        LOG_WARN("Connection to S3TP was closed by server");
        connected = false;
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (rd < 0) {
        LOG_ERROR("Couldn't receive acknowledgement from server during configuration. Shutting down");
        closeConnection();
        return CODE_ERROR_SOCKET_CONFIG;
    }

    if (commCode == CODE_SERVER_PORT_BUSY) {
        LOG_WARN(std::string("Cannot use S3TP on port " + std::to_string((int)config.port)
                             + " because it is currently busy"));

        closeConnection();
        return CODE_SERVER_PORT_BUSY;
    }

    //Starting asynchronous routine only if callback was set
    if (callback != NULL) {
        listener_thread = std::thread(&S3tpConnector::asyncListener, this);
    }

    return CODE_SUCCESS;
}

int S3tpConnector::send(const void * data, size_t len) {
    ssize_t wr;
    int error = 0;
    AppMessageType type = APP_DATA_MESSAGE;

    if (!isConnected()) {
        LOG_ERROR("Trying to write on closed channel. Sutting down");

        return CODE_ERROR_SOCKET_NO_CONN;
    }

    std::unique_lock<std::mutex> lock(connector_mutex);
    do {
        //Send message type first
        wr = write(socketDescriptor, &type, sizeof(type));
        if (wr <= 0) {
            LOG_WARN("Error while writing to S3TP socket");

            return CODE_ERROR_SOCKET_WRITE;
        }

        //Send message lengths
        error = write_length_safe(socketDescriptor, len);
        if (error == CODE_ERROR_SOCKET_WRITE) {
            LOG_WARN("Error while writing on S3TP socket");

            return error;
        }

        wr = write(socketDescriptor, data, len);
        if (wr <= 0) {
            LOG_WARN("Error while writing to S3TP socket");

            return CODE_ERROR_SOCKET_WRITE;
        }

        LOG_DEBUG(std::string("Written " + std::to_string(wr) + " bytes to S3TP"));

        //Waiting for asynchronous response
        status_cond.wait(lock);
        if (!connected) {
            LOG_DEBUG("Discnnected from S3TP");
            return CODE_ERROR_SOCKET_NO_CONN;
        }

        if (lastMessageAck && !s3tpBufferFull) {
            //ACK was received correctly
            break;
        }
        LOG_DEBUG("Received NACK from S3TP. Queue is currently full. Waiting till queue can be written again");
        status_cond.wait(lock);
    } while(connected);

    LOG_DEBUG("Received ACK from S3TP");

    return (int)wr;
}

//TODO: update recv logic!!
int S3tpConnector::recv(void * buffer, size_t len) {
    int error = 0;
    size_t msg_len;
    ssize_t rd, i;

    if (!isConnected()) {
        LOG_ERROR("Trying to read from a closed channel. Shutting down");

        return CODE_ERROR_SOCKET_NO_CONN;
    }

    error = read_length_safe(socketDescriptor, &msg_len);
    if (error == CODE_ERROR_SOCKET_NO_CONN) {
        connector_mutex.lock();
        connected = false;
        connector_mutex.unlock();
        LOG_WARN("Connection to S3TP was closed by server");
        return error;
    } else if (error < 0) {
        closeConnection();
        return error;
    }

    if (msg_len > len) {
        LOG_WARN("Recv error: provided buffer cannot hold message");
        closeConnection();
        return CODE_ERROR_INVALID_LENGTH;
    }
    len = MIN(len, msg_len);

    char * currentPosition = (char *)buffer;
    rd = 0;
    do {
        i = read(socketDescriptor, currentPosition, (len - rd));
        if (i == 0) {
            connector_mutex.lock();
            connected = false;
            connector_mutex.unlock();
            LOG_WARN("Connection to S3TP was closed by server");
            return CODE_ERROR_SOCKET_NO_CONN;
        } else if (i < 0) {
            LOG_WARN("Error while reading from S3TP socket");
            closeConnection();
            return CODE_ERROR_SOCKET_READ;
        }
        rd += i;
        currentPosition += i;
    } while (rd < len);

    return (int)rd;
}

//TODO: update recvRaw logic
char * S3tpConnector::recvRaw(size_t * len, int * error) {
    size_t msg_len = 0;
    ssize_t rd, i;

    if (!isConnected()) {
        LOG_WARN("Trying to read from a closed channel. No connection to S3TP available");
        *len = 0;
        *error = CODE_ERROR_SOCKET_NO_CONN;
        return NULL;
    }

    *error = read_length_safe(socketDescriptor, &msg_len);
    if (*error == CODE_ERROR_SOCKET_NO_CONN) {
        connector_mutex.lock();
        connected = false;
        connector_mutex.unlock();
        *len = 0;
        LOG_WARN("Connection to S3TP was closed by server");
        return NULL;
    } else if (*error < 0) {
        *len = 0;
        closeConnection();
        return NULL;
    }

    char * msg = new char[*len];
    char * currentPosition = msg;
    //Read payload
    rd = 0;
    do {
        i = read(socketDescriptor, currentPosition, (*len - rd));
        if (i == 0) {
            *error = CODE_ERROR_SOCKET_NO_CONN;
            connector_mutex.lock();
            connected = false;
            connector_mutex.unlock();
            *len = 0;
            LOG_WARN("Connection to S3TP was closed by server");
            delete [] msg;
            return NULL;
        } else if (i < 0) {
            *error = CODE_ERROR_SOCKET_READ;
            delete [] msg;
            LOG_WARN("Error while reading from S3TP socket");
            closeConnection();
            return NULL;
        }
        rd += i;
        currentPosition += i;
    } while(rd < *len);

    return msg;
}

void S3tpConnector::closeConnection() {
    connector_mutex.lock();
    if (connected) {
        shutdown(socketDescriptor, SHUT_RDWR);
        close(socketDescriptor);

        LOG_INFO(std::string("Closed connector (socket " + std::to_string(socketDescriptor) + ")"));
    }
    connected = false;
    status_cond.notify_all();
    connector_mutex.unlock();
}

/*
 * Asynchronous thread routine
 */
void S3tpConnector::asyncListener() {
    ssize_t rd;
    AppMessageType type;
    S3TP_CONTROL control;

    LOG_DEBUG("Started Client async listener thread");

    while (isConnected()) {
        rd = read(socketDescriptor, &type, sizeof(AppMessageType));
        if (rd == 0) {
            //Socket was already closed
            LOG_WARN("Connection to S3TP was closed by server");

            closeConnection();
            break;
        } else if (rd < 0) {
            LOG_ERROR("Unknown error occurred during read phase. Shutting down");

            closeConnection();
            break;
        }
        type = safeMessageTypeInterpretation(type);
        if (type == APP_CONTROL_MESSAGE) {
            if (!receiveControlMessage(control)) {
                break;
            }
        } else if (type == APP_DATA_MESSAGE) {
            if (!receiveDataMessage()) {
                break;
            }
        }
    }
    LOG_DEBUG("Listener thread: STOP");
}

bool S3tpConnector::receiveControlMessage(S3TP_CONTROL& control) {
    ssize_t rd;

    rd = read(socketDescriptor, &control, sizeof(S3TP_CONTROL));

    //Handle socket errors
    if (rd == 0) {
        connector_mutex.lock();
        connected = false;
        connector_mutex.unlock();
        LOG_WARN("Connection to S3TP was closed by server");
        return false;
    } else if (rd < 0) {
        closeConnection();
        return false;
    }

    //Now checking the actual contents of the control message
    AppControlMessageType type = safeMessageTypeInterpretation(control.controlMessageType);
    connector_mutex.lock();
    switch (type) {
        case ACK:
            lastMessageAck = true;
            break;
        case NACK:
            lastMessageAck = false;
            //The type of error is not relevant, as we just cannot write the message
            s3tpBufferFull = true;
            break;
        case AVAILABLE:
            s3tpBufferFull = false;
            break;
        default:
            break;
    }
    status_cond.notify_all();
    connector_mutex.unlock();
    return true;
}

bool S3tpConnector::receiveDataMessage() {
    int err = 0;
    ssize_t i, rd;
    size_t len;

    err = read_length_safe(socketDescriptor, &len);
    if (err == CODE_ERROR_LENGTH_CORRUPT) {
        LOG_ERROR("Corrupt data received while connecting to S3TP daemon. Shutting down");

        closeConnection();
        return false;
    } else if (err == CODE_ERROR_SOCKET_NO_CONN) {
        //Socket was already closed
        LOG_WARN("Connection to S3TP was closed by server");

        connector_mutex.lock();
        connected = false;
        connector_mutex.unlock();
        return false;
    } else if (err < 0) {
        LOG_ERROR("Unknown error occurred during read phase. Shutting down");

        closeConnection();
        return false;
    }
    //Length of next message received
    char * message = new char[len+1];
    char * currentPosition = message;

    //Read payload
    rd = 0;
    do {
        i = read(socketDescriptor, currentPosition, (len - rd));
        //Checking errors
        if (i == 0) {
            connector_mutex.lock();
            connected = false;
            connector_mutex.unlock();
            delete [] message;

            LOG_WARN("Connection to S3TP was closed by server");

            return false;
        } else if (i < 0) {

            LOG_WARN("Error while reading from S3TP socket");
            closeConnection();
            delete [] message;
            return false;
        }
        rd += i;
        currentPosition += i;
    } while(rd < len);

    //Payload received entirely
    message[len] = '\0';

    LOG_DEBUG(std::string("Received data (" + std::to_string(len)
                          + " bytes) on port " + std::to_string((int)config.port)
                          + ": <" + message + ">"));
    callback->onNewMessage(message, len);

    return true;
}
