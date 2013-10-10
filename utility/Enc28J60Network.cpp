/*
 Enc28J60Network.h
 UIPEthernet network driver for Microchip ENC28J60 Ethernet Interface.

 Copyright (c) 2013 Norbert Truchsess <norbert.truchsess@t-online.de>
 All rights reserved.

 based on enc28j60.c file from the AVRlib library by Pascal Stang.
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

#include "Enc28J60Network.h"
#include "Arduino.h"

extern "C" {
#include <avr/io.h>
#include "enc28j60.h"
#include "uip.h"
}

#define DMARUNNING 1
#define DMANEWPACKET 2
#define NEWPACKETFREED 4

// set CS to 0 = active
#define CSACTIVE digitalWrite(ENC28J60_CONTROL_CS, LOW)
// set CS to 1 = passive
#define CSPASSIVE digitalWrite(ENC28J60_CONTROL_CS, HIGH)
//
#define waitspi() while(!(SPSR&(1<<SPIF)))

Enc28J60Network::Enc28J60Network() :
    MemoryPool(TXSTART_INIT, TXSTOP_INIT-TXSTART_INIT+1),
    nextPacketPtr(0xffff),
    status(0),
    bank(0xff),
    readPtr(0xffff),
    writePtr(0xffff)
{
}

void Enc28J60Network::init(uint8_t* macaddr)
{
  // initialize I/O
  // ss as output:
  pinMode(ENC28J60_CONTROL_CS, OUTPUT);
  CSPASSIVE; // ss=0
  //
  pinMode(SPI_MOSI, OUTPUT);

  pinMode(SPI_SCK, OUTPUT);

  pinMode(SPI_MISO, INPUT);


  digitalWrite(SPI_MOSI, LOW);

  digitalWrite(SPI_SCK, LOW);

  /*DDRB  |= 1<<PB3 | 1<<PB5; // mosi, sck output
  cbi(DDRB,PINB4); // MISO is input
  //
  cbi(PORTB,PB3); // MOSI low
  cbi(PORTB,PB5); // SCK low
  */
  //
  // initialize SPI interface
  // master mode and Fosc/2 clock:
  SPCR = (1<<SPE)|(1<<MSTR);
  SPSR |= (1<<SPI2X);
  // perform system reset
  writeOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
  delay(50);
  // check CLKRDY bit to see if reset is complete
  // The CLKRDY does not work. See Rev. B4 Silicon Errata point. Just wait.
  //while(!(readReg(ESTAT) & ESTAT_CLKRDY));
  // do bank 0 stuff
  // initialize receive buffer
  // 16-bit transfers, must write low byte first
  // set receive buffer start address
  nextPacketPtr = RXSTART_INIT;
  // Rx start
  writeReg(ERXSTL, RXSTART_INIT&0xFF);
  writeReg(ERXSTH, RXSTART_INIT>>8);
  // set receive pointer address
  writeReg(ERXRDPTL, RXSTART_INIT&0xFF);
  writeReg(ERXRDPTH, RXSTART_INIT>>8);
  // RX end
  writeReg(ERXNDL, RXSTOP_INIT&0xFF);
  writeReg(ERXNDH, RXSTOP_INIT>>8);
  // TX start
  writeReg(ETXSTL, TXSTART_INIT&0xFF);
  writeReg(ETXSTH, TXSTART_INIT>>8);
  // TX end
  writeReg(ETXNDL, TXSTOP_INIT&0xFF);
  writeReg(ETXNDH, TXSTOP_INIT>>8);
  // do bank 1 stuff, packet filter:
  // For broadcast packets we allow only ARP packtets
  // All other packets should be unicast only for our mac (MAADR)
  //
  // The pattern to match on is therefore
  // Type     ETH.DST
  // ARP      BROADCAST
  // 06 08 -- ff ff ff ff ff ff -> ip checksum for theses bytes=f7f9
  // in binary these poitions are:11 0000 0011 1111
  // This is hex 303F->EPMM0=0x3f,EPMM1=0x30
  //TODO define specific pattern to receive dhcp-broadcast packages instead of setting ERFCON_BCEN!
  writeReg(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN|ERXFCON_BCEN);
  writeReg(EPMM0, 0x3f);
  writeReg(EPMM1, 0x30);
  writeReg(EPMCSL, 0xf9);
  writeReg(EPMCSH, 0xf7);
  //
  //
  // do bank 2 stuff
  // enable MAC receive
  writeReg(MACON1, MACON1_MARXEN|MACON1_TXPAUS|MACON1_RXPAUS);
  // bring MAC out of reset
  writeReg(MACON2, 0x00);
  // enable automatic padding to 60bytes and CRC operations
  writeOp(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN);
  // set inter-frame gap (non-back-to-back)
  writeReg(MAIPGL, 0x12);
  writeReg(MAIPGH, 0x0C);
  // set inter-frame gap (back-to-back)
  writeReg(MABBIPG, 0x12);
  // Set the maximum packet size which the controller will accept
  // Do not send packets longer than MAX_FRAMELEN:
  writeReg(MAMXFLL, MAX_FRAMELEN&0xFF);
  writeReg(MAMXFLH, MAX_FRAMELEN>>8);
  // do bank 3 stuff
  // write MAC address
  // NOTE: MAC address in ENC28J60 is byte-backward
  writeReg(MAADR5, macaddr[0]);
  writeReg(MAADR4, macaddr[1]);
  writeReg(MAADR3, macaddr[2]);
  writeReg(MAADR2, macaddr[3]);
  writeReg(MAADR1, macaddr[4]);
  writeReg(MAADR0, macaddr[5]);
  // no loopback of transmitted frames
  phyWrite(PHCON2, PHCON2_HDLDIS);
  // switch to bank 0
  setBank(ECON1);
  // enable interrutps
  writeOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE|EIE_PKTIE);
  // enable packet reception
  writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);
  //Configure leds
  phyWrite(PHLCON,0x476);
}

