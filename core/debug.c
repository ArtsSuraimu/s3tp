/*
 * debug.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */


#include <stdio.h>
#include "debug.h"

int hexdump(
		const unsigned char *buffer,
		ssize_t len)
{
	int i,n;

	n = 0;
	while (n<len) {
		fprintf(stdout,"%04x    ",n);
		for (i=0; i<16 && n<len; i++,n++) {
			if (i==8)
				fprintf(stdout," ");

			fprintf(stdout,"%02hhx ",buffer[n]);
		}
		fprintf(stdout,"\n");
	}

	return n;
}
