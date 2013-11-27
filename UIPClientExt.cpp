/*
 UIPClientExt.cpp - Arduino implementation of a uIP wrapper class.
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

UIPClientExt::UIPClientExt() :
    conn(NULL)
{
  UIPClient();
}

int
UIPClientExt::connectNB(IPAddress ip, uint16_t port)
{
    stop();
    uip_ipaddr_t ipaddr;
    uip_ip_addr(ipaddr, ip);
    conn = uip_connect(&ipaddr, htons(port));
    return conn ? 1 : 0;
}

void
UIPClientExt::stop()
{
  if (!data && conn)
    conn->tcpstateflags = UIP_CLOSED;
  else
    {
      conn = NULL;
      UIPClient::stop();
    }
}

uint8_t
UIPClientExt::connected()
{
  return this ? UIPClient::connected() : 0;
}

UIPClientExt::operator bool()
{
  UIPEthernetClass::tick();
  if (conn && !data && (conn->tcpstateflags & UIP_TS_MASK) != UIP_CLOSED)
    {
      if ((conn->tcpstateflags & UIP_TS_MASK) == UIP_ESTABLISHED)
        {
          data = (uip_userdata_t*) conn->appstate;
#ifdef UIPETHERNET_DEBUG_CLIENT
          Serial.print(F("connected, state: "));
          Serial.print(data->state);
          Serial.print(F(", first packet in: "));
          Serial.println(data->packets_in[0]);
#endif
        }
      return true;
    }
  return data && (!(data->state & UIP_CLIENT_REMOTECLOSED) || data->packets_in[0] != NOBLOCK);
}

uint16_t
UIPClientExt::localPort() {
  if (!conn) return 0;
  return htons(conn->lport);
}

IPAddress
UIPClientExt::remoteIP() {
  if (!conn) return IPAddress();
  return ip_addr_uip(conn->ripaddr);
}

uint16_t
UIPClientExt::remotePort() {
  if (!conn) return 0;
  return htons(conn->rport);
}


