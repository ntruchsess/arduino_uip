/*
 UIPEthernet.h - Arduino implementation of a uIP wrapper class.
 Copyright (c) 2013 Norbert Truchsess <norbert.truchsess@t-online.de>
 work based on
 SerialIP.h (Copyright (c) 2010 Adam Nielsen <malvineous@shikadi.net>)
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

#ifndef UIPETHERNET_H
#define UIPETHERNET_H

#include <Arduino.h>
#include "Dhcp.h"
#include "IPAddress.h"

extern "C"
{
#include "utility/timer.h"
#include "utility/uip.h"
}

#define uip_ip_addr(addr, ip) do { \
                     ((u16_t *)(addr))[0] = HTONS(((ip[0]) << 8) | (ip[1])); \
                     ((u16_t *)(addr))[1] = HTONS(((ip[2]) << 8) | (ip[3])); \
                  } while(0)

#define ip_addr_uip(a) IPAddress(a[0] >> 8, a[0] & 0xFF, a[1] >> 8 , a[1] & 0xFF); //TODO this is not IPV6 capable

#define uip_seteth_addr(eaddr) do {uip_ethaddr.addr[0] = eaddr[0]; \
                              uip_ethaddr.addr[1] = eaddr[1];\
                              uip_ethaddr.addr[2] = eaddr[2];\
                              uip_ethaddr.addr[3] = eaddr[3];\
                              uip_ethaddr.addr[4] = eaddr[4];\
                              uip_ethaddr.addr[5] = eaddr[5];} while(0)

typedef void
(*fn_uip_cb_t)(uip_tcp_appstate_t *conn);

typedef void
(*fn_uip_udp_cb_t)(uip_udp_appstate_t *conn);

class UIPEthernetClass
{
public:
  UIPEthernetClass();

  int begin(const uint8_t* mac);
  void begin(const uint8_t* mac, IPAddress ip);
  void begin(const uint8_t* mac, IPAddress ip, IPAddress dns);
  void begin(const uint8_t* mac, IPAddress ip, IPAddress dns, IPAddress gateway);
  void begin(const uint8_t* mac, IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet);

  // maintain() must be called at regular intervals to process the incoming serial
  // data and issue IP events to the sketch.  It does not return until all IP
  // events have been processed. Renews dhcp-lease if required.
  int maintain();

  IPAddress localIP();
  IPAddress subnetMask();
  IPAddress gatewayIP();
  IPAddress dnsServerIP();

  // Set a user function to handle raw uIP events as they happen.  The
  // callback function can only use uIP functions, but it can also use uIP's
  // protosockets.
  void set_uip_callback(fn_uip_cb_t fn);
  void set_uip_udp_callback(fn_uip_udp_cb_t fn);

private:
  IPAddress _dnsServerAddress;
  DhcpClass* _dhcp;

  struct timer periodic_timer;
  struct UIPEthernet_state *cur_conn; // current connection (for print etc.)
  fn_uip_cb_t fn_uip_cb;
  fn_uip_udp_cb_t fn_uip_udp_cb;

  void init(const uint8_t* mac);
  void configure(IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet);

  void tick();

  void uip_callback();

  friend void uipethernet_appcall(void);

  void uip_udp_callback();

  friend void uipudp_appcall(void);

  friend class UIPServer;

  friend class UIPClient;

  friend class UIPUDP;
};

extern UIPEthernetClass UIPEthernet;

#endif
