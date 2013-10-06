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

typedef uint8_t uip_socket_ptr;

typedef struct uip_userdata {
  memaddress out_pos;
  memhandle packets_in[UIP_SOCKET_NUMPACKETS];
  memhandle packets_out[UIP_SOCKET_NUMPACKETS];
  uint8_t packet_in_start;
  uint8_t packet_in_end;
  uint8_t packet_out_start;
  uint8_t packet_out_end;
  bool close;
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
  static void uip_callback(uip_tcp_appstate_t *s);

private:
  UIPClient(struct uip_conn *_conn);

  struct uip_conn *_uip_conn;

  static size_t _write(struct uip_conn*,const uint8_t *buf, size_t size);
  static int _available(struct uip_conn*);

  friend class UIPServer;
};

#endif
