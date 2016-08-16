#include "MessageSampler.h"
#include <stdlib.h>
#include <string.h>

void removeTail(MESSAGE_SAMPLER * pMs);

int init_msg_sampler (void * pSampler,
						void * pBuf,
						unsigned int length,
						FP_CHECKFUNC isStartofMessage,
						FP_CHECKFUNC isValidMessage,
						FP_CHECKFUNC getMessageLen
						)
{
	int status;
	MESSAGE_SAMPLER * ms = (MESSAGE_SAMPLER *) pSampler;

	if(!(pSampler == 0) && !(pBuf == 0) && !(isStartofMessage == 0) && !(isValidMessage == 0))
	{
		ms->isStartOfMessage = isStartofMessage;
		ms->isMessageValid = isValidMessage;
		ms->getMessageLength = getMessageLen;
		ms->pBufStart = (char *) pBuf;
		ms->pBufEnd = ms->pBufStart + length;
		ms->lenBuf = 0;
		ms->pBuf = ms->pBufStart;
		ms->appendStreamData = &append;
		ms->sampleMessage = &sample;

		status = SUCCESS;
	}else{
		status = ERR_NULL_POINTER;
	}

	return status;
}

unsigned int sample (MESSAGE_SAMPLER* pMs,
				   char* pMessage, 
				   unsigned int length)
{
	int result = 0;
	unsigned int msgLen = 0;
	unsigned int msgMaxLen = 0;
	char * pBuffer;
	char * pLastChar;


	pLastChar = &(pMs->pBufStart[pMs->lenBuf]);

	for (pBuffer = pMs->pBufStart; pBuffer < pLastChar; pBuffer++)
	{
	   msgMaxLen = pLastChar - pBuffer;

	   if (pMs->isStartOfMessage(pBuffer, msgMaxLen))
	   {
	      msgLen = pMs->getMessageLength(pBuffer, msgMaxLen);

	      if ((msgLen > 0) && (msgLen <= msgMaxLen))      // complete message within buffer??
	      {
	         if (pMs->isMessageValid(pBuffer, msgLen))      // message valid??
	         {
	            // valid and complete message sampled => copy to destination
	            if ((pMessage != NULL) && (msgLen <= length))
	            {
	               memcpy(pMessage, pBuffer, msgLen);      // copy to destination

	               // remove message from buffer and remove leading trash bytes
				   memcpy(pMs->pBufStart, (PTR_TO_STRING)&(pBuffer[msgLen]), pMs->lenBuf - msgLen);
				   pMs->lenBuf -= msgLen;

	               removeTail(pMs);
	               result = msgLen;
	            }
	            else
	            {
	               // insufficient memory at pMessage...
					result = ERR_INSUFFICIENT_MEMORY;
	            }
	         }
	         else
	         {
	            // invalid message within buffer starting at pBuffer
	            // => remove start byte and leading trash bytes at pBuffer and
	            //    recall sampleMessage( )
				memcpy(pMs->pBufStart, pBuffer + 1, msgMaxLen - 1);
				pMs->lenBuf--;
	            
	            removeTail(pMs);

				result = pMs->sampleMessage(pMs, pMessage, length);
	         }
	      }

	      break;
	   }
	}


	return (result);
}

void removeTail( MESSAGE_SAMPLER * ms )
{

	INT32U trashBytes;

	// find start of message
	for (trashBytes = 0; trashBytes < ms->lenBuf; trashBytes++)
	{
		if (ms->isStartOfMessage((PTR_TO_STRING)&(ms->pBufStart[trashBytes]), ms->lenBuf - trashBytes))
	      break;
	}
	// remove leading trash bytes
	if (trashBytes > 0)
	{
		memcpy(ms->pBufStart, &(ms->pBufStart[trashBytes]), ms->lenBuf - trashBytes);
	   ms->lenBuf -= trashBytes;
	}

}

unsigned int append (MESSAGE_SAMPLER* pMs,
					 char* data, 
					 unsigned int length)
{
	PTR_TO_STRING pBuffer;
	INT32U bytesToCopy = 0;


	if ((data != NULL) && (length > 0))
	{
	   // get pointer to first invalid byte within buffer
		pBuffer = (PTR_TO_STRING)&(pMs->pBufStart[pMs->lenBuf]);

	   // calculate number of bytes to copy from data to pBuffer
	   if ((pMs->lenBuf + length) < MSG_SAMPLER_BUFFER_SIZE)
	      bytesToCopy = length;
	   else
	   {
		   bytesToCopy = MSG_SAMPLER_BUFFER_SIZE - pMs->lenBuf;

	   }

	   memcpy(pBuffer, data, bytesToCopy);
	   pMs->lenBuf += bytesToCopy;

	   removeTail(pMs);
	}

	return (bytesToCopy);
}
