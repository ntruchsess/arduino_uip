/*
 UIPClient.cpp - Arduino implementation of a uIP wrapper class.
 Copyright (c) 2013 Norbert Truchsess <norbert.truchsess@t-online.de>
 All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

extern "C"
{
#import "uip-conf.h"
#import "uip.h"
#import "string.h"
}
#include "UIPClient.h"
#include "UIPEthernet.h"
#include "Dns.h"

#define UIP_TCP_PHYH_LEN UIP_LLH_LEN+UIP_IPTCPH_LEN
#define BUF ((struct uip_tcpip_hdr *)&uip_buf[UIP_LLH_LEN])

UIPClient::UIPClient() :
    _uip_conn(NULL)
{
}

UIPClient::UIPClient(struct uip_conn* conn) :
    _uip_conn(conn)
{
}

int
UIPClient::connect(IPAddress ip, uint16_t port)
{
  uip_ipaddr_t ipaddr;
  uip_ip_addr(ipaddr, ip);
  _uip_conn = uip_connect(&ipaddr, htons(port));
  return _uip_conn ? 1 : 0;
}

int
UIPClient::connect(const char *host, uint16_t port)
{
  // Look up the host first
  int ret = 0;
  DNSClient dns;
  IPAddress remote_addr;

  dns.begin(UIPEthernet.dnsServerIP());
  ret = dns.getHostByName(host, remote_addr);
  if (ret == 1) {
    return connect(remote_addr, port);
  } else {
    return ret;
  }
}

void
UIPClient::stop()
{
  uip_userdata_t *u;
  if (_uip_conn && (u = (uip_userdata_t *)_uip_conn->appstate.user))
    {
      u->close = true;
    }
  _uip_conn = NULL;
  UIPEthernet.tick();
}

uint8_t
UIPClient::connected()
{
  return (_uip_conn && (_uip_conn->tcpstateflags != UIP_CLOSE || available())) ? 1 : 0;
}

UIPClient::operator bool()
{
  UIPEthernet.tick();
  return _uip_conn && _uip_conn->appstate.user;
}

size_t
UIPClient::write(uint8_t c)
{
  return _write(_uip_conn, &c, 1);
}

size_t
UIPClient::write(const uint8_t *buf, size_t size)
{
  return _write(_uip_conn, buf, size);
}

size_t
UIPClient::_write(struct uip_conn* conn, const uint8_t *buf, size_t size)
{
  uip_userdata_t *u;
  UIPEthernet.tick();
  if (conn && (u = (uip_userdata_t *)conn->appstate.user))
    {
      int remain = size;
      uint16_t written;
      memhandle* p = &u->packets_out[u->packet_out_end];
      if (*p == NOBLOCK)
        {
newpacket:
          *p = UIPEthernet.network.newPacket(UIP_SOCKET_DATALEN);
          if (*p == NOBLOCK)
            goto ready;
          u->out_pos = 0;
        }
      written = UIPEthernet.network.writePacket(*p,u->out_pos,(uint8_t*)buf+size-remain,remain);
      remain -= written;
      u->out_pos+=written;
      if (remain > 0)
        {
          uint8_t next = u->packet_out_end == UIP_SOCKET_NUMPACKETS-1 ? 0 : u->packet_out_end+1;
          if (next == u->packet_out_start)
            goto ready;
          u->packet_out_end = next;
          p = &u->packets_out[next];
          goto newpacket;
        }
ready:
      return size-remain;
    }
  return -1;
}

int
UIPClient::available()
{
  return _available(_uip_conn);
}

int
UIPClient::_available(struct uip_conn* conn)
{
  uip_userdata_t *u;
  UIPEthernet.tick();
  if (conn && (u = (uip_userdata_t *)conn->appstate.user))
    {
      uint8_t i = u->packet_in_start;
      if (u->packets_in[i] == NOBLOCK)
        return 0;
      int len = 0;
      do
        {
          len += UIPEthernet.network.packetLen(u->packets_in[i]);
          if (i != u->packet_in_end)
            break;
          i = i == UIP_SOCKET_NUMPACKETS-1 ? 0 : i+1;
        }
      while (true);
      return len;
    }
  return -1;
}

int
UIPClient::read(uint8_t *buf, size_t size)
{
  uip_userdata_t *u;
  UIPEthernet.tick();
  if (_uip_conn && (u = (uip_userdata_t *)_uip_conn->appstate.user))
    {
      int remain = size;
      memhandle* p = &u->packets_in[u->packet_in_start];
      if (*p == NOBLOCK)
        return 0;
      int read;
      do
        {
          read = UIPEthernet.network.readPacket(*p,0,buf+size-remain,remain);
          if (read == UIPEthernet.network.packetLen(*p))
            {
              remain -= read;
              UIPEthernet.network.freeBlock(*p);
              *p = NOBLOCK;
              if (u->packet_in_start == u->packet_in_end)
                return size-remain;
              u->packet_in_start = u->packet_in_start == UIP_SOCKET_NUMPACKETS-1 ? 0 : u->packet_in_start+1;
              p = &u->packets_in[u->packet_in_start];
            }
          else
            {
              UIPEthernet.network.resizePacket(*p,read);
              break;
            }
        }
      while(remain > 0);
      return size;
    }
  return -1;
}

int
UIPClient::read()
{
  uint8_t c;
  if (read(&c,1) < 0)
    return -1;
  return c;
}

int
UIPClient::peek()
{
  uip_userdata_t *u;
  UIPEthernet.tick();
  if (_uip_conn && (u = (uip_userdata_t *)_uip_conn->appstate.user))
    {
      memhandle p = u->packets_in[u->packet_in_start];
      if (p != NOBLOCK)
        {
          uint8_t c;
          UIPEthernet.network.readPacket(p,0,&c,1);
          return c;
        }
    }
  return -1;
}

void
UIPClient::flush()
{
  uip_userdata_t *u;
  UIPEthernet.tick();
  if (_uip_conn && (u = (uip_userdata_t *)_uip_conn->appstate.user) && u->packets_out[u->packet_out_start] != NOBLOCK)
    {
      uip_poll_conn(_uip_conn);
    }
}

void
UIPClient::uip_callback(uip_tcp_appstate_t *s)
{
  // If the connection has been closed, release the data we allocated earlier.
  if (uip_closed())
    {
      if (s->user)
        free(s->user);
      s->user = NULL;
      return;
    }

  if (uip_connected())
    {

      // We want to store some data in our connections, so allocate some space
      // for it.  The connection_data struct is defined in a separate .h file,
      // due to the way the Arduino IDE works.  (typedefs come after function
      // definitions.)

      uip_userdata_t *data = (uip_userdata_t*) malloc(sizeof(uip_userdata_t));

      memset(data,0,sizeof(uip_userdata_t));

      s->user = data;

    }

  if (uip_userdata_t *u = (uip_userdata_t *) s->user)
    {
      if (u->close)
        uip_close();
      if (uip_newdata())
        {
          uint8_t i = u->packet_in_end;
          if (u->packets_in[i] != NOBLOCK)
            {
              i = i == UIP_SOCKET_NUMPACKETS-1 ? 0 : i+1;
              if (i == u->packet_in_start)
                {
                  UIPEthernet.freepacket = false;
                  goto finish_newdata;
                }
            }
          uint16_t hdrlen = ((uint8_t*)uip_appdata)-uip_buf;
          UIPEthernet.network.resizePacket(UIPEthernet.in_packet,hdrlen,uip_len);
          u->packets_in[i] = UIPEthernet.in_packet;
          UIPEthernet.in_packet = NOBLOCK;
          u->packet_in_end = i;
        }
finish_newdata:
      if (uip_acked())
        {
          memhandle *p = &u->packets_out[u->packet_out_start];
          if (*p != NOBLOCK)
            {
              UIPEthernet.network.freeBlock(*p);
              *p = NOBLOCK;
              u->packet_out_start = u->packet_out_start == UIP_SOCKET_NUMPACKETS-1 ? 0 : u->packet_out_start+1;
            }
        }
      if (uip_poll() || uip_rexmit())
        {
          memhandle p = u->packets_out[u->packet_out_start];
          if (p != NOBLOCK)
            {
              if (u->packet_out_start == u->packet_out_end)
                {
                  uip_len = u->out_pos;
                  UIPEthernet.network.resizePacket(p,0,uip_len);
                  u->packet_out_end = u->packet_out_end == UIP_SOCKET_NUMPACKETS-1 ? 0 : u->packet_out_end+1;
                }
              else
                uip_len = UIPEthernet.network.packetLen(p);
              if (uip_len > 0)
                {
                  UIPEthernet.network.readPacket(p,0,(uint8_t*)uip_appdata,uip_len);
                  uip_send(uip_appdata,uip_len);
                }
            }
        }
    }
}