memhandle
Enc28J60Network::receivePacket()
{
  uint16_t rxstat;
  uint16_t len;
  // check if a packet has been received and buffered
  //if( !(readReg(EIR) & EIR_PKTIF) ){
  // The above does not work. See Rev. B4 Silicon Errata point 6.
  if (readReg(EPKTCNT) != 0)
    {
      readPtr = nextPacketPtr+6;
      // Set the read pointer to the start of the received packet
      writeReg(ERDPTL, (nextPacketPtr));
      writeOp(ENC28J60_WRITE_CTRL_REG, ERDPTH, (nextPacketPtr) >> 8);
      // read the next packet pointer
      nextPacketPtr = readOp(ENC28J60_READ_BUF_MEM, 0);
      nextPacketPtr |= readOp(ENC28J60_READ_BUF_MEM, 0) << 8;
      // read the packet length (see datasheet page 43)
      len = readOp(ENC28J60_READ_BUF_MEM, 0);
      len |= readOp(ENC28J60_READ_BUF_MEM, 0) << 8;
      len -= 4; //remove the CRC count
      // read the receive status (see datasheet page 43)
      rxstat = readOp(ENC28J60_READ_BUF_MEM, 0);
      rxstat |= readOp(ENC28J60_READ_BUF_MEM, 0) << 8;
      // decrement the packet counter indicate we are done with this packet
      writeOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);
      // check CRC and symbol errors (see datasheet page 44, table 7-3):
      // The ERXFCON.CRCEN is set by default. Normally we should not
      // need to check this.
      if ((rxstat & 0x80) != 0)
        {
          receivePkt.begin = readPtr;
          receivePkt.size = len;
          return UIP_RECEIVEBUFFERHANDLE;
        }
      // Move the RX read pointer to the start of the next received packet
      // This frees the memory we just read out
      writeReg(ERXRDPTL, (nextPacketPtr));
      writeReg(ERXRDPTH, (nextPacketPtr) >> 8);
    }
  return (NOBLOCK);
}

memaddress
Enc28J60Network::blockSize(memhandle handle)
{
  return handle == UIP_RECEIVEBUFFERHANDLE ? receivePkt.size : blocks[handle].size;
}

void
Enc28J60Network::sendPacket(memhandle handle)
{
  memblock *packet = &blocks[handle];
  uint16_t addr = packet->begin;
  // TX start
  writeReg(ETXSTL, addr&0xFF);
  writeReg(ETXSTH, addr>>8);
  // Set the TXND pointer to correspond to the packet size given
  addr += packet->size-1;
  writeReg(ETXNDL, addr&0xFF);
  writeReg(ETXNDH, addr>>8);
  // send the contents of the transmit buffer onto the network
  writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
  // Reset the transmit logic problem. See Rev. B4 Silicon Errata point 12.
  if( (readReg(EIR) & EIR_TXERIF) )
    {
      writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
    }
}

uint16_t
Enc28J60Network::setReadPtr(memhandle handle, memaddress position, uint16_t len)
{
  memblock *packet = handle == UIP_RECEIVEBUFFERHANDLE ? &receivePkt : &blocks[handle];
  memaddress start = packet->begin + position;
  if (readPtr != start)
    {
      writeReg(ERDPTL, (start));
      writeOp(ENC28J60_WRITE_CTRL_REG, ERDPTH, (start) >> 8);
      readPtr = start;
    }
  if (len > packet->size - position)
    len = packet->size - position;
  return len;
}

uint16_t
Enc28J60Network::readPacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len)
{
  len = setReadPtr(handle, position, len);
  checkDMA();
  readBuffer(len, buffer);
  return len;
}

uint16_t
Enc28J60Network::writePacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len)
{
  memblock *packet = &blocks[handle];
  uint16_t start = packet->begin + position;
  if (writePtr != start)
    {
      writeReg(EWRPTL, (start));
      writeOp(ENC28J60_WRITE_CTRL_REG, EWRPTH, (start) >> 8);
      writePtr = start;
    }
  if (len > packet->size - position)
    len = packet->size - position;
  writeBuffer(len, buffer);
  return len;
}

void
Enc28J60Network::copyPacket(memhandle dest_pkt, memaddress dest_pos, memhandle src_pkt, memaddress src_pos, uint16_t len)
{
  memblock *dest = &blocks[dest_pkt];
  memblock *src = src_pkt == UIP_RECEIVEBUFFERHANDLE ? &receivePkt : &blocks[src_pkt];
  memblock_mv_cb(dest->begin+dest_pos,src->begin+src_pos,len);
  if (src_pkt == UIP_RECEIVEBUFFERHANDLE)
    status |= DMANEWPACKET;
}

void
Enc28J60Network::memblock_mv_cb(uint16_t dest, uint16_t src, uint16_t len)
{
  checkDMA();

  //as ENC28J60 DMA is unable to copy single bytes:
  if (len == 1)
    {
      if (readPtr == src)
        {
          readPtr++;
        }
      else
        {
          readPtr = src+1;
          // Set the read pointer to the start of the received packet
          writeReg(ERDPTL, (src));
          writeOp(ENC28J60_WRITE_CTRL_REG, ERDPTH, (src) >> 8);
        }
      if (writePtr == dest)
        {
          writePtr++;
        }
      else
        {
          writePtr = dest+1;
          writeReg(EWRPTL, (dest));
          writeOp(ENC28J60_WRITE_CTRL_REG, EWRPTH, (dest) >> 8);
        }
      uint8_t data;
      CSACTIVE;
      // issue read command
      SPDR = ENC28J60_READ_BUF_MEM;
      waitspi();
      // read data
      SPDR = 0x00;
      waitspi();
      data = SPDR;
      // issue write command
      SPDR = ENC28J60_WRITE_BUF_MEM;
      waitspi();
      // write data
      SPDR = data;
      waitspi();
      CSPASSIVE;
    }
  else
    {
      setBank(ECON1);

      // calculate address of last byte
      len += src - 1;

      /*  1. Appropriately program the EDMAST, EDMAND
       and EDMADST register pairs. The EDMAST
       registers should point to the first byte to copy
       from, the EDMAND registers should point to the
       last byte to copy and the EDMADST registers
       should point to the first byte in the destination
       range. The destination range will always be
       linear, never wrapping at any values except from
       8191 to 0 (the 8-Kbyte memory boundary).
       Extreme care should be taken when
       programming the start and end pointers to
       prevent a never ending DMA operation which
       would overwrite the entire 8-Kbyte buffer.
       */
      writeOp(ENC28J60_WRITE_CTRL_REG, EDMASTL, (src));
      writeOp(ENC28J60_WRITE_CTRL_REG, EDMASTH, (src) >> 8);
      writeOp(ENC28J60_WRITE_CTRL_REG, EDMADSTL, (dest));
      writeOp(ENC28J60_WRITE_CTRL_REG, EDMADSTH, (dest) >> 8);

      if ((src <= RXSTOP_INIT)&& (len > RXSTOP_INIT))len -= (RXSTOP_INIT-RXSTART_INIT);
      writeOp(ENC28J60_WRITE_CTRL_REG, EDMANDL, (len));
      writeOp(ENC28J60_WRITE_CTRL_REG, EDMANDH, (len) >> 8);

      /*
       2. If an interrupt at the end of the copy process is
       desired, set EIE.DMAIE and EIE.INTIE and
       clear EIR.DMAIF.

       3. Verify that ECON1.CSUMEN is clear. */
      writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_CSUMEN);

      /* 4. Start the DMA copy by setting ECON1.DMAST. */
      writeOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_DMAST);

      status |= DMARUNNING;
    }
}

void
Enc28J60Network::checkDMA()
{
  if (status & DMARUNNING)
    {
      // wait until runnig DMA is completed
      while (readOp(ENC28J60_READ_CTRL_REG, ECON1) & ECON1_DMAST);

      if (status & DMANEWPACKET)
        {
          // Move the RX read pointer to the start of the next received packet
          // This frees the memory we just read out
          writeReg(ERXRDPTL, (nextPacketPtr));
          writeReg(ERXRDPTH, (nextPacketPtr) >> 8);
          status = NEWPACKETFREED;
        }
      else
        status &= ~DMARUNNING;
    }
}

void
Enc28J60Network::freePacket()
{
  if (status & DMANEWPACKET)
    // wait until runnig DMA is completed
    while (readOp(ENC28J60_READ_CTRL_REG, ECON1) & ECON1_DMAST);
  if (!(status & NEWPACKETFREED))
    {
      // Move the RX read pointer to the start of the next received packet
      // This frees the memory we just read out
      writeReg(ERXRDPTL, (nextPacketPtr));
      writeReg(ERXRDPTH, (nextPacketPtr) >> 8);
    }
  status = 0;
}

uint8_t
Enc28J60Network::readOp(uint8_t op, uint8_t address)
{
  CSACTIVE;
  // issue read command
  SPDR = op | (address & ADDR_MASK);
  waitspi();
  // read data
  SPDR = 0x00;
  waitspi();
  // do dummy read if needed (for mac and mii, see datasheet page 29)
  if(address & 0x80)
  {
    SPDR = 0x00;
    waitspi();
  }
  // release CS
  CSPASSIVE;
  return(SPDR);
}

void
Enc28J60Network::writeOp(uint8_t op, uint8_t address, uint8_t data)
{
  CSACTIVE;
  // issue write command
  SPDR = op | (address & ADDR_MASK);
  waitspi();
  // write data
  SPDR = data;
  waitspi();
  CSPASSIVE;
}

void
Enc28J60Network::readBuffer(uint16_t len, uint8_t* data)
{
  readPtr+=len;
  CSACTIVE;
  // issue read command
  SPDR = ENC28J60_READ_BUF_MEM;
  waitspi();
  while(len)
  {
    len--;
    // read data
    SPDR = 0x00;
    waitspi();
    *data = SPDR;
    data++;
  }
  //*data='\0';
  CSPASSIVE;
}

void
Enc28J60Network::writeBuffer(uint16_t len, uint8_t* data)
{
  writePtr+=len;
  CSACTIVE;
  // issue write command
  SPDR = ENC28J60_WRITE_BUF_MEM;
  waitspi();
  while(len)
  {
    len--;
    // write data
    SPDR = *data;
    data++;
    waitspi();
  }
  CSPASSIVE;
}

void
Enc28J60Network::setBank(uint8_t address)
{
  // set the bank (if needed)
  if((address & BANK_MASK) != bank)
  {
    // set the bank
    writeOp(ENC28J60_BIT_FIELD_CLR, ECON1, (ECON1_BSEL1|ECON1_BSEL0));
    writeOp(ENC28J60_BIT_FIELD_SET, ECON1, (address & BANK_MASK)>>5);
    bank = (address & BANK_MASK);
  }
}

uint8_t
Enc28J60Network::readReg(uint8_t address)
{
  // set the bank
  setBank(address);
  // do the read
  return readOp(ENC28J60_READ_CTRL_REG, address);
}

void
Enc28J60Network::writeReg(uint8_t address, uint8_t data)
{
  // set the bank
  setBank(address);
  // do the write
  writeOp(ENC28J60_WRITE_CTRL_REG, address, data);
}

void
Enc28J60Network::phyWrite(uint8_t address, uint16_t data)
{
  // set the PHY register address
  writeReg(MIREGADR, address);
  // write the PHY data
  writeReg(MIWRL, data);
  writeReg(MIWRH, data>>8);
  // wait until the PHY write completes
  while(readReg(MISTAT) & MISTAT_BUSY){
    delayMicroseconds(15);
  }
}

void
Enc28J60Network::clkout(uint8_t clk)
{
  //setup clkout: 2 is 12.5MHz:
  writeReg(ECOCON, clk & 0x7);
}

// read the revision of the chip:
uint8_t
Enc28J60Network::getrev(void)
{
  return(readReg(EREVID));
}

uint16_t
Enc28J60Network::chksum(uint16_t sum, memhandle handle, memaddress pos, uint16_t len)
{
  uint16_t t;
  checkDMA();
  len = setReadPtr(handle, pos, len)-1;
  readPtr+=len;
  CSACTIVE;
  // issue read command
  SPDR = ENC28J60_READ_BUF_MEM;
  waitspi();
  uint16_t i;
  for (i = 0; i < len; i+=2)
  {
    // read data
    SPDR = 0x00;
    waitspi();
    t = SPDR << 8;
    SPDR = 0x00;
    waitspi();
    t += SPDR;
    sum += t;
    if(sum < t) {
      sum++;            /* carry */
    }
  }
  if(i == len) {
    SPDR = 0x00;
    waitspi();
    t = (SPDR << 8) + 0;
    sum += t;
    if(sum < t) {
      sum++;            /* carry */
    }
  }
  CSPASSIVE;

  /* Return sum in host byte order. */
  return sum;
}
