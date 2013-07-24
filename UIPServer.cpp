/*
 UIPServer.cpp - Arduino implementation of a uIP wrapper class.
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
#include "UIPEthernet.h"
#include "UIPServer.h"
extern "C" {
  #include "uip-conf.h"
}

UIPServer::UIPServer(uint16_t port) : _port(HTONS(port)) {

}

UIPClient UIPServer::available() {
  UIPEthernet.tick();
  for (int sock = 0; sock < UIP_CONNS; sock++) {
    struct uip_conn* conn = &uip_conns[sock];
    if (conn->lport == _port) {
      if (UIPClient::_available(conn)) {
        return UIPClient(conn);
      }
    }
  }
  return UIPClient();
}

void UIPServer::begin() {
  uip_listen(_port);
  UIPEthernet.tick();
}

size_t UIPServer::write(uint8_t c) {
  size_t ret = 0;
  for (int sock = 0; sock < UIP_CONNS; sock++) {
    struct uip_conn* conn = &uip_conns[sock];
    if (conn->lport == _port && (conn->tcpstateflags != UIP_CLOSE)) {
      ret = UIPClient::_write(conn,c);
    }
  }
  return ret;
}

size_t UIPServer::write(const uint8_t *buf, size_t size) {
  size_t ret = 0;
  for (int sock = 0; sock < UIP_CONNS; sock++) {
    struct uip_conn* conn = &uip_conns[sock];
    if (conn->lport == _port && (conn->tcpstateflags != UIP_CLOSE)) {
      ret = UIPClient::_write(conn,buf,size);
    }
  }
  return ret;
}
