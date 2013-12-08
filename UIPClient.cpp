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
#import "utility/uip-conf.h"
#import "utility/uip.h"
#import "utility/uip_arp.h"
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
            {
#ifdef UIPETHERNET_DEBUG_CLIENT
              Serial.print("connected, state: ");
              Serial.print(((uip_userdata_t *)_uip_conn->appstate.user)->state);
              Serial.print(", first packet in: ");
              Serial.println(((uip_userdata_t *)_uip_conn->appstate.user)->packets_in[0]);
#endif
              return 1;
            }
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
  if (data)
    {
      _flushBlocks(&data->packets_in[0]);
      if (data->state & UIP_CLIENT_CLOSED)
        {
          uip_userdata_closed_t** cc = &UIPClient::closed_conns[0];
          for (uint8_t i = 0; i < UIP_CONNS; i++)
            {
              if (*cc == (void*)data)
                {
                  *cc = NULL;
                  break;
                }
              cc++;
            }
          free(data);
        }
      else
        {
          data->state |= UIP_CLIENT_CLOSE;
        }
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
      && (!(data->state & UIP_CLIENT_CLOSED) || data->packets_in[0] != NOBLOCK);
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
  int remain = size;
  uint16_t written;
#if UIP_ATTEMPTS_ON_WRITE > 0
  uint16_t attempts = UIP_ATTEMPTS_ON_WRITE;
#endif
  repeat:
  UIPEthernet.tick();
  if (conn && (u = (uip_userdata_t *)conn->appstate.user) && !(u->state & (UIP_CLIENT_CLOSE | UIP_CLIENT_CLOSED)))
    {
      memhandle* p = _currentBlock(&u->packets_out[0]);
      if (*p == NOBLOCK)
        {
newpacket:
          *p = UIPEthernet.network.allocBlock(UIP_SOCKET_DATALEN);
          if (*p == NOBLOCK)
            {
#if UIP_ATTEMPTS_ON_WRITE > 0
              if ((--attempts)>0)
#endif
#if UIP_ATTEMPTS_ON_WRITE != 0
                goto repeat;
#endif
              goto ready;
            }
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
          if (p==&u->packets_out[UIP_SOCKET_NUMPACKETS-1])
            {
#if UIP_ATTEMPTS_ON_WRITE > 0
              if ((--attempts)>0)
#endif
#if UIP_ATTEMPTS_ON_WRITE != 0
                goto repeat;
#endif
              goto ready;
            }
          p++;
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
  memhandle* p = &u->packets_in[0];
  if (*p == NOBLOCK)
    return 0;
  int len = 0;
  for(memhandle* end = p+UIP_SOCKET_NUMPACKETS; p < end; p++)
    {
      if(*p == NOBLOCK)
        break;
      len += UIPEthernet.network.blockSize(*p);
    }
  return len;
}

int
UIPClient::read(uint8_t *buf, size_t size)
{
  if (*this)
    {
      int remain = size;
      memhandle* p = &data->packets_in[0];
      if (*p == NOBLOCK)
        return 0;
      int read;
      do
        {
          read = UIPEthernet.network.readPacket(*p,0,buf+size-remain,remain);
          if (read == UIPEthernet.network.blockSize(*p))
            {
              remain -= read;
              _eatBlock(p);
              if (_uip_conn && uip_stopped(_uip_conn) && !(data->state & (UIP_CLIENT_CLOSE | UIP_CLIENT_CLOSED)))
                data->state |= UIP_CLIENT_RESTART;
              if (*p == NOBLOCK)
                return size-remain;
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
      memhandle p = data->packets_in[0];
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
      _flushBlocks(&data->packets_in[0]);
    }
}

void
UIPClient::uip_callback(uip_tcp_appstate_t *s)
{
  uip_userdata_t *u = (uip_userdata_t *) s->user;
  if (!u && uip_connected())
    {
#ifdef UIPETHERNET_DEBUG_CLIENT
      Serial.println("UIPClient uip_connected");
#endif
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
                  memhandle* p = _currentBlock(&u->packets_in[0]);
                  //if it's not the first packet
                  if (*p != NOBLOCK)
                    {
                      uint8_t slot = p - &u->packets_in[0];
                      if (slot < UIP_SOCKET_NUMPACKETS-1)
                        p++;
                      //if this is the last slot stop this connection
                      if (slot >= UIP_SOCKET_NUMPACKETS-2)
                        {
                          uip_stop();
                          //if there's no free slot left omit loosing this packet and (again) stop this connection
                          if (slot == UIP_SOCKET_NUMPACKETS-1)
                            goto reject_newdata;
                        }
                    }
                  UIPEthernet.network.copyPacket(newPacket,0,UIPEthernet.in_packet,((uint8_t*)uip_appdata)-uip_buf,uip_len);
                  *p = newPacket;
                  goto finish_newdata;
                }
reject_newdata:
              UIPEthernet.packetstate &= ~UIPETHERNET_FREEPACKET;
              uip_stop();
            }
        }
finish_newdata:
      if (u->state & UIP_CLIENT_RESTART)
        {
          u->state &= ~UIP_CLIENT_RESTART;
          uip_restart();
        }
      // If the connection has been closed, save received but unread data.
      if (uip_closed() || uip_timedout())
        {
#ifdef UIPETHERNET_DEBUG_CLIENT
          Serial.println("UIPClient uip_closed");
#endif
          // drop outgoing packets not sent yet:
          _flushBlocks(&u->packets_out[0]);
          if (u->packets_in[0] != NOBLOCK)
            {
              uip_userdata_closed_t** closed_conn_data = &UIPClient::closed_conns[0];
              for (uip_socket_ptr i = 0; i < UIP_CONNS; i++)
                {
                  if (!*closed_conn_data)
                    {
                      *closed_conn_data = (uip_userdata_closed_t*)u;
                      (*closed_conn_data)->lport = uip_conn->lport;
                      break;
                    }
                  closed_conn_data++;
                }
            }
          u->state |= UIP_CLIENT_CLOSED;
          // disassociate appdata.
          s->user = NULL;
          goto nodata;
        }
      if (uip_acked())
        {
#ifdef UIPETHERNET_DEBUG_CLIENT
          Serial.println("UIPClient uip_acked");
#endif
          _eatBlock(&u->packets_out[0]);
        }
      if (uip_poll() || uip_rexmit())
        {
#ifdef UIPETHERNET_DEBUG_CLIENT
          Serial.println("UIPClient uip_poll");
#endif
          memhandle p = u->packets_out[0];
          if (p != NOBLOCK)
            {
              if (u->packets_out[1] == NOBLOCK)
                {
                  uip_len = u->out_pos;
                  if (uip_len > 0)
                    {
                      UIPEthernet.network.resizeBlock(p,0,uip_len);
                    }
                }
              else
                uip_len = UIPEthernet.network.blockSize(p);
              if (uip_len > 0)
                {
                  UIPEthernet.uip_hdrlen = ((uint8_t*)uip_appdata)-uip_buf;
                  UIPEthernet.uip_packet = UIPEthernet.network.allocBlock(UIPEthernet.uip_hdrlen+uip_len);
                  if (UIPEthernet.uip_packet != NOBLOCK)
                    {
                      UIPEthernet.network.copyPacket(UIPEthernet.uip_packet,UIPEthernet.uip_hdrlen,p,0,uip_len);
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
#ifdef UIPETHERNET_DEBUG_CLIENT
              Serial.print("UIPClient state UIP_CLIENT_CLOSE");
#endif
          if (u->packets_out[0] == NOBLOCK)
            {
#ifdef UIPETHERNET_DEBUG_CLIENT
              Serial.print("UIPClient state UIP_CLIENT_CLOSE -> free userdata");
#endif
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

memhandle*
UIPClient::_currentBlock(memhandle* block)
{
  for(memhandle* end = block+UIP_SOCKET_NUMPACKETS-1; block < end; block++)
    if(*(block+1) == NOBLOCK)
      break;
  return block;
}

void
UIPClient::_eatBlock(memhandle* block)
{
#ifdef UIPETHERNET_DEBUG_CLIENT
  memhandle* start = block;
  Serial.print("eatblock(");
  Serial.print(*block);
  Serial.print("): ");
  for (uint8_t i = 0; i < UIP_SOCKET_NUMPACKETS; i++)
    {
      Serial.print(start[i]);
      Serial.print(" ");
    }
  Serial.print("-> ");
#endif
  memhandle* end = block+(UIP_SOCKET_NUMPACKETS-1);
  UIPEthernet.network.freeBlock(*block);
  while(block < end)
    *block = *((block++)+1);
  *end = NOBLOCK;
#ifdef UIPETHERNET_DEBUG_CLIENT
  for (uint8_t i = 0; i < UIP_SOCKET_NUMPACKETS; i++)
    {
      Serial.print(start[i]);
      Serial.print(" ");
    }
  Serial.println();
#endif
}

void
UIPClient::_flushBlocks(memhandle* block)
{
  for(memhandle* end = block+UIP_SOCKET_NUMPACKETS; block < end; block++)
    {
      if(*block != NOBLOCK)
        {
          UIPEthernet.network.freeBlock(*block);
          *block = NOBLOCK;
        }
      else
        break;
    }
}

