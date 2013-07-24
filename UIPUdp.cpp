#include "UIPUdp.h"
#include "UIPEthernet.h"
#include "uip-conf.h"
#include "uip.h"

// Constructor
UIPUDP::UIPUDP()
{

}

// initialize, start listening on specified port. Returns 1 if successful, 0 if there are no sockets available to use
uint8_t
UIPUDP::begin(uint16_t port)
{
  _uip_udp_conn = uip_udp_new(NULL, port);
  UIPEthernet.tick();
  return _uip_udp_conn ? 1 : 0;
}

// Finish with the UDP socket
void
UIPUDP::stop()
{
  if (_uip_udp_conn)
    {
      flush();
      uip_udp_remove(_uip_udp_conn);
      _uip_udp_conn=NULL;
    }
}

// Sending UDP packets

// Start building up a packet to send to the remote host specific in ip and port
// Returns 1 if successful, 0 if there was a problem with the supplied IP address or port
int
UIPUDP::beginPacket(IPAddress ip, uint16_t port)
{
  if (!_uip_udp_conn)
    {
      begin(0);
    }
  if (_uip_udp_conn)
    {
      uip_ip_addr(_uip_udp_conn->ripaddr, ip);
      _uip_udp_conn->rport = port;
      uip_udp_appstate_t *s = &_uip_udp_conn->appstate;
      if (!s->user)
        {
          s->user = (uint8_t*)malloc(UDP_TX_PACKET_MAX_SIZE);
          s->len = UDP_TX_PACKET_MAX_SIZE;
        }
      s->pos = 0;
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
  if (_uip_udp_conn)
    {
      uip_udp_appstate_t *s = &_uip_udp_conn->appstate;
      uip_send((char *) s->user, s->len);
      flush();
      return 1;
    }
  return 0;
}

// Write a single byte into the packet
size_t
UIPUDP::write(uint8_t c)
{
  if (_uip_udp_conn)
    {
      uip_udp_appstate_t *s = &_uip_udp_conn->appstate;
      if (s->pos < s->len && s->user)
        {
          s->user[s->pos++] = c;
          return 1;
        }
    }
  return 0;
}

// Write size bytes from buffer into the packet
size_t
UIPUDP::write(const uint8_t *buffer, size_t size)
{
  if (_uip_udp_conn)
    {
      uip_udp_appstate_t *s = &_uip_udp_conn->appstate;
      if (s->user) {
        size_t len = size < s->len - s->pos ? size : s->len - s->pos;
        memcpy(s->user + s->pos, buffer, len);
        return len;
      }
    }
  return 0;
}

// Start processing the next available incoming packet
// Returns the size of the packet in bytes, or 0 if no packets are available
int
UIPUDP::parsePacket()
{
  if (_uip_udp_conn)
    {
      UIPEthernet.tick();
      uip_udp_appstate_t *s = &_uip_udp_conn->appstate;
      return s->len;
    }
  return 0;
}

// Number of bytes remaining in the current packet
int
UIPUDP::available()
{
  if (_uip_udp_conn)
    {
      UIPEthernet.tick();
      uip_udp_appstate_t *s = &_uip_udp_conn->appstate;
      return s->len - s->pos;
    }
  return 0;
}

// Read a single byte from the current packet
int
UIPUDP::read()
{
  if (_uip_udp_conn)
    {
      UIPEthernet.tick();
      uip_udp_appstate_t *s = &_uip_udp_conn->appstate;
      if (s->pos < s->len && s->user)
        {
          return s->user[s->pos++];
        }
    }
  return -1;
}

// Read up to len bytes from the current packet and place them into buffer
// Returns the number of bytes read, or 0 if none are available
int
UIPUDP::read(unsigned char* buffer, size_t len)
{
  if (_uip_udp_conn)
    {
      UIPEthernet.tick();
      uip_udp_appstate_t *s = &_uip_udp_conn->appstate;
      if (s->user) {
        size_t retlen = len < s->len - s->pos ? retlen : s->len - s->pos;
        memcpy(buffer, s->user + s->pos, retlen);
        return retlen;
      }
    }
  return 0;
}

// Return the next byte from the current packet without moving on to the next byte
int
UIPUDP::peek()
{
  if (_uip_udp_conn)
    {
      UIPEthernet.tick();
      uip_udp_appstate_t *s = &_uip_udp_conn->appstate;
      if (s->pos < s->len && s->user)
        {
          return s->user[s->pos];
        }
    }
  return -1;
}

// Finish reading the current packet
void
UIPUDP::flush()
{
  if (_uip_udp_conn)
    {
      uip_udp_appstate_t *s = &_uip_udp_conn->appstate;
      if (s->user)
        {
          free(s->user);
        }
      s->pos = s->len = 0;
    }
}

// Return the IP address of the host who sent the current incoming packet
IPAddress
UIPUDP::remoteIP()
{
  if (_uip_udp_conn)
    {
      uint16_t *a = (uint16_t *) &_uip_udp_conn->ripaddr;
      return ip_addr_uip(a);
    }
  return IPAddress((uint32_t)0);
}

// Return the port of the host who sent the current incoming packet
uint16_t
UIPUDP::remotePort()
{
  if (_uip_udp_conn)
    {
      return _uip_udp_conn->rport;
    }
  return 0;
}

void UIPUDP::uip_callback(uip_udp_appstate_t *s) {
  if (uip_newdata()) {
    if (!s->user) { //TODO what if there's allready data present?
      s->user=(uint8_t*)malloc(uip_datalen()); //TODO is this safe?
      s->len=uip_datalen();
      s->pos=0;
      memcpy(s->user,uip_appdata,uip_datalen());
    }
  }
}
