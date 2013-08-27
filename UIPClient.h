/*
 UIPClient.h - Arduino implementation of a uIP wrapper class.
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

#ifndef UIPCLIENT_H
#define UIPCLIENT_H

#import "Client.h"
extern "C" {
  #import "utility/uip.h"
  #import "utility/psock.h"
}

#define UIP_SOCKET_BUFFER_SIZE 64
typedef uint8_t uip_socket_ptr;

typedef struct uip_userdata {
  uip_socket_ptr in_pos;
  uip_socket_ptr in_len;
  uip_socket_ptr out_pos;
  uip_socket_ptr out_len;
  uint8_t in_buffer[UIP_SOCKET_BUFFER_SIZE];
  uint8_t out_buffer[UIP_SOCKET_BUFFER_SIZE];
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

  static size_t _write(struct uip_conn*,uint8_t);
  static size_t _write(struct uip_conn*,const uint8_t *buf, size_t size);
  static int _available(struct uip_conn*);

  static bool readyToReceive(uip_tcp_appstate_t *s);
  static void dataReceived(uip_tcp_appstate_t *s);
  static bool readyToSend(uip_tcp_appstate_t *s);
  static void dataSent(uip_tcp_appstate_t *s);
  static bool isClosed(uip_tcp_appstate_t *s);

  static int handle_connection(uip_tcp_appstate_t *s);

  friend class UIPServer;
  friend class Enc28J60IPStack;

};

#endif
