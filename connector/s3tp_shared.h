//
// Created by Lorenzo Donini on 30/08/16.
//

#include <stdlib.h>
#include <stdio.h>

#define S3TP_PARAM_ARQ 0x01;
#define S3TP_PARAM_CUSTOM 0x02;

#define CODE_ERROR_SOCKET_CREATE -1
#define CODE_ERROR_SOCKET_CONNECT -2
#define CODE_ERROR_SOCKET_BIND -3
#define CODE_ERROR_SOCKET_CONFIG -4
#define CODE_ERROR_SOCKET_NO_CONN -5
#define CODE_SUCCESS 0

typedef unsigned char u8;
typedef char i8;
typedef unsigned short u16;
typedef short i16;
typedef int i32;
typedef unsigned int u32;
typedef long i64;
typedef unsigned long u64;
typedef long long i128;
typedef unsigned long long u128;
typedef int SOCKET;

extern const char * socket_path;

typedef void (*S3TP_CALLBACK) (char*, int);

typedef struct tag_s3tp_config {
    u8 port;
    u8 channel;
    u8 options;

    void setArq(int active) {
        options ^= (active & 0x01);
    }
}S3TP_CONFIG;

/*void read_safe(int fd, void * buf, int size) {
    //Read 3 times

}*/