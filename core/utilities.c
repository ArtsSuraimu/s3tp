/*
 * utilities.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */

#include "utilities.h"

i8 calc_checksum(
		const void * pdu,
		int len)
{
	i8 ret = 0;
	int i = 0;
	i8* p = (i8*) pdu;

	if(pdu == NULL)
	{
		return (0);
	}

	for(i=0;i<len;i++)
	{
		ret += p[i];
	}

	return ret;
}

bool verify_checksum(
		const void * pdu,
		int len,
		i8 checksum)
{
	bool ret = (calc_checksum(pdu, len) - checksum);
	return ret;
}
