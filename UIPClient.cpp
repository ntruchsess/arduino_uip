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
#import "psock.h"
#import "uip.h"
}
#include "UIPClient.h"
#include "UIPEthernet.h"
#include "Dns.h"

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
  _uip_conn = uip_connect(&ipaddr, port);
  UIPEthernet.tick();
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

int
UIPClient::read(uint8_t *buf, size_t size)
{
  return 0; //TODO implement read to buffer
}

void
UIPClient::stop()
{
  uip_userdata_t *u;
  if (_uip_conn && (u = (uip_userdata_t *) (((uip_tcp_appstate_t) _uip_conn->appstate).user)))
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
  return _uip_conn;
}

size_t
UIPClient::write(uint8_t c)
{
  return _write(_uip_conn, c);
}

size_t
UIPClient::_write(struct uip_conn* conn, uint8_t c)
{
  uip_userdata_t *u;
  if (conn
      && (u = (uip_userdata_t *) (((uip_tcp_appstate_t) conn->appstate).user)))
    {
      if (u->out_pos == UIP_SOCKET_BUFFER_SIZE)
        {
          UIPEthernet.tick();
          if (u->out_pos == UIP_SOCKET_BUFFER_SIZE)
            {
              return 0;
            }
        }
      u->out_buffer[(u->out_pos)++] = c;
      if (u->out_pos == UIP_SOCKET_BUFFER_SIZE)
        {
          UIPEthernet.tick();
        }
      return 1;
    }
  return -1;
}

size_t
UIPClient::write(const uint8_t *buf, size_t size)
{
  return _write(_uip_conn, buf, size);
}

size_t
UIPClient::_write(struct uip_conn*, const uint8_t *buf, size_t size)
{
  return 0; //TODO implement this!
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
  if (conn && (u = (uip_userdata_t *) (((uip_tcp_appstate_t) conn->appstate).user)))
    {
      return u->in_len - u->in_pos;
    }
  return -1;
}

int
UIPClient::read()
{
  uip_userdata_t *u;
  UIPEthernet.tick();
  if (_uip_conn && (u = (uip_userdata_t *) (((uip_tcp_appstate_t) _uip_conn->appstate).user)))
    {
      if (u->in_len == u->in_pos)
        return -1;
      int ret = u->in_buffer[(u->in_pos)++];
      return ret;
    }
  return -1;
}

int
UIPClient::peek()
{
  uip_userdata_t *u;
  UIPEthernet.tick();
  if (_uip_conn && (u = (uip_userdata_t *) (((uip_tcp_appstate_t) _uip_conn->appstate).user)))
    {
      if (u->in_len == u->in_pos)
        return -1;
      return u->in_buffer[u->in_pos];
    }
  return -1;
}

void
UIPClient::flush()
{
  UIPEthernet.tick();
}

bool
UIPClient::readyToSend(uip_tcp_appstate_t *s)
{
  if (uip_userdata_t *u = (uip_userdata_t *) s->user)
    {
      if (u->out_len == 0 && u->out_pos > 0)
        {
          u->out_len = u->out_pos;
          return true;
        }
      return u->out_len > 0;
    }
  return 0;
}

void
UIPClient::dataSent(uip_tcp_appstate_t *s)
{
  if (uip_userdata_t *u = (uip_userdata_t *) s->user)
    {
      u->out_pos -= u->out_len;
      if (u->out_pos > 0)
        {
          memcpy(u->out_buffer, u->out_buffer + u->out_len, u->out_pos);
        }
      u->out_len = 0;
    }
}

bool
UIPClient::readyToReceive(uip_tcp_appstate_t *s)
{
  if (uip_userdata_t *u = (uip_userdata_t *) s->user)
    {
      u->in_len -= u->in_pos;
      if (u->in_len > 0 && u->in_pos > 0)
        {
          memcpy(u->in_buffer, u->in_buffer + u->in_pos, u->in_len);
        }
      u->in_pos = 0;
      psock *p = &s->p;
      p->bufptr = u->in_buffer + u->in_len;
      p->bufsize = UIP_SOCKET_BUFFER_SIZE - u->in_len;
      return p->bufsize > 0;
    }
  return 0;
}

void
UIPClient::dataReceived(uip_tcp_appstate_t *s)
{
  if (uip_userdata_t *u = (uip_userdata_t *) s->user)
    {
      u->in_len += PSOCK_DATALEN(&s->p);
    }
}

bool
UIPClient::isClosed(uip_tcp_appstate_t *s)
{
  if (uip_userdata_t *u = (uip_userdata_t *) s->user)
    {
      return u->close;
    }
  return false;
}

void
UIPClient::uip_callback(uip_tcp_appstate_t *s)
{
  if (uip_connected())
    {

      // We want to store some data in our connections, so allocate some space
      // for it.  The connection_data struct is defined in a separate .h file,
      // due to the way the Arduino IDE works.  (typedefs come after function
      // definitions.)

      uip_userdata_t *data = (uip_userdata_t*) malloc(sizeof(uip_userdata_t));

      data->in_len = 0;
      data->in_pos = 0;
      data->out_len = 0;
      data->out_pos = 0;
      data->close = false;

      s->user = data;

      // The protosocket's read functions need a per-connection buffer to store
      // data they read.  We've got some space for this in our connection_data
      // structure, so we'll tell uIP to use that.
      PSOCK_INIT(&s->p, data->in_buffer, sizeof(data->in_buffer));
    }

  // Call/resume our protosocket handler.
  handle_connection(s);

  // If the connection has been closed, release the data we allocated earlier.
  if (uip_closed())
    {
      if (s->user)
        free(s->user);
      s->user = NULL;
    }
}

// This function is going to use uIP's protosockets to handle the connection.
// This means it must return int, because of the way the protosockets work.
// In a nutshell, when a PSOCK_* macro needs to wait for something, it will
// return from handle_connection so that other work can take place.  When the
// event is triggered, uip_callback() will call this function again and the
// switch() statement (see below) will take care of resuming execution where
// it left off.  It *looks* like this function runs from start to finish, but
// that's just an illusion to make it easier to code :-)
int
UIPClient::handle_connection(uip_tcp_appstate_t *s)
{
  // All protosockets must start with this macro.  Its internal implementation
  // is that of a switch() statement, so all code between PSOCK_BEGIN and
  // PSOCK_END is actually inside a switch block.  (This means for example,
  // that you can't declare variables in the middle of it!)

  struct psock *p = &s->p;
  uip_userdata_t *u = (uip_userdata_t *) s->user;

  PSOCK_BEGIN(p);

    if (uip_newdata() && readyToReceive(s))
      {
        PSOCK_READBUF_LEN(p, 0); //TODO check what happens when there's more new data incoming than space left in UIPClients buffer (most likely this is just discarded, isn't it?)
        dataReceived(s);
      }

    if (readyToSend(s))
      {
        PSOCK_SEND(p, u->out_buffer, u->out_len);
        dataSent(s);
      }

    if (isClosed(s))
      {
        // Disconnect.
        PSOCK_CLOSE(p);
      }

    // All protosockets must end with this macro.  It closes the switch().
  PSOCK_END(p);
}

