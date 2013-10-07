/*
 UIPServer.cpp - Arduino implementation of a uIP wrapper class.
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
#include "UIPEthernet.h"
#include "UIPServer.h"
extern "C" {
  #include "uip-conf.h"
}

UIPServer::UIPServer(uint16_t port) : _port(htons(port))
{
  UIPEthernet.set_uip_callback(&UIPClient::uip_callback);
}

UIPClient UIPServer::available()
{
  UIPEthernet.tick();
  uip_userdata_t* u;
  for (uint8_t sock = 0; sock < UIP_CONNS; sock++)
    {
      struct uip_conn* conn = &uip_conns[sock];
      if (conn->lport == _port && (u = (uip_userdata_t*) conn->appstate.user))
        {
          if (UIPClient::_available(u))
            return UIPClient(conn);
        }
    }
  uip_userdata_closed_t** cc = &UIPClient::closed_conns[0];
  for (uint8_t i = 0; i < UIP_CONNS; i++)
    {
      if (*cc && (*cc)->lport == _port)
        {
          if ((*cc)->packets_in[(*cc)->packet_in_start] == NOBLOCK)
            {
              free(*cc);
              *cc = NULL;
            }
          else
            return UIPClient(*cc);
        }
      cc++;
    }
  return UIPClient();
}

void UIPServer::begin()
{
  uip_listen(_port);
  UIPEthernet.tick();
}

size_t UIPServer::write(uint8_t c)
{
  return write(&c,1);
}

size_t UIPServer::write(const uint8_t *buf, size_t size)
{
  size_t ret = 0;
  for (int sock = 0; sock < UIP_CONNS; sock++)
    {
      struct uip_conn* conn = &uip_conns[sock];
      if (conn->lport == _port && (conn->tcpstateflags != UIP_CLOSE))
        ret = UIPClient::_write(conn,buf,size);
    }
  return ret;
}
