//
// Created by Lorenzo Donini on 01/09/16.
//

//#include <s3tp/connector/S3tpConnector.h>
#include "../connector/S3tpConnector.h"
#include <csignal>

std::condition_variable newConnectionCond;
std::mutex mainMutex;

/*
 * Main Test Program
 */
class CallbackClass: public S3tpCallback {
    virtual void onNewMessage(char * data, size_t len) {
        if (len <= 0) {
            LOG_INFO("Received empty message");
            return;
        }
        LOG_INFO(std::string("Received message <" + std::string(data, len) + ">"));
    }

    virtual void onError(int code, char * error) {
        LOG_ERROR(std::string("Received error with code " + std::to_string(code)));
    }

    virtual void onConnectionUp() {
        LOG_INFO(std::string("Connection was established"));
        newConnectionCond.notify_all();
    }

    virtual void onConnectionDown(int code) {
        LOG_INFO(std::string("Connection was closed with code " + std::to_string(code)));
    }
};

void communicationRoutine(S3tpConnector& connector) {
    std::string data = "";

    while (connector.isBound()) {
        std::cout << "Please insert a message to send: \n";
        std::getline(std::cin, data);
        if (data.compare("") == 0) {
            break;
        }
        connector.send((void *)data.data(), data.length());
    }

    connector.closeConnection();

    std::cout << "Lost connection to s3tpd. Exiting" << std::endl;
}

int main(int argc, char* argv[]) {
    //args: socket_path L/C
    if (argc != 3)
    {
        std::cout << "Illegal arguments\n";
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    std::string appPort = "";
    std::string mode;
    S3tpConnector connector;
    S3TP_CONFIG config;
    CallbackClass callback;
    int port = 0;

    int argi = 1;

    socket_path = argv[argi++];
    mode = argv[argi++];
    std::cout << "Please select the application port: ";
    std::getline(std::cin, appPort);
    port = std::stoi(appPort);
    config.port = (uint8_t)port;
    config.channel = 3;
    config.options = S3TP_OPTION_ARQ;
    connector.init(config, &callback);
    sleep(1);

    if (mode.compare("L") == 0) {
        std::cout << "Listening for incoming connections..." << std::endl;
        connector.listen();
        std::unique_lock<std::mutex> lock(mainMutex);
        newConnectionCond.wait(lock);
        std::cout << "Connected to remote host!" << std::endl;
        communicationRoutine(connector);
    } else if (mode.compare("C") == 0) {
        if (connector.connectToRemoteHost()) {
            std::cout << "Connected to remote host!" << std::endl;
            communicationRoutine(connector);
        } else {
            std::cout << "Connection to remote host failed. Quitting" << std::endl;
        }
    } else {
        std::cout << "Invalid mode. Quitting" << std::endl;
    }

    return 0;
}