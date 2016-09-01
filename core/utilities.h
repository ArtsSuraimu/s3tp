/*
 * utilities.h
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#ifndef CORE_UTILITIES_H_
#define CORE_UTILITIES_H_

#include "s3tp_types.h"

uint16_t calc_checksum(const char *data, uint16_t size);
bool verify_checksum(const char *data, uint16_t len, uint16_t checksum);
//u64 get_timestamp();


#endif /* CORE_UTILITIES_H_ */
