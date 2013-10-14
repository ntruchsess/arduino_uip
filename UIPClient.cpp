/*
 UIPClient.cpp - Arduino implementation of a uIP wrapper class.
 Copyright (c) 2013 Norbert Truchsess <norbert.truchsess@t-online.de>
 All rights reserved.

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

extern "C"
{
#import "uip-conf.h"
#import "uip.h"
#import "uip_arp.h"
#import "string.h"
}
#include "UIPEthernet.h"
#include "UIPClient.h"
#include "Dns.h"

#ifdef UIPETHERNET_DEBUG_CLIENT
#include "HardwareSerial.h"
#endif

#define UIP_TCP_PHYH_LEN UIP_LLH_LEN+UIP_IPTCPH_LEN

uip_userdata_closed_t* UIPClient::closed_conns[UIP_CONNS];

UIPClient::UIPClient() :
    _uip_conn(NULL),
    data(NULL)
{
  UIPEthernet.set_uip_callback(&UIPClient::uip_callback);
}

UIPClient::UIPClient(struct uip_conn* conn) :
    _uip_conn(conn),
    data(conn ? (uip_userdata_t*)conn->appstate.user : NULL)
{
}

UIPClient::UIPClient(uip_userdata_closed_t* conn_data) :
    _uip_conn(NULL),
    data((uip_userdata_t*)conn_data)
{
}

int
UIPClient::connect(IPAddress ip, uint16_t port)
{
  uip_ipaddr_t ipaddr;
  uip_ip_addr(ipaddr, ip);
  _uip_conn = uip_connect(&ipaddr, htons(port));
  if (_uip_conn)
    {
      while((_uip_conn->tcpstateflags & UIP_TS_MASK) != UIP_CLOSED)
        {
          UIPEthernet.tick();
          if ((_uip_conn->tcpstateflags & UIP_TS_MASK) == UIP_ESTABLISHED)
            return 1;
        }
    }
  return 0;
}

int
UIPClient::connect(const char *host, uint16_t port)
{
  // Look up the host first
  int ret = 0;
#if UIP_UDP
  DNSClient dns;
  IPAddress remote_addr;

  dns.begin(UIPEthernet.dnsServerIP());
  ret = dns.getHostByName(host, remote_addr);
  if (ret == 1) {
    return connect(remote_addr, port);
  }
#endif
  return ret;
}

void
UIPClient::stop()
{
  if (*this)
    {
      data->state |= UIP_CLIENT_CLOSE;
      flush();
      data = NULL;
    }
  _uip_conn = NULL;
  UIPEthernet.tick();
}

uint8_t
UIPClient::connected()
{
  return ((_uip_conn && (_uip_conn->tcpstateflags & UIP_TS_MASK) == UIP_ESTABLISHED) || available() > 0) ? 1 : 0;
}

UIPClient::operator bool()
{
  UIPEthernet.tick();
  return (data || (_uip_conn && (data = (uip_userdata_t*)_uip_conn->appstate.user)))
      && (!(data->state & UIP_CLIENT_CLOSED) || data->packets_in[data->packet_in_start] != NOBLOCK);
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
  if (conn && (u = (uip_userdata_t *)conn->appstate.user) && !(u->state & (UIP_CLIENT_CLOSE | UIP_CLIENT_CLOSED)))
    {
      int remain = size;
      uint16_t written;
      memhandle* p = &u->packets_out[u->packet_out_end];
      if (*p == NOBLOCK)
        {
newpacket:
          *p = UIPEthernet.network.allocBlock(UIP_SOCKET_DATALEN);
          if (*p == NOBLOCK)
            goto ready;
          u->out_pos = 0;
        }
#ifdef UIPETHERNET_DEBUG_CLIENT
      Serial.print("UIPClient.write: writePacket(");
      Serial.print(*p);
      Serial.print(") pos: ");
      Serial.print(u->out_pos);
      Serial.print(", buf[");
      Serial.print(size-remain);
      Serial.print("-");
      Serial.print(remain);
      Serial.print("]: '");
      Serial.write((uint8_t*)buf+size-remain,remain);
      Serial.println("'");
#endif
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
  if (*this)
    return _available(data);
  return -1;
}

int
UIPClient::_available(uip_userdata_t *u)
{
  uint8_t i = u->packet_in_start;
  if (u->packets_in[i] == NOBLOCK)
    return 0;
  int len = 0;
  do
    {
      len += UIPEthernet.network.blockSize(u->packets_in[i]);
      if (i != u->packet_in_end)
        break;
      i = i == UIP_SOCKET_NUMPACKETS-1 ? 0 : i+1;
    }
  while (true);
  return len;
}

int
UIPClient::read(uint8_t *buf, size_t size)
{
  if (*this)
    {
      int remain = size;
      memhandle* p = &data->packets_in[data->packet_in_start];
      if (*p == NOBLOCK)
        return 0;
      int read;
      do
        {
          read = UIPEthernet.network.readPacket(*p,0,buf+size-remain,remain);
          if (read == UIPEthernet.network.blockSize(*p))
            {
              remain -= read;
              UIPEthernet.network.freeBlock(*p);
              *p = NOBLOCK;
              if (_uip_conn && uip_stopped(_uip_conn) && !(data->state & (UIP_CLIENT_CLOSE | UIP_CLIENT_CLOSED)))
                data->state |= UIP_CLIENT_RESTART;
              if (data->packet_in_start == data->packet_in_end)
                return size-remain;
              data->packet_in_start = data->packet_in_start == UIP_SOCKET_NUMPACKETS-1 ? 0 : data->packet_in_start+1;
              p = &data->packets_in[data->packet_in_start];
            }
          else
            {
              UIPEthernet.network.resizeBlock(*p,read);
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
  if (*this)
    {
      memhandle p = data->packets_in[data->packet_in_start];
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
  if (*this)
    {
      memhandle *p = &data->packets_in[0];
      for (uint8_t i = 0; i < UIP_SOCKET_NUMPACKETS; i++)
        {
          if (*p != NOBLOCK)
            {
              UIPEthernet.network.freeBlock(*p);
              *p = NOBLOCK;
            }
          p++;
        }
      data->packet_in_start = data->packet_in_end = 0;
    }
}

void
UIPClient::uip_callback(uip_tcp_appstate_t *s)
{
  uip_userdata_t *u = (uip_userdata_t *) s->user;
  if (!u && uip_connected())
    {
      // We want to store some data in our connections, so allocate some space
      // for it.  The connection_data struct is defined in a separate .h file,
      // due to the way the Arduino IDE works.  (typedefs come after function
      // definitions.)

      u = (uip_userdata_t*) malloc(sizeof(uip_userdata_t));
      if (u)
        {
          memset(u,0,sizeof(uip_userdata_t));
          s->user = u;
        }
    }
  if (u)
    {
      if (u->state & UIP_CLIENT_RESTART)
        {
          u->state &= ~UIP_CLIENT_RESTART;
          uip_restart();
        }
      if (uip_newdata())
        {
#ifdef UIPETHERNET_DEBUG_CLIENT
          Serial.print("UIPClient uip_newdata, uip_len:");
          Serial.println(uip_len);
#endif
          if (uip_len && !(u->state & (UIP_CLIENT_CLOSE | UIP_CLIENT_CLOSED)))
            {
              memhandle newPacket = UIPEthernet.network.allocBlock(uip_len);
              if (newPacket != NOBLOCK)
                {
                  uint8_t i = u->packet_in_end;
                  //if it's not the first packet
                  if (u->packets_in[i] != NOBLOCK)
                    {
                      i = i == UIP_SOCKET_NUMPACKETS-1 ? 0 : i+1;
                      //if this is the last slot stop this connection
                      if ((i == UIP_SOCKET_NUMPACKETS-1 ? 0 : i+1) == u->packet_in_start)
                        {
                          uip_stop();
                        }
                      //if there's no free slot left omit loosing this packet and (again) stop this connection
                      else if (i == u->packet_in_start)
                        goto reject_newdata;
                    }
                  UIPEthernet.network.copyPacket(newPacket,0,UIPEthernet.in_packet,((uint8_t*)uip_appdata)-uip_buf+UIP_INPACKETOFFSET,uip_len);
                  u->packets_in[i] = newPacket;
                  u->packet_in_end = i;
                  goto finish_newdata;
                }
reject_newdata:
              UIPEthernet.packetstate &= ~UIPETHERNET_FREEPACKET;
              uip_stop();
            }
        }
finish_newdata:
      // If the connection has been closed, save received but unread data.
      if (uip_closed() || uip_timedout())
        {
          // drop outgoing packets not sent yet:
          memhandle *p = &u->packets_out[0];
          for (uip_socket_ptr i = 0; i < UIP_SOCKET_NUMPACKETS; i++)
            {
              if (*p != NOBLOCK)
                {
                  UIPEthernet.network.freeBlock(*p);
                  *p = NOBLOCK;
                }
              p++;
            }
          uip_userdata_closed_t** closed_conn_data = &UIPClient::closed_conns[0];
          for (uip_socket_ptr i = 0; i < UIP_CONNS; i++)
            {
              if (!*closed_conn_data)
                {
                  *closed_conn_data = (uip_userdata_closed_t*)u;
                  (*closed_conn_data)->lport = uip_conn->lport;
                  (*closed_conn_data)->state |= UIP_CLIENT_CLOSED;
                  break;
                }
              closed_conn_data++;
            }
          // disassociate appdata.
          s->user = NULL;
          goto nodata;
        }
      if (uip_acked())
        {
#ifdef UIPETHERNET_DEBUG_CLIENT
          Serial.println("UIPClient uip_acked");
#endif
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
#ifdef UIPETHERNET_DEBUG_CLIENT
          Serial.println("UIPClient uip_poll");
#endif
          memhandle p = u->packets_out[u->packet_out_start];
          if (p != NOBLOCK)
            {
              if (u->packet_out_start == u->packet_out_end)
                {
                  uip_len = u->out_pos;
                  if (uip_len > 0)
                    {
                      UIPEthernet.network.resizeBlock(p,0,uip_len);
                      u->packet_out_end = u->packet_out_end == UIP_SOCKET_NUMPACKETS-1 ? 0 : u->packet_out_end+1;
                    }
                }
              else
                uip_len = UIPEthernet.network.blockSize(p);
              if (uip_len > 0)
                {
                  UIPEthernet.uip_hdrlen = ((uint8_t*)uip_appdata)-uip_buf;
                  UIPEthernet.uip_packet = UIPEthernet.network.allocBlock(UIPEthernet.uip_hdrlen+UIP_OUTPACKETOFFSET+uip_len);
                  if (UIPEthernet.uip_packet != NOBLOCK)
                    {
                      UIPEthernet.network.copyPacket(UIPEthernet.uip_packet,UIPEthernet.uip_hdrlen+UIP_OUTPACKETOFFSET,p,0,uip_len);
                      UIPEthernet.packetstate |= UIPETHERNET_SENDPACKET;
                      uip_send(uip_appdata,uip_len);
                    }
                  return;
                }
            }
        }
      // don't close connection unless all outgoing packets are sent
      if (u->state & UIP_CLIENT_CLOSE)
        {
          if (u->packets_out[u->packet_out_start] == NOBLOCK)
            {
              free(u);
              s->user = NULL;
              uip_close();
            }
          else
            uip_stop();
        }
    }
nodata:
  UIPEthernet.uip_packet = NOBLOCK;
  uip_len=0;
}
