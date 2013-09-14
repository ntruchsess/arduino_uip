/*
 * Enc28J60Network.h
 *
 *  Created on: 16.09.2013
 *      Author: Norbert Truchsess
 */

#ifndef ENC28J60NETWORK_H_
#define ENC28J60NETWORK_H_

extern "C" {
#include "enc28j60.h"
}
#include "mempool.h"

#define DMARUNNING 1
#define NEWPACKET 2

/*
 * Empfangen von ip-header, arp etc...
 * wenn tcp/udp -> tcp/udp-callback -> assign new packet to connection
 */

class Enc28J60Network
{
private:
  uint8_t dmaStatus;
  uint16_t nextPacketPtr;
  MemoryPool packetPool;
  void dmaCopy(uint16_t dest, uint16_t src, uint16_t len);
  void checkDMA();

public:
  Enc28J60Network();
  memhandle receivePacket();
  uint16_t readPacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len);
  void freePacket(memhandle handle);
  memhandle newPacket(uint16_t len);
  uint16_t writePacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len);
  uint16_t packetLen(memhandle handle);
};

#endif /* ENC28J60NETWORK_H_ */
