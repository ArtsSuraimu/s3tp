//
// Created by Lorenzo Donini on 01/09/16.
//

#include "s3tp_shared.h"

const int LOG_LEVEL = LOG_LEVEL_DEBUG;

char * socket_path = nullptr;

int read_length_safe(int fd, size_t * out_length) {
    ssize_t rd = 0;
    S3TP_INTRO_REDUNDANT len;
    int i = 0, j = 0;
    int tempCount = 0, count = 0;

    //Receive structure, then check if all redundant values are the same, so that we are safe against bit flips
    rd = read(fd, &len, sizeof(len));
    //Checking for errors and return appropriate error code
    if (rd < 0) {
        return CODE_ERROR_SOCKET_READ;
    } else if (rd == 0) {
        return CODE_ERROR_SOCKET_NO_CONN;
    }

    *out_length = len.command[0];

    for (i=0; i<SAFE_TRANSMISSION_COUNT - 1; i++) {
        tempCount = 0;
        for (j=1; j<SAFE_TRANSMISSION_COUNT; j++) {
            if (len.command[i] == len.command[j]) {
                tempCount++;
            }
            if (tempCount > count) {
                *out_length = len.command[i];
                count = tempCount;
            }
        }
    }

    if (count <= (SAFE_TRANSMISSION_COUNT / 2)) {
        //No quorum, return error
        *out_length = 0;
        return CODE_ERROR_LENGTH_CORRUPT;
    }
    return CODE_SUCCESS;
}


int write_length_safe(int fd, size_t len) {
    S3TP_INTRO_REDUNDANT redundant_length;
    //Transmit the data N times in a structure, so that we are safe against bit flips
    for (int i=0; i<SAFE_TRANSMISSION_COUNT; i++) {
        redundant_length.command[i] = len;
    }
    if (write(fd, &redundant_length, sizeof(redundant_length)) <= 0) {
        //Error occurred. Abort.
        return CODE_ERROR_SOCKET_WRITE;
    }
    return CODE_SUCCESS;
}
