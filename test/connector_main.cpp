//
// Created by Lorenzo Donini on 01/09/16.
//

#include "../connector/S3tpConnector.h"
#include <string>
#include <iostream>

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

struct MyTest {
    int val1;
    char test[5];
    long val2;
};

int main(int argc, char* argv[]) {
    if (argc != 2)
    {
        std::cout << "Illegal arguments\n";
        return -1;
    }

    S3tpConnector connector;
    S3TP_CONFIG config;
    CallbackClass callback;
    bool async = false;
    int port = 0;

    int argi = 1;

    socket_path = argv[argi++];

    std::cout << "Please select a port to connect to: ";
    std::cin >> port;
    config.port = (uint8_t)port;
    config.channel = 3;
    config.options = S3TP_OPTION_ARQ;
    printf("Options: %d\n", config.options);
    std::string testS = "y";
    //std::cout << "Async comm (y/n)? ";
    //std::cin >> testS;
    if (testS.compare("n") == 0) {
        connector.init(config, nullptr);
    } else {
        async = true;
        connector.init(config, &callback);
    }
    sleep(1);
    if (!async) {
        MyTest testStr;
        testStr.val1 = 10;
        testStr.val2 = 9;
        testStr.test[0] = 'p';
        testStr.test[1] = 'i';
        testStr.test[2] = 'p';
        testStr.test[3] = 'i';
        testStr.test[4] = '\0';
        connector.send(&testStr, sizeof(testStr));

        MyTest response;
        int res = connector.recv(&response, sizeof(response));
        printf("Result code %d. Received %d, %s, %ld\n", res, response.val1, response.test, response.val2);
        printf("What now?\n");
    } else {
        while (testS.compare("") != 0 && connector.isConnected()) {
            std::cout << "Please insert a message to send: \n";
            std::cin >> testS;
            connector.send((void *)testS.data(), testS.length());
        }
    }

    connector.closeConnection();
    printf("Client exited\n");

    return 0;
}