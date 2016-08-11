/*
 * s3tp.h
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#ifndef CORE_S3TP_H_
#define CORE_S3TP_H_

#include "constants.h"
#include "s3tp_types.h"

int s3tp_init(RAW_SEND send, RAW_RECV recv);

int s3tp_send(const void* payload, size_t len);
int s3tp_peek(void);
int s3tp_recv(OUT void* pBuf, size_t* len);


#endif /* CORE_S3TP_H_ */
