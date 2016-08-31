//
// Created by Lorenzo Donini on 30/08/16.
//

#include <string>
#include <iostream>
#include "s3tp_connector.h"

const char * socket_path = "/tmp/testing";

s3tp_connector::s3tp_connector() {
    connected = false;
    pthread_mutex_init(&connector_mutex, NULL);
}

int s3tp_connector::init(S3TP_CONFIG config, S3TP_CALLBACK callback) {
    struct sockaddr_un addr;

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
    connected = true;
    printf("Connected to server successfully!\n");

    //Sending configuration over to server
    if (write(socketDescriptor, &config, sizeof(S3TP_CONFIG)) <= 0) {
        printf("Error while sending configuration to server\n");
        return CODE_ERROR_SOCKET_CONFIG;
    }
    printf("Configuration sent to server\n");

    //Starting asynchronous routine only if callback was set
    if (callback != NULL) {
        pthread_create(&listener_thread, NULL, staticAsyncListener, this);
    }

    return CODE_SUCCESS;
}

int s3tp_connector::send(const void * data, size_t len) {
    ssize_t wr;

    pthread_mutex_lock(&connector_mutex);
    if (!connected) {
        printf("Trying to write on closed channel");
        pthread_mutex_unlock(&connector_mutex);
        return CODE_ERROR_SOCKET_NO_CONN;
    }
    pthread_mutex_unlock(&connector_mutex);

    wr = write(socketDescriptor, data, len);
    printf("Written %ld bytes\n", wr);

    return (int)wr;
}

void s3tp_connector::closeConnection() {
    pthread_mutex_lock(&connector_mutex);
    if (connected) {
        close(socketDescriptor);
        connected = false;
    }
    pthread_mutex_unlock(&connector_mutex);
    //Not waiting for the listener thread to die for now
}

/*
 * Asynchronous thread routine
 */
void s3tp_connector::asyncListener() {
    ssize_t i, rd, err;
    int len;

    printf("Started client thread\n");
    pthread_mutex_lock(&connector_mutex);
    while (connected) {
        pthread_mutex_unlock(&connector_mutex);
        i = read(socketDescriptor, &len, sizeof(int));
        if (i <= 0) {
            printf("Error reading from socket\n");
            pthread_mutex_lock(&connector_mutex);
            connected = false;
            break;
        }
        err = 0;
        //Length of next message received
        char * message = new char[len];

        //Read payload
        rd = 0;
        do {
            i = read(socketDescriptor, message, ((size_t)len - rd));
            if (i <= 0) {
                printf("Error reading payload\n");
                err = i;
                break;
            }
            rd += i;
        } while(rd < len);

        if (err != 0) {
            pthread_mutex_lock(&connector_mutex);
            continue;
        }

        //Payload received entirely
        printf("Client %d on port %d: received data <%s>\n", socketDescriptor, config.port, message);
        callback(message, len);

        pthread_mutex_lock(&connector_mutex);
    }
    pthread_mutex_unlock(&connector_mutex);
    printf("Closed socket %d\n", socketDescriptor);

    pthread_exit(NULL);
}

void * s3tp_connector::staticAsyncListener(void * args) {
    static_cast<s3tp_connector *>(args)->asyncListener();
    return NULL;
}

/*
 * Main Program
 */
/*void dummyCallback(char * msg, int len) {
    if (len <= 0) {
        printf("Received empty message\n");
    } else {
        printf("Received message %s\n", msg);
    }
}

int main() {
    s3tp_connector connector;
    S3TP_CONFIG config;
    config.port = 15;
    config.channel = 3;
    config.options = S3TP_PARAM_ARQ;
    connector.init(config, dummyCallback);
    std::string testS;
    std::cout << "Please insert a message to send: ";
    std::cin >> testS;
    connector.send((void *)testS.data(), testS.length());

    sleep(10);
    connector.closeConnection();

    return 0;
}*/