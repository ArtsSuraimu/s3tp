//
// Created by Lorenzo Donini on 01/09/16.
//

#include <s3tp/connector/S3tpConnector.h>
#include <csignal>

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
};

int main(int argc, char* argv[]) {
    if (argc != 2)
    {
        std::cout << "Illegal arguments\n";
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    std::string testS = "";
    S3tpConnector connector;
    S3TP_CONFIG config;
    CallbackClass callback;
    int port = 0;

    int argi = 1;

    socket_path = argv[argi++];

    std::cout << "Please select a port to connect to: ";
    std::getline(std::cin, testS);
    port = std::stoi(testS);
    config.port = (uint8_t)port;
    config.channel = 3;
    config.options = S3TP_OPTION_ARQ;

    connector.init(config, &callback);
    sleep(1);

    while (connector.isConnected()) {
        std::cout << "Please insert a message to send: \n";
        std::getline(std::cin, testS);
        if (testS.compare("") == 0) {
            break;
        }
        connector.send((void *)testS.data(), testS.length());
    }

    connector.closeConnection();

    return 0;
}