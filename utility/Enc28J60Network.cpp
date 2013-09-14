/*
 * Enc28J60Network.cpp
 *
 *  Created on: 16.09.2013
 *      Author: norbert
 */

#include "Enc28J60Network.h"

extern "C" {
#include "enc28j60.h"
}

Enc28J60Network::Enc28J60Network() : packetPool(TXSTART_INIT, TXSTOP_INIT-TXSTART_INIT), nextPacketPtr(0), dmaStatus(0)
{
}

void
Enc28J60Network::dmaCopy(uint16_t dest, uint16_t src, uint16_t len)
{
  checkDMA();

  enc28j60SetBank(ECON1);

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
  enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG, EDMASTL, (src));
  enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG, EDMASTH, (src) >> 8);
  enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG, EDMADSTL, (dest));
  enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG, EDMADSTH, (dest) >> 8);

  if ((src <= RXSTOP_INIT)&& (len > RXSTOP_INIT))len -= (RXSTOP_INIT-RXSTART_INIT);
  enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG, EDMANDL, (len));
  enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG, EDMANDH, (len) >> 8);

  /*
   2. If an interrupt at the end of the copy process is
   desired, set EIE.DMAIE and EIE.INTIE and
   clear EIR.DMAIF.

   3. Verify that ECON1.CSUMEN is clear. */
  enc28j60WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_CSUMEN);

  /* 4. Start the DMA copy by setting ECON1.DMAST. */
  enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_DMAST);

  dmaStatus = DMARUNNING;

}

memhandle
Enc28J60Network::receivePacket()
{
  uint16_t rxstat;
  uint16_t len;
  uint16_t nextPacketPtrLocal;
  // check if a packet has been received and buffered
  //if( !(enc28j60Read(EIR) & EIR_PKTIF) ){
  // The above does not work. See Rev. B4 Silicon Errata point 6.
  if (enc28j60Read(EPKTCNT) == 0)
    {
      return (NOBLOCK);
    }

  memaddress src = nextPacketPtr + 6;
  memhandle handle = NOBLOCK;
  // Set the read pointer to the start of the received packet
  enc28j60Write(ERDPTL, (nextPacketPtr));
  enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG, ERDPTH, (nextPacketPtr) >> 8);
  // read the next packet pointer
  nextPacketPtrLocal = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
  nextPacketPtrLocal |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;
  // read the packet length (see datasheet page 43)
  len = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
  len |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;
  len -= 4; //remove the CRC count
  // read the receive status (see datasheet page 43)
  rxstat = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
  rxstat |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;
  // check CRC and symbol errors (see datasheet page 44, table 7-3):
  // The ERXFCON.CRCEN is set by default. Normally we should not
  // need to check this.
  if ((rxstat & 0x80) == 0)
    {
      // invalid
      len = 0;
    }
  else
    {
      // copy the packet from the receive buffer
      handle = packetPool.allocBlock(len);
      // ignore if no space left (try again on next call to receivePacket)
      if (handle != NOBLOCK)
        {
          nextPacketPtr = nextPacketPtrLocal;
          dmaCopy(packetPool.blocks[handle].begin, src, len);
          dmaStatus |= NEWPACKET;
        }
    }
  return (handle);
}

void
Enc28J60Network::checkDMA()
{
  if (dmaStatus)
    {
      // wait until runnig DMA is completed

      while (enc28j60ReadOp(ENC28J60_READ_CTRL_REG, ECON1) & ECON1_DMAST)
        ;
      if (dmaStatus && NEWPACKET)
        {
          // Move the RX read pointer to the start of the next received packet
          // This frees the memory we just read out
          enc28j60Write(ERXRDPTL, (nextPacketPtr));
          enc28j60Write(ERXRDPTH, (nextPacketPtr) >> 8);
          // decrement the packet counter indicate we are done with this packet
          enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);
        }
      dmaStatus = 0;
    }
}

uint16_t
Enc28J60Network::readPacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len)
{
  memblock *packet = &packetPool.blocks[handle];
  memaddress start = packet->begin + position;
  enc28j60Write(ERDPTL, (start));
  enc28j60Write(ERDPTH, (start) >> 8);
  if (len > packet->size - position)
    len = packet->size - position;
  checkDMA();
  enc28j60ReadBuffer(len, buffer);
  return len;
}

void
Enc28J60Network::freePacket(memhandle handle)
{
  packetPool.freeBlock(handle);
}

memhandle
Enc28J60Network::newPacket(uint16_t len)
{
  return packetPool.allocBlock(len);
}

uint16_t
Enc28J60Network::writePacket(memhandle handle, memaddress position, uint8_t* buffer, uint16_t len)
{
  memblock *packet = &packetPool.blocks[handle];
  memaddress start = packet->begin + position;
  enc28j60Write(EWRPTL, (start));
  enc28j60Write(EWRPTH, (start) >> 8);
  if (len > packet->size - position)
    len = packet->size - position;
  enc28j60WriteBuffer(len, buffer);
  return len;
}

uint16_t Enc28J60Network::packetLen(memhandle handle)
{
  return packetPool.blocks[handle].size;
}

