/*
 UIPClient.h - Arduino implementation of a uIP wrapper class.
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

#ifndef UIPCLIENT_H
#define UIPCLIENT_H

#include "ethernet_comp.h"
#import "Client.h"
#import "utility/mempool.h"

extern "C" {
  #import "utility/uip.h"
}

#define UIP_SOCKET_DATALEN UIP_TCP_MSS
//#define UIP_SOCKET_NUMPACKETS UIP_RECEIVE_WINDOW/UIP_TCP_MSS+1
#define UIP_SOCKET_NUMPACKETS 5

#define UIP_CLIENT_CLOSE 1
#define UIP_CLIENT_CLOSED 2
#define UIP_CLIENT_RESTART 4

typedef uint8_t uip_socket_ptr;

typedef struct {
  uint8_t state;
  memhandle packets_in[UIP_SOCKET_NUMPACKETS];
  uip_socket_ptr packet_in_start;
  uip_socket_ptr packet_in_end;
  uint16_t lport;        /**< The local TCP port, in network byte order. */
} uip_userdata_closed_t;

typedef struct {
  uint8_t state;
  memhandle packets_in[UIP_SOCKET_NUMPACKETS];
  uip_socket_ptr packet_in_start;
  uip_socket_ptr packet_in_end;
  memhandle packets_out[UIP_SOCKET_NUMPACKETS];
  uip_socket_ptr packet_out_start;
  uip_socket_ptr packet_out_end;
  memaddress out_pos;
} uip_userdata_t;

class UIPClient : public Client {

public:
  UIPClient();
  int connect(IPAddress ip, uint16_t port);
  int connect(const char *host, uint16_t port);
  int read(uint8_t *buf, size_t size);
  void stop();
  uint8_t connected();
  operator bool();

  size_t write(uint8_t);
  size_t write(const uint8_t *buf, size_t size);
  int available();
  int read();
  int peek();
  void flush();

private:
  UIPClient(struct uip_conn *_conn);
  UIPClient(uip_userdata_closed_t* closed_conn);

  struct uip_conn *_uip_conn;

  uip_userdata_t* data;

  static uip_userdata_closed_t* closed_conns[UIP_CONNS];

  static size_t _write(struct uip_conn*,const uint8_t *buf, size_t size);
  static int _available(uip_userdata_t *);

  friend class UIPEthernetClass;
  friend class UIPServer;
  static void uip_callback(uip_tcp_appstate_t *s);
};

#endif
