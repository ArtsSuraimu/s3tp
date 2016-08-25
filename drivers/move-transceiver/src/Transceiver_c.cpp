#include "Transceiver.h"
#include "Transceiver_c.h"

class CTransceiver :
	public Transceiver
{
public:
	CTransceiver_CallbackRecv cb;
	
	CTransceiver(const std::string& spiDevice, const std::string& interruptPinId) :
		Transceiver(spiDevice, interruptPinId),
		cb(nullptr)
	{
	}
	
	~CTransceiver()
	{
	}
	
	void handleFrame(bool arq, int channel, void* data, int length)
	{
		if (this->cb != nullptr)
		{
			transceiver_msg msg;
			msg.arq = arq;
			msg.channel = channel;
			msg.data = data;
			msg.length = length;
			
			this->cb(arq, channel, data, length);
		}
	}
}

CTransceiver* CTransceiver_create(const char* spiDevice, const char* interruptPinId)
{
	return new CTransceiver(std::string(spiDevice), std::string(interruptPinId));
}

void CTransceiver_setCallbackRecv(CTransceiver* obj, CTransceiver_CallbackRecv cb)
{
	obj->cb = cb;
}

void CTransceiver_init(CTransceiver* obj)
{
	return obj->init();
}

int CTransceiver_sendFrame(CTransceiver* obj, transceiver_msg msg)
{
	return obj->sendFrame(msg);
}

void CTransceiver_destroy(CTrainsceiver* obj)
{
	delete obj;
}
