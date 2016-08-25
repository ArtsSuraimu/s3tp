#ifndef MOVE_TRANSCEIVER
#define MOVE_TRANSCEIVER

#include <string>
#include <memory>
#include <cstdint>
#include <thread>

#include <moveio/SPIDevice.h>
#include <moveio/GPIOPin.h>

#include "BlockingQueue.h"

/*
SEND 0x01 Enqueue one frame for transmission variable
RECV S 0x11 Get next frame(s) up to a total size of 256 bytes 256 bytes
RECV M 0x12 Get next frame(s) up to a total size of 1 kbyte 1 kbyte
RECV L 0x13 Get next frame(s) up to a total size of 4092 bytes 4092 bytes
RECV ACC 0x1F Acknowledge last RECV 2 bytes
STAT 0x21 Get the current status information 24 bytes
BUFF 0x22 Get the current buffer information 10 bytes
PARQ 0x31 Query the current params 2 bytes
PARS 0x32 Set a new array of params 2 bytes
INTR 0x81 Get interrupt reason 2 bytes
*/

typedef interrupt_flags uint16_t

struct TR_Buffer
{
	unsigned char vc_counts[8];
	unsigned char rx_count;
	unsigned char vc_full_flags;
};

#define TR_INTR_CONUP 0x1
#define TR_INTR_RXS 0x2
#define TR_INNTR_RXM 0x4
#define TR_INTR_RXL 0x8

#define TR_PAR_POWER_MASK (0x3 << 1)
#define TR_PAR_POWER_0 (0x0 << 1)
#define TR_PAR_POWER_1 (0x1 << 1)
#define TR_PAR_POWER_2 (0x2 << 1)
#define TR_PAR_POWER_3 (0x3 << 1)

#define TR_PAR_SAFEMODE_MASK 0x1
#define TR_PAR_SAFEMODE 0x1

class Transceiver
{
public:
	
	Transceiver(const std::string& spiDevice, const std::string& interruptPinId);
	
	void init();
	
	int sendFrame(bool arq, int channel, void* data, int length);
	virtual void handleFrame(bool arq, int channel, void* data, int length) = 0;
	
protected:
	~Transceiver();
	
private:
	enum EVENTS
	{
		EVENT_INTR,
		EVENT_SEND
	}
	
	bool active;
	
	SPIDevice spi;
	GPIOPin interruptPin;
	
	std::unique_ptr<std::thread> interruptHandler;
	std::unique_ptr<std::thread> messageHandler;
	
	BlockingQueue<TransceiverEvent> events;
	
	char recvBuf[4096], sendBuf[4096];
	void transferPacket();
	
	static void interruptMethod(Transceiver* t);
	static void messageMethod(Transceiver* t);
	
	static short getCrc16(void* data, int length);
};

#endif MOVE_TRANSCEIVER
