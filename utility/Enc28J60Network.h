/*
 Enc28J60Network.h
 UIPEthernet network driver for Microchip ENC28J60 Ethernet Interface.

 Copyright (c) 2013 Norbert Truchsess <norbert.truchsess@t-online.de>
 All rights reserved.

 inspired by enc28j60.c file from the AVRlib library by Pascal Stang.
 For AVRlib See http://www.procyonengineering.com/

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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

#define UIP_OUTPACKETOFFSET 1
#define UIP_INPACKETOFFSET 0

#define UIP_RECEIVEBUFFERHANDLE 0xff

/*
 * Empfangen von ip-header, arp etc...
 * wenn tcp/udp -> tcp/udp-callback -> assign new packet to connection
 */

class Enc28J60Network : public MemoryPool
{

private:
  uint8_t status;
  uint16_t nextPacketPtr;
  uint16_t readPtr;
  uint16_t writePtr;
  uint8_t bank;

  struct memblock receivePkt;

  void checkDMA();

  uint8_t readOp(uint8_t op, uint8_t address);
  void writeOp(uint8_t op, uint8_t address, uint8_t data);
  uint16_t setReadPtr(memhandle handle, memaddress position, uint16_t len);
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
  void freePacket();
  memaddress blockSize(memhandle handle);
  void sendPacket(memhandle handle);
  uint16_t readPacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len);
  uint16_t writePacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len);
  void copyPacket(memhandle dest, memaddress dest_pos, memhandle src, memaddress src_pos, uint16_t len);
  uint16_t chksum(uint16_t sum, memhandle handle, memaddress pos, uint16_t len);
};

#endif /* ENC28J60NETWORK_H_ */
