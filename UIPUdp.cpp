#include "UIPUdp.h"
#include "UIPEthernet.h"

extern "C" {
#include "uip-conf.h"
#include "uip.h"
#include "uip_arp.h"
#include "network.h"
}

#define UIP_ARPHDRSIZE 42
#define UDPBUF ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])

// Constructor
UIPUDP::UIPUDP()
{
  _uip_udp_conn = NULL;
  appdata.mode = 0;
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
      appdata.mode = 0;
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
          appdata.mode = 0;
        }
    }
  if (_uip_udp_conn)
    {
      uip_udp_periodic_conn(_uip_udp_conn);
      if (appdata.mode == UDP_PACKET_OUT)
        {
          return 1;
        }
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
  if (appdata.mode == UDP_PACKET_OUT)
    {
      //set uip_slen (uip private) by calling uip_send
      uip_send(uip_buf+UIPEthernet.hdrlen,UIPEthernet.num);
      //let uip_process regenerate the headers:
      uip_process(UIP_UDP_SEND_CONN);
      UIPEthernet.stream_packet_write_end();
      appdata.mode = 0;
      return 1;
    }
  return 0;
}

// Write a single byte into the packet
size_t
UIPUDP::write(uint8_t c)
{
  if (appdata.mode == UDP_PACKET_OUT)
    {
      UIPEthernet.stream_packet_write(&c,1);
      return 1;
    }
  return 0;
}

// Write size bytes from buffer into the packet
size_t
UIPUDP::write(const uint8_t *buffer, size_t size)
{
  if (appdata.mode == UDP_PACKET_OUT)
    {
      UIPEthernet.stream_packet_write((unsigned char *)buffer,size);
      return size;
    }
  return 0;
}

// Start processing the next available incoming packet
// Returns the size of the packet in bytes, or 0 if no packets are available
int
UIPUDP::parsePacket()
{
  UIPEthernet.tick();
  return available();
}

// Number of bytes remaining in the current packet
int
UIPUDP::available()
{
  if (appdata.mode == UDP_PACKET_IN)
    {
      return UIPEthernet.num;
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
  if (appdata.mode == UDP_PACKET_IN)
    {
      return UIPEthernet.stream_packet_read(buffer,len);
    }
  return 0;
}

// Return the next byte from the current packet without moving on to the next byte
int
UIPUDP::peek()
{
  if (appdata.mode == UDP_PACKET_IN)
    {
      return UIPEthernet.stream_packet_peek();
    }
  return -1;
}

// Finish reading the current packet
void
UIPUDP::flush()
{
  if (appdata.mode == UDP_PACKET_IN)
    {
      UIPEthernet.stream_packet_read_end();
      appdata.mode = 0;
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
  if (s->user && *(uint8_t *)s->user == 0)
    {
      if (uip_newdata())
        {
          UIPEthernet.hdrlen = UIP_LLH_LEN+UIP_IPUDPH_LEN;
          UIPEthernet.stream_packet_read_start();
          UIPEthernet.num = ntohs(UDPBUF->udplen)-UIP_UDPH_LEN;
          appdata_t *data = (appdata_t *)s->user;
          data->mode = UDP_PACKET_IN;
          data->rport = ntohs(UDPBUF->srcport);
          data->ripaddr = ip_addr_uip(UDPBUF->srcipaddr);
        }
      else if (uip_poll())
        {
          UIPEthernet.hdrlen = UIP_LLH_LEN+UIP_IPUDPH_LEN;
          uip_udp_send(1); //set fake uip_slen, otherwise uip_process would drop the packet
          uip_process(UIP_UDP_SEND_CONN); //generate udp + ip headers
          uip_arp_out(); //add arp
          if (uip_len == UIP_ARPHDRSIZE) //is packet replaced by arp-request?
            {
              network_send();
            }
          else //arp found ethaddr for ip
            {
              UIPEthernet.stream_packet_write_start();
              ((appdata_t *)s->user)->mode = UDP_PACKET_OUT;
            }
        }
    }
}
