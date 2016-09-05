//
// Created by Lorenzo Donini on 30/08/16.
//

#include "s3tp_connector.h"

s3tp_connector::s3tp_connector() {
    connected = false;
    pthread_mutex_init(&connector_mutex, NULL);
}

bool s3tp_connector::isConnected() {
    pthread_mutex_lock(&connector_mutex);
    bool result = connected;
    pthread_mutex_unlock(&connector_mutex);
    return result;
}

int s3tp_connector::init(S3TP_CONFIG config, S3TP_CALLBACK callback) {
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
    printf("client: Connected to server successfully!\n");

    //Sending configuration over to server
    wr = write(socketDescriptor, &config, sizeof(S3TP_CONFIG));
    if (wr == 0) {
        printf("Socket was closed by server\n");
        connected = false;
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (wr < 0) {
        printf("Error while sending configuration to server\n");
        closeConnection();
        return CODE_ERROR_SOCKET_CONFIG;
    }

    printf("client: Configuration sent to server\n");
    rd = read(socketDescriptor, &commCode, sizeof(int));
    if (rd == 0) {
        printf("Socket was closed by server\n");
        connected = false;
        return CODE_ERROR_SOCKET_NO_CONN;
    } else if (rd < 0) {
        printf("Error while receiving ack from server\n");
        closeConnection();
        return CODE_ERROR_SOCKET_CONFIG;
    }

    if (commCode == CODE_SERVER_PORT_BUSY) {
        printf("Cannot connect to server on port %d, because it is currently busy\n", config.port);
        closeConnection();
        return CODE_SERVER_PORT_BUSY;
    }

    //Starting asynchronous routine only if callback was set
    if (callback != NULL) {
        pthread_create(&listener_thread, NULL, staticAsyncListener, this);
    }

    return CODE_SUCCESS;
}

int s3tp_connector::send(const void * data, size_t len) {
    ssize_t wr;
    int error = 0;

    if (!isConnected()) {
        printf("client: Trying to write on closed channel\n");
        pthread_mutex_unlock(&connector_mutex);
        return CODE_ERROR_SOCKET_NO_CONN;
    }

    error = write_length_safe(socketDescriptor, len);
    if (error == CODE_ERROR_SOCKET_WRITE) {
        printf("client: error while writing on socket\n");
        return error;
    }

    wr = write(socketDescriptor, data, len);
    if (wr <= 0) {
        printf("client: error while writing on socket\n");
        return CODE_ERROR_SOCKET_WRITE;
    }
    printf("client: Written %ld bytes\n", wr);

    return (int)wr;
}

int s3tp_connector::recv(void * buffer, size_t len) {
    int error = 0;
    size_t msg_len;
    ssize_t rd, i;

    if (!isConnected()) {
        printf("client: Trying to read from a closed channel\n");
        return CODE_ERROR_SOCKET_NO_CONN;
    }

    error = read_length_safe(socketDescriptor, &msg_len);
    if (error == CODE_ERROR_SOCKET_NO_CONN) {
        pthread_mutex_lock(&connector_mutex);
        connected = false;
        pthread_mutex_unlock(&connector_mutex);
        printf("client: connection was closed by server\n");
        return error;
    } else if (error < 0) {
        closeConnection();
        return error;
    }

    if (msg_len > len) {
        printf("client: provided buffer cannot hold message\n");
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
            printf("client: connection was closed by server\n");
            return CODE_ERROR_SOCKET_NO_CONN;
        } else if (i < 0) {
            printf("client: error while reading from socket\n");
            closeConnection();
            return CODE_ERROR_SOCKET_READ;
        }
        rd += i;
        currentPosition += i;
    } while (rd < len);

    return (int)rd;
}

char * s3tp_connector::recvRaw(size_t * len, int * error) {
    size_t msg_len = 0;
    ssize_t rd, i;

    if (!isConnected()) {
        printf("client: Trying to read from a closed channel\n");
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
        printf("client: connection was closed by server\n");
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
            printf("client: connection was closed by server\n");
            delete [] msg;
            return NULL;
        } else if (i < 0) {
            *error = CODE_ERROR_SOCKET_READ;
            delete [] msg;
            printf("client: error while reading from socket\n");
            closeConnection();
            return NULL;
        }
        rd += i;
        currentPosition += i;
    } while(rd < *len);

    return msg;
}

void s3tp_connector::closeConnection() {
    pthread_mutex_lock(&connector_mutex);
    if (connected) {
        close(socketDescriptor);
        printf("client: closed connection %d\n", socketDescriptor);
    }
    connected = false;
    pthread_mutex_unlock(&connector_mutex);
    //Not waiting for the listener thread to die for now
}

/*
 * Asynchronous thread routine
 */
void s3tp_connector::asyncListener() {
    int err = 0;
    ssize_t i, rd;
    size_t len;

    printf("client: Started client thread\n");
    pthread_mutex_lock(&connector_mutex);
    while (connected) {
        pthread_mutex_unlock(&connector_mutex);
        err = read_length_safe(socketDescriptor, &len);
        if (err == CODE_ERROR_LENGTH_CORRUPT) {
            printf("client: Corrupt data received, dropping connection\n");
            closeConnection();
            break;
        } else if (err < 0) {
            printf("client: error during read phase. Shutting down connection\n");
            closeConnection();
            break;
        } else if (err == CODE_ERROR_SOCKET_NO_CONN) {
            //Socket was already closed
            pthread_mutex_lock(&connector_mutex);
            connected = false;
            break;
        }
        //Length of next message received
        char * message = new char[len];
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
                printf("client: connection was closed by server\n");
                delete [] message;
                return;
            } else if (i < 0) {
                printf("client: error while reading from socket\n");
                closeConnection();
                delete [] message;
                return;
            }
            rd += i;
            currentPosition += i;
        } while(rd < len);

        //Payload received entirely
        printf("client %d on port %d: received data <%s>\n", socketDescriptor, config.port, message);
        callback(message, len);

        pthread_mutex_lock(&connector_mutex);
    }
    pthread_mutex_unlock(&connector_mutex);

    pthread_exit(NULL);
}

void * s3tp_connector::staticAsyncListener(void * args) {
    static_cast<s3tp_connector *>(args)->asyncListener();
    return NULL;
}
