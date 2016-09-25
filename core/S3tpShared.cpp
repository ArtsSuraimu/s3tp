//
// Created by Lorenzo Donini on 01/09/16.
//

#include "S3tpShared.h"

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

uint8_t safe_bool_interpretation(uint8_t val) {
    if (val == 0x7F || val > 0x80) {
        return 0xFF;
    } else {
        return 0x00;
    }
}

AppControlMessageType safeMessageTypeInterpretation(uint8_t val) {
    int i=0;
    int count_low = 0, count_high = 0;
    for (i = 0; i < 4; i++) {
        if ((val % 2) != 0) {
            //First bit is 1
            count_low++;
        }
        val = val >> 1;
    }
    for (i = 0; i < 4; i++) {
        if ((val % 2) != 0) {
            //First bit is 1
            count_high++;
        }
        val = val >> 1;
    }
    if (count_low <= 1 && count_high <= 1) {
        return ACK; //Must be 0
    } else if (count_high <= 1) {
        return NACK;
    } else if (count_low <= 1) {
        return AVAILABLE;
    }
    return RESERVED;
}
