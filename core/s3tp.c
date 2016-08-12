/*
 * s3tp.c
 *
 *  Created on: Aug 11, 2016
 *      Author: dai
 */
#include <pthread.h>

#include "s3tp.h"
#include "queue.h"

static pthread_t thdRx;
static pthread_t thdTx;
static RAW_RECV recv;
static RAW_SEND send;
static qnode_t queue;
static int peer_ident;
static int con_opts;
static int run;

void cleanup (
		int sig)
{
	fprintf(stderr,"\nReceived signal %d. Shutting down...\n",sig);
	run = 0;
}


int init(
		RAW_RECV rx_func,
		RAW_SEND tx_func,
		void* recvBuf,
		void* sendBuf,
		ssize_t recvBufLen,
		ssize_t sendBufLen,
		int options,
		S3TP_CALLBACK callback,
		void* pData)
{
	//check consistency
	if(rx_func == 0 || tx_func == 0)
	{
		return -1;
	}


	//process user callback
	if(callback!=0)
	{
		callback(pData);
	}

	//assign parameters
	recv = rx_func;
	send = tx_func;
	con_opts= options;

	if(con_opts & REORDERING)
	{
		queue = init_queue();
	}

	//
	peer_ident = (con_opts & TYPE_SAT) + (con_opts & TYPE_GND);

	run = 1;

}
