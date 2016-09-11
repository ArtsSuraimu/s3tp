//
// Created by Lorenzo Donini on 30/08/16.
//

#include "S3tpConnector.h"

S3tpConnector::S3tpConnector() {
    connected = false;
    pthread_mutex_init(&connector_mutex, NULL);
}

bool S3tpConnector::isConnected() {
    pthread_mutex_lock(&connector_mutex);
    bool result = connected;
    pthread_mutex_unlock(&connector_mutex);
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
    pthread_mutex_lock(&connector_mutex);
    connected = true;
    pthread_mutex_unlock(&connector_mutex);
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
        std::ostringstream os;
        os << "Cannot use S3TP on port " << config.port << " because it is currently busy";
        LOG_WARN(os.str());

        closeConnection();
        return CODE_SERVER_PORT_BUSY;
    }

    //Starting asynchronous routine only if callback was set
    if (callback != NULL) {
        pthread_create(&listener_thread, NULL, staticAsyncListener, this);
    }

    return CODE_SUCCESS;
}

int S3tpConnector::send(const void * data, size_t len) {
    ssize_t wr;
    int error = 0;

    if (!isConnected()) {
        LOG_ERROR("Trying to write on closed channel. Sutting down");

        pthread_mutex_unlock(&connector_mutex);
        return CODE_ERROR_SOCKET_NO_CONN;
    }

    error = write_length_safe(socketDescriptor, len);
    if (error == CODE_ERROR_SOCKET_WRITE) {
        LOG_WARN("Error while writing on S3TP socket");

        return error;
    }

    wr = write(socketDescriptor, data, len);
    if (wr <= 0) {
        LOG_WARN("Error while reading from S3TP socket");

        return CODE_ERROR_SOCKET_WRITE;
    }
    std::ostringstream os;
    os << "Written " << wr << " bytes to S3TP";
    LOG_DEBUG(os.str());

    return (int)wr;
}

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
        pthread_mutex_lock(&connector_mutex);
        connected = false;
        pthread_mutex_unlock(&connector_mutex);
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
            pthread_mutex_lock(&connector_mutex);
            connected = false;
            pthread_mutex_unlock(&connector_mutex);
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
        pthread_mutex_lock(&connector_mutex);
        connected = false;
        pthread_mutex_unlock(&connector_mutex);
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
            pthread_mutex_lock(&connector_mutex);
            connected = false;
            pthread_mutex_unlock(&connector_mutex);
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
    pthread_mutex_lock(&connector_mutex);
    if (connected) {
        close(socketDescriptor);

        std::ostringstream os;
        os << "Closed connection (socket " << socketDescriptor << ")";
        LOG_INFO(os.str());
    }
    connected = false;
    pthread_mutex_unlock(&connector_mutex);
    //Not waiting for the client_if thread to die for now
}

/*
 * Asynchronous thread routine
 */
void S3tpConnector::asyncListener() {
    int err = 0;
    ssize_t i, rd;
    size_t len;
    std::ostringstream os;

    LOG_DEBUG("Started Client async listener thread");
    pthread_mutex_lock(&connector_mutex);
    while (connected) {
        pthread_mutex_unlock(&connector_mutex);
        err = read_length_safe(socketDescriptor, &len);
        if (err == CODE_ERROR_LENGTH_CORRUPT) {
            LOG_ERROR("Corrupt data received while connecting to S3TP daemon. Shutting down");

            closeConnection();
            break;
        } else if (err == CODE_ERROR_SOCKET_NO_CONN) {
            //Socket was already closed
            LOG_WARN("Connection to S3TP was closed by server");

            pthread_mutex_lock(&connector_mutex);
            connected = false;
            break;
        } else if (err < 0) {
            LOG_ERROR("Unknown error occurred during read phase. Shutting down");

            closeConnection();
            break;
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
                pthread_mutex_lock(&connector_mutex);
                connected = false;
                pthread_mutex_unlock(&connector_mutex);
                delete [] message;

                LOG_WARN("Connection to S3TP was closed by server");

                return;
            } else if (i < 0) {

                LOG_WARN("Error while reading from S3TP socket");
                closeConnection();
                delete [] message;
                return;
            }
            rd += i;
            currentPosition += i;
        } while(rd < len);

        //Payload received entirely
        message[len] = '\0';

        os.str("");
        os << "Received data (" << len << " bytes) on port " << config.port << ": <";
        os << message << ">";
        LOG_DEBUG(os.str());
        callback->onNewMessage(message, len);

        pthread_mutex_lock(&connector_mutex);
    }
    pthread_mutex_unlock(&connector_mutex);
}

void * S3tpConnector::staticAsyncListener(void * args) {
    static_cast<S3tpConnector *>(args)->asyncListener();
    pthread_exit(NULL);
    return NULL;
}
