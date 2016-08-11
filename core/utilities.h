/*
 * utilities.h
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#ifndef CORE_UTILITIES_H_
#define CORE_UTILITIES_H_

#include "s3tp_types.h"


i8 calc_checksum(const void * pdu, int len);
bool verify_checksum(const void * pdu, int len, i8 checksum);

#endif /* CORE_UTILITIES_H_ */
