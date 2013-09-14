#ifndef MEMPOOLCONF_H
#define MEMPOOLCONF_H

typedef uint16_t memaddress;
typedef uint8_t memhandle;

#define NUM_MEMBLOCKS 10 // 4 tcp packets inbound + 4 outbound + 1 udp packet inbound + 1 outbound

//void enc28j60_mv_packet(memaddress dest, memaddress src, memaddress size);

//#define MEMBLOCK_MV enc28j60_mv_packet

#endif
