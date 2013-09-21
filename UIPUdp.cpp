#include "UIPUdp.h"
#include "UIPEthernet.h"

extern "C" {
#include "uip-conf.h"
#include "uip.h"
#include "uip_arp.h"
}
#include "mempool.h"

#define UIP_ARPHDRSIZE 42
#define UDPBUF ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])

// Constructor
UIPUDP::UIPUDP()
{
  _uip_udp_conn = NULL;
  appdata.rport = 0;
  appdata.ripaddr = IPAddress();
}

// initialize, start listening on specified port. Returns 1 if successful, 0 if there are no sockets available to use
uint8_t
UIPUDP::begin(uint16_t port)
{
  if (!_uip_udp_conn)
    {
      _uip_udp_conn = uip_udp_new(NULL, 0);
    }
  if (_uip_udp_conn)
    {
      uip_udp_bind(_uip_udp_conn,htons(port));
      _uip_udp_conn->appstate.user = &appdata;
      return 1;
    }
  return 0;
}

// Finish with the UDP socket
void
UIPUDP::stop()
{
  if (_uip_udp_conn)
    {
      flush();
      uip_udp_remove(_uip_udp_conn);
      _uip_udp_conn->appstate.user = NULL;
      _uip_udp_conn=NULL;
      if (appdata.packet_in != NOBLOCK)
        {
          UIPEthernet.network.freePacket(appdata.packet_in);
          appdata.packet_in = NOBLOCK;
        }
      uint8_t i = 0;
      memhandle* packet = &appdata.packets_in[0];
      while (*packet != NOBLOCK && i < UIP_UDP_NUMPACKETS)
        {
          UIPEthernet.network.freePacket(*packet);
          *packet++ = NOBLOCK;
          i++;
        }
      if (appdata.packet_out != NOBLOCK)
        {
          UIPEthernet.network.freePacket(appdata.packet_out);
          appdata.packet_out = NOBLOCK;
        }
    }
}

// Sending UDP packets

// Start building up a packet to send to the remote host specific in ip and port
// Returns 1 if successful, 0 if there was a problem with the supplied IP address or port
int
UIPUDP::beginPacket(IPAddress ip, uint16_t port)
{
  UIPEthernet.tick();
  if (ip && port)
    {
      uip_ipaddr_t ripaddr;
      uip_ip_addr(&ripaddr, ip);
      if (_uip_udp_conn)
        {
          _uip_udp_conn->rport = HTONS(port);
          uip_ipaddr_copy(_uip_udp_conn->ripaddr, &ripaddr);
        }
      else
        {
          _uip_udp_conn = uip_udp_new(&ripaddr,HTONS(port));
          _uip_udp_conn->appstate.user = &appdata;
        }
    }
  if (_uip_udp_conn && appdata.packet_out == NOBLOCK)
    {
      appdata.packet_out = UIPEthernet.network.newPacket(UIP_UDP_MAXPACKETSIZE);
      appdata.out_pos = UIP_UDP_PHYH_LEN;
      if (appdata.packet_out != NOBLOCK)
        return 1;
    }
  return 0;
}

// Start building up a packet to send to the remote host specific in host and port
// Returns 1 if successful, 0 if there was a problem resolving the hostname or port
int
UIPUDP::beginPacket(const char *host, uint16_t port)
{
  return 0; //TODO implement DNS
}

// Finish off this packet and send it
// Returns 1 if the packet was sent successfully, 0 if there was an error
int
UIPUDP::endPacket()
{
  if (_uip_udp_conn && appdata.packet_out != NOBLOCK)
    {
      appdata.send = true;
      UIPEthernet.network.resizePacket(appdata.packet_out,0,UIP_UDP_PHYH_LEN+appdata.out_pos);
      uip_udp_periodic_conn(_uip_udp_conn);
      if (uip_len > 0)
        {
          UIPEthernet.network_send();
          return 1;
        }
    }
  return 0;
}

// Write a single byte into the packet
size_t
UIPUDP::write(uint8_t c)
{
  return write(&c,1);
}

// Write size bytes from buffer into the packet
size_t
UIPUDP::write(const uint8_t *buffer, size_t size)
{
  if (appdata.packet_out != NOBLOCK)
    {
      size_t ret = UIPEthernet.network.writePacket(appdata.packet_out,appdata.out_pos,(uint8_t*)buffer,size);
      appdata.out_pos += ret;
      return ret;
    }
  return 0;
}

// Start processing the next available incoming packet
// Returns the size of the packet in bytes, or 0 if no packets are available
int
UIPUDP::parsePacket()
{
  UIPEthernet.tick();
  if (appdata.packet_in != NOBLOCK)
    {
      UIPEthernet.network.freePacket(appdata.packet_in);
    }
  memhandle *packet = &appdata.packets_in[0];
  appdata.packet_in = *packet;
  if (appdata.packet_in != NOBLOCK)
    {
      if (UIP_UDP_NUMPACKETS > 1)
        {
          uint8_t i = 1;
          memhandle* p = packet+1;
freeloop:
          *packet = *p;
          if (*packet == NOBLOCK)
            goto freeready;
          packet++;
          if (i < UIP_UDP_NUMPACKETS-1)
            {
              i++;
              p++;
              goto freeloop;
            }
        }
      *packet = NOBLOCK;
freeready:
      return UIPEthernet.network.packetLen(appdata.packet_in);
    }
  return 0;
}

// Number of bytes remaining in the current packet
int
UIPUDP::available()
{
  UIPEthernet.tick();
  if (appdata.packet_in != NOBLOCK)
    {
      return UIPEthernet.network.packetLen(appdata.packet_in);
    }
  return 0;
}

// Read a single byte from the current packet
int
UIPUDP::read()
{
  unsigned char c;
  if (read(&c,1) > 0)
    {
      return c;
    }
  return -1;
}

// Read up to len bytes from the current packet and place them into buffer
// Returns the number of bytes read, or 0 if none are available
int
UIPUDP::read(unsigned char* buffer, size_t len)
{
  UIPEthernet.tick();
  if (appdata.packet_in != NOBLOCK)
    {
      int read = UIPEthernet.network.readPacket(appdata.packet_in,0,buffer,len);
      UIPEthernet.network.resizePacket(appdata.packet_in,read);
      return read;
    }
  return 0;
}

// Return the next byte from the current packet without moving on to the next byte
int
UIPUDP::peek()
{
  UIPEthernet.tick();
  if (appdata.packet_in != NOBLOCK)
    {
      unsigned char c;
      if (UIPEthernet.network.readPacket(appdata.packet_in,0,&c,1) == 1)
        return c;
    }
  return -1;
}

// Finish reading the current packet
void
UIPUDP::flush()
{
  UIPEthernet.tick();
  if (appdata.packet_in != NOBLOCK)
    {
      UIPEthernet.network.freePacket(appdata.packet_in);
      appdata.packet_in = NOBLOCK;
    }
}

// Return the IP address of the host who sent the current incoming packet
IPAddress
UIPUDP::remoteIP()
{
  return appdata.ripaddr;
}

// Return the port of the host who sent the current incoming packet
uint16_t
UIPUDP::remotePort()
{
  return appdata.rport;
}

void UIPUDP::uip_callback(uip_udp_appstate_t *s) {
  if (appdata_t *data = (appdata_t *)s->user)
    {
      if (uip_newdata())
        {
          data->rport = ntohs(UDPBUF->srcport);
          data->ripaddr = ip_addr_uip(UDPBUF->srcipaddr);
          memhandle *packet = &data->packets_in[0];
          uint8_t i = 0;
          do
            {
              if (*packet == NOBLOCK)
                {
                  *packet = UIPEthernet.in_packet;
                  UIPEthernet.in_packet = NOBLOCK;
                  //discard Linklevel and IP and udp-header and any trailing bytes:
                  UIPEthernet.network.resizePacket(*packet,UIP_UDP_PHYH_LEN,UDPBUF->udplen);
                  break;
                }
              packet++;
              i++;
            }
          while (i < UIP_UDP_NUMPACKETS);
        }
      if (uip_poll() && data->send)
        {
          //set uip_slen (uip private) by calling uip_udp_send
          uip_udp_send(data->out_pos);
          uip_process(UIP_UDP_SEND_CONN); //generate udp + ip headers
          uip_arp_out(); //add arp
          if (uip_len != UIP_ARPHDRSIZE) //arp found ethaddr for ip (otherwise packet is replaced by arp-request)
            UIPEthernet.out_packet = data->packet_out;
          data->send = false;
        }
    }
}
