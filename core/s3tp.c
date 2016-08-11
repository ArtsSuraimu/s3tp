/*
 * s3tp.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */
#include <pthread.h>

#include "s3tp.h"

static pthread_t thdRx;
static pthread_t thdTx;
static int run;

void cleanup (
		int sig)
{
	fprintf(stderr,"\nReceived signal %d. Shutting down...\n",sig);
	run = 0;
}
