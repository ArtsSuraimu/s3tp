#include "Transceiver.h"

Transceiver::Transceiver(const std::string& spiDevice, const std::string& interruptPinId) :
	active(false),
	spi(spiDevice, SPIDevice::MODE_CPHA),
	interruptPin(interruptPinId),
	interruptHandler(),
	messageHandler()
{
}

Transceiver::~Transceiver()
{
	this->active = false;
	
	this->messageHandler.join();
	this->interruptHandler.join();
}

void Transceiver::init()
{
	this->active = true;
	this->interruptHandler(&Transceiver::interruptMethod, std::ref(*this));
	this->messageHandler(&Transceiver::messageMethod, std::ref(*this));
}

bool Transceiver::sendFrame(bool arq, int channel, void* data, int length)
{
	// TODO: queue packet
}

void Transceiver::transferPacket(int payloadSize)
{
	int packetSize = payloadSize + 4;
	if (packetSize > 4096)
	{
		// TODO: wrong argument
		return;
	}
	
	short crc16 = Transceiver::getCrc16(this->sendBuf, payloadSize + 1);
	this->sendBuf[payloadSize+1] = (unsigned char) (crc >> 8);
	this->sendBuf[payloadSize+2] = (unsigned char) crc;
	
	int i;
	for (i=0 ; i<5 ; i++)
	{
		if (!this->spi.transfer(this->recvBuf, this->sendBuf, packetSize))
		{
			
		}
	}
	
	if (i == 5)
	{
		// TODO: too many errors!
		return;
	}
	
	
	// packet sent
	return;
}

void Transceiver::interruptMethod(Transceiver* t)
{
	GPIOPin& pin = t->interruptPin;
	
	pin.setDirection(GPIOPin::INPUT);
	pin.setEdgeTrigger(GPIOPin::TRIGGER_RISING);
	
	while (t->active)
	{
		pin.waitForEdge();
		
		if (pin.readPin())
		{
			// TODO: queue parse message
		}
	}
}

void Transceiver::messageMethod(Transceiver* t)
{
	BlockingQueue<TransceiverEvent> &events = t->events;
	
	while (t->active)
	{
		TransceiverEvent event = events.pop();
		
		switch (event.type)
		{
		case Transceiver::EVENT_INTR:
			t->handleInterruptRequest();
			break;
		case Transceiver::EVENT_SEND:
			break;
		}
		// TODO: handle event
	}
}

short Transceiver::getCrc16(void* data, int length)
{
	// TODO: include crc16 routine
	return 0;
}
