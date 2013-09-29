/*********************************************
 * Enc28J60Network.h
 * Author: Norbert Truchsess
 * Copyright: GPL V2
 * http://www.gnu.org/licenses/gpl.html
 *
 * Based on the enc28j60.c file from the AVRlib library by Pascal Stang.
 * For AVRlib See http://www.procyonengineering.com/
 *
 * Title: Microchip ENC28J60 Ethernet Interface Driver
 * Chip type           : ATMEGA88 with ENC28J60
 *********************************************/

#ifndef ENC28J60NETWORK_H_
#define ENC28J60NETWORK_H_

#include "mempool.h"

#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
#define ENC28J60_CONTROL_CS     53
#define SPI_MOSI        51
#define SPI_MISO        50
#define SPI_SCK         52
#else;
#define ENC28J60_CONTROL_CS     10
#define SPI_MOSI        11
#define SPI_MISO        12
#define SPI_SCK         13
#endif;

/*
 * Empfangen von ip-header, arp etc...
 * wenn tcp/udp -> tcp/udp-callback -> assign new packet to connection
 */

class Enc28J60Network : public MemoryPool
{

private:
  uint8_t dmaStatus;
  uint16_t nextPacketPtr;
  uint16_t readPtr;
  uint16_t writePtr;
  uint8_t bank;

  void checkDMA();

  uint8_t readOp(uint8_t op, uint8_t address);
  void writeOp(uint8_t op, uint8_t address, uint8_t data);
  void readBuffer(uint16_t len, uint8_t* data);
  void writeBuffer(uint16_t len, uint8_t* data);
  void setBank(uint8_t address);
  uint8_t readReg(uint8_t address);
  void writeReg(uint8_t address, uint8_t data);
  void phyWrite(uint8_t address, uint16_t data);
  void clkout(uint8_t clk);
  uint8_t getrev(void);

protected:
  void memblock_mv_cb(memaddress dest, memaddress src, memaddress size);

public:
  Enc28J60Network();
  void init(uint8_t* macaddr);
  memhandle receivePacket();
  void sendPacket(memhandle handle);
  uint16_t readPacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len);
  void resizePacket(memhandle, memaddress position);
  void resizePacket(memhandle, memaddress position, memaddress size);
  void freePacket(memhandle handle);
  memhandle newPacket(uint16_t len);
  uint16_t writePacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len);
  void copyPacket(memhandle dest, memaddress dest_pos, memhandle src, memaddress src_pos, uint16_t len);
  uint16_t packetLen(memhandle handle);
};

#endif /* ENC28J60NETWORK_H_ */
