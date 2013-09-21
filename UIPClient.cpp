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
      memhandle* packet = &u->packets_out[UIP_SOCKET_NUMPACKETS-1];
      uint8_t i = UIP_SOCKET_NUMPACKETS-1;
      uint16_t written;
searchpacket:
      if (*packet != NOBLOCK)
        {
          if (u->out_pos == UIP_SOCKET_DATALEN)
            {
              if (i == UIP_SOCKET_NUMPACKETS-1)
                goto ready;
              packet++;
              goto newpacket;
            }
          goto writepacket;
        }
      if (i > 0)
        {
          i--;
          packet--;
          goto searchpacket;
        }
newpacket:
      *packet = UIPEthernet.network.newPacket(UIP_SOCKET_DATALEN);
      if (*packet == NOBLOCK)
          goto ready;
      u->out_pos = 0;
writepacket:
      written = UIPEthernet.network.writePacket(*packet,u->out_pos,(uint8_t*)buf+size-remain,remain);
      remain -= written;
      u->out_pos+=written;
      if (remain > 0)
        {
          if (i == UIP_SOCKET_NUMPACKETS-1)
            goto ready;
          i++;
          packet++;
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
      memhandle* packet = &u->packets_in[0];
      if (*packet == NOBLOCK)
        return 0;
      int len = UIPEthernet.network.packetLen(*packet++) - u->in_pos;
      for (uint8_t i=1;i<UIP_SOCKET_NUMPACKETS;i++)
        {
          if (*packet == NOBLOCK)
            break;
          len += UIPEthernet.network.packetLen(*packet++);
        }
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
      memhandle* packet = &u->packets_in[0];
      uint8_t i = 0;
      memhandle *p;
      uint16_t read;
readloop:
      //packets left to read?
      if (*packet == NOBLOCK)
        {
          if (i == 0)
            return 0;
          u->packets_in[0] = NOBLOCK;
          goto ready;
        }
      read = UIPEthernet.network.readPacket(*packet,u->in_pos,buf+size-remain,remain);
      remain -= read;
      u->in_pos += read;
      //packet completely read?
      if (u->in_pos == UIPEthernet.network.packetLen(*packet))
        {
          u->in_pos = 0;
          UIPEthernet.network.freePacket(*packet++);
          i++;
          //more data requested (and further slots available to test)?
          if (remain > 0 && i < UIP_SOCKET_NUMPACKETS)
            goto readloop;
        }
      // first packet not completely read:
      if (i == 0)
        return read;
      p = &u->packets_in[0];
movehandles:
      if (i == UIP_SOCKET_NUMPACKETS || *packet == NOBLOCK)
        {
          *p = NOBLOCK;
          goto ready;
        }
      *p++ = *packet++;
      i++;
      goto movehandles;
ready:
      return size-remain;
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
      memhandle p = u->packets_in[0];
      if (p != NOBLOCK)
        {
          uint8_t c;
          UIPEthernet.network.readPacket(p,u->in_pos,&c,1);
          return c;
        }
    }
  return -1;
}

void
UIPClient::flush()
{
  UIPEthernet.tick();
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
      memhandle *packet;
      if (u->close)
        uip_close();
      if (uip_newdata())
        {
          packet = &u->packets_in[0];
          uint8_t i = 0;
          for (; i < UIP_SOCKET_NUMPACKETS; i++)
            {
              if (*packet == NOBLOCK)
                {
                  *packet = UIPEthernet.in_packet;
                  UIPEthernet.in_packet = NOBLOCK;
                  uint16_t hdrlen = ((uint8_t*)uip_appdata)-uip_buf;
                  UIPEthernet.network.resizePacket(*packet,hdrlen,uip_len-hdrlen);
                  break;
                }
              packet++;
            }
        }
      if (uip_acked())
        {
          packet = &u->packets_out[0];
          if (*packet != NOBLOCK)
            {
              UIPEthernet.network.freePacket(*packet);
              if (UIP_SOCKET_NUMPACKETS > 1)
                {
                  uint8_t i = 1;
                  memhandle* p = packet+1;
ackloop:
                  *packet = *p;
                  if (*packet == NOBLOCK)
                    goto ackready;
                  packet++;
                  if (i < UIP_SOCKET_NUMPACKETS-1)
                    {
                      i++;
                      p++;
                      goto ackloop;
                    }
                }
              *packet = NOBLOCK;
            }
        }
ackready:
      if (uip_poll() || uip_rexmit())
        {
          if (UIPEthernet.out_data == NOBLOCK)
            {
              packet = &u->packets_out[0];
              if (*packet != NOBLOCK)
                {
                  UIPEthernet.out_data = *packet;
                  UIPEthernet.hdrlen = UIP_TCP_PHYH_LEN;
                  uip_send(uip_appdata,(UIP_SOCKET_NUMPACKETS > 1 && *packet+1 != NOBLOCK) ? UIP_SOCKET_DATALEN : u->out_pos);
                }
            }
        }
    }
}
