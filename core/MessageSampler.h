#ifndef INC_MESSAGESAMPLER_H
#define INC_MESSAGESAMPLER_H

#define MSG_SAMPLER_BUFFER_SIZE 1024

typedef struct tag_message_sampler MESSAGE_SAMPLER;
typedef char* PTR_TO_STRING;
typedef unsigned int INT32U;
typedef unsigned char CHARU;
typedef char CHARS;
typedef unsigned char INT8U;

typedef int (* FP_CHECKFUNC) (char * message,
							  int length);

typedef unsigned int (* FP_UTILITY) (MESSAGE_SAMPLER * pMs,
									 char * message, 
									 unsigned int length);
/*
typedef enum tag_bool{
	FALSE = 0,
	TRUE = 1
}BOOLEAN;
*/

typedef enum tag_message_sampler_error{
	SUCCESS = 0x0,
	ERR_NULL_POINTER = -1,
	ERR_BUF_FULL = -2,
	ERR_GENERAL = -3,
	ERR_INSUFFICIENT_MEMORY = -4
}MESSAGE_SAMPLER_ERROR;

typedef struct tag_message_sampler{
	char * pBufStart;
	char * pBufEnd;
	char * pBuf;

	unsigned int lenBuf;

	FP_CHECKFUNC isStartOfMessage;
	FP_CHECKFUNC isMessageValid;
	FP_CHECKFUNC getMessageLength;
	FP_UTILITY sampleMessage;
	FP_UTILITY appendStreamData;
};

unsigned int sample (MESSAGE_SAMPLER* pMs,
						  char* pMessage, 
						  unsigned int length);

unsigned int append (MESSAGE_SAMPLER* pMs,
							   char* data, 
							   unsigned int length);

int initMessageSampler (void * pSampler,
						void * pBuf,
						unsigned int length,
						FP_CHECKFUNC isStartofMessage,
						FP_CHECKFUNC isValidMessage,
						FP_CHECKFUNC getMessageLen
						);

#endif