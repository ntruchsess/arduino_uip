/*
 UIPEthernet.cpp - Arduino implementation of a uIP wrapper class.
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

#include <Arduino.h>
#include "UIPEthernet.h"
#include "Enc28J60Network.h"

#if(defined UIPETHERNET_DEBUG || defined UIPETHERNET_DEBUG_CHKSUM)
#include "HardwareSerial.h"
#endif

extern "C"
{
#include "uip-conf.h"
#include "utility/uip.h"
#include "utility/uip_arp.h"
#include "utility/uip_timer.h"
}

#define ETH_HDR ((struct uip_eth_hdr *)&uip_buf[0])

// Because uIP isn't encapsulated within a class we have to use global
// variables, so we can only have one TCP/IP stack per program.

UIPEthernetClass::UIPEthernetClass() :
    fn_uip_cb(NULL),
    fn_uip_udp_cb(NULL),
    in_packet(NOBLOCK),
    uip_packet(NOBLOCK),
    uip_hdrlen(0),
    packetstate(0),
    _dhcp(NULL)
{
}

int
UIPEthernetClass::begin(const uint8_t* mac)
{
  static DhcpClass s_dhcp;
  _dhcp = &s_dhcp;

  // Initialise the basic info
  init(mac);

  // Now try to get our config info from a DHCP server
  int ret = _dhcp->beginWithDHCP((uint8_t*)mac);
  if(ret == 1)
  {
    // We've successfully found a DHCP server and got our configuration info, so set things
    // accordingly
    configure(_dhcp->getLocalIp(),_dhcp->getDnsServerIp(),_dhcp->getGatewayIp(),_dhcp->getSubnetMask());
  }
  return ret;
}

void
UIPEthernetClass::begin(const uint8_t* mac, IPAddress ip)
{
  IPAddress dns = ip;
  dns[3] = 1;
  begin(mac, ip, dns);
}

void
UIPEthernetClass::begin(const uint8_t* mac, IPAddress ip, IPAddress dns)
{
  IPAddress gateway = ip;
  gateway[3] = 1;
  begin(mac, ip, dns, gateway);
}

void
UIPEthernetClass::begin(const uint8_t* mac, IPAddress ip, IPAddress dns, IPAddress gateway)
{
  IPAddress subnet(255, 255, 255, 0);
  begin(mac, ip, dns, gateway, subnet);
}

void
UIPEthernetClass::begin(const uint8_t* mac, IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet)
{
  init(mac);
  configure(ip,dns,gateway,subnet);
}

int UIPEthernetClass::maintain(){
  tick();
  int rc = DHCP_CHECK_NONE;
  if(_dhcp != NULL){
    //we have a pointer to dhcp, use it
    rc = _dhcp->checkLease();
    switch ( rc ){
      case DHCP_CHECK_NONE:
        //nothing done
        break;
      case DHCP_CHECK_RENEW_OK:
      case DHCP_CHECK_REBIND_OK:
        //we might have got a new IP.
        configure(_dhcp->getLocalIp(),_dhcp->getDnsServerIp(),_dhcp->getGatewayIp(),_dhcp->getSubnetMask());
        break;
      default:
        //this is actually a error, it will retry though
        break;
    }
  }
  return rc;
}

IPAddress UIPEthernetClass::localIP()
{
  IPAddress ret;
  uip_ipaddr_t a;
  uip_gethostaddr(a);
  return ip_addr_uip(a);
}

IPAddress UIPEthernetClass::subnetMask()
{
  IPAddress ret;
  uip_ipaddr_t a;
  uip_getnetmask(a);
  return ip_addr_uip(a);
}

IPAddress UIPEthernetClass::gatewayIP()
{
  IPAddress ret;
  uip_ipaddr_t a;
  uip_getdraddr(a);
  return ip_addr_uip(a);
}

IPAddress UIPEthernetClass::dnsServerIP()
{
  return _dnsServerAddress;
}

void
UIPEthernetClass::tick()
{
  if (in_packet == NOBLOCK)
    {
      in_packet = network.receivePacket();
#ifdef UIPETHERNET_DEBUG
      if (in_packet != NOBLOCK)
        {
          Serial.print("--------------\nreceivePacket: ");
          Serial.println(in_packet);
        }
#endif
    }
  if (in_packet != NOBLOCK)
    {
      uip_len = network.blockSize(in_packet-UIP_INPACKETOFFSET);
      packetstate = UIPETHERNET_FREEPACKET;
      if (uip_len > 0)
        {
          network.readPacket(in_packet,UIP_INPACKETOFFSET,(uint8_t*)uip_buf,UIP_BUFSIZE);
          if (ETH_HDR ->type == HTONS(UIP_ETHTYPE_IP))
            {
              uip_packet = in_packet;
#ifdef UIPETHERNET_DEBUG
              Serial.print("readPacket type IP, uip_len: ");
              Serial.println(uip_len);
#endif
              uip_arp_ipin();
              uip_input();
              if (uip_len > 0)
                {
                  uip_arp_out();
                  network_send();
                }
            }
          else if (ETH_HDR ->type == HTONS(UIP_ETHTYPE_ARP))
            {
#ifdef UIPETHERNET_DEBUG
              Serial.print("readPacket type ARP, uip_len: ");
              Serial.println(uip_len);
#endif
              uip_arp_arpin();
              if (uip_len > 0)
                {
                  network_send();
                }
            }
        }
      if (in_packet != NOBLOCK && ((packetstate & UIPETHERNET_FREEPACKET) > 0))
        {
#ifdef UIPETHERNET_DEBUG
          Serial.print("freeing packet: ");
          Serial.println(in_packet);
#endif
          network.freeBlock(in_packet);
          in_packet = NOBLOCK;
        }
    }

  if (uip_timer_expired(&periodic_timer))
    {
      uip_timer_reset(&periodic_timer);
      for (int i = 0; i < UIP_CONNS; i++)
        {
          uip_periodic(i);
          // If the above function invocation resulted in data that
          // should be sent out on the network, the global variable
          // uip_len is set to a value > 0.
          if (uip_len > 0)
            {
              uip_arp_out();
              network_send();
            }
        }

#if UIP_UDP
      for (int i = 0; i < UIP_UDP_CONNS; i++)
        {
          uip_udp_periodic(i);
          // If the above function invocation resulted in data that
          // should be sent out on the network, the global variable
          // uip_len is set to a value > 0. */
          if (uip_len > 0)
            {
              network_send();
            }
        }
#endif /* UIP_UDP */
    }
}

boolean UIPEthernetClass::network_send()
{
  uint8_t control = 0; //TODO this belongs to Enc28J60Network!
  if ((packetstate & UIPETHERNET_SENDPACKET) > 0)
    {
#ifdef UIPETHERNET_DEBUG
      Serial.print("network_send uip_packet: ");
      Serial.println(uip_packet);
#endif
      network.writePacket(uip_packet,0,&control,1);
      network.writePacket(uip_packet,UIP_OUTPACKETOFFSET,uip_buf,uip_hdrlen);
      network.sendPacket(uip_packet);
      network.freeBlock(uip_packet);
      uip_packet = NOBLOCK;
      return true;
    }
  uip_packet = network.allocBlock(uip_len+UIP_OUTPACKETOFFSET);
  if (uip_packet != NOBLOCK)
    {
#ifdef UIPETHERNET_DEBUG
      Serial.print("network_send uip_buf (uip_len): ");
      Serial.println(uip_len);
#endif
      network.writePacket(uip_packet,0,&control,1);
      network.writePacket(uip_packet,UIP_OUTPACKETOFFSET,uip_buf,uip_len);
      network.sendPacket(uip_packet);
      network.freeBlock(uip_packet);
      uip_packet = NOBLOCK;
      return true;
    }
  return false;
}

void UIPEthernetClass::init(const uint8_t* mac) {
  uip_timer_set(&this->periodic_timer, CLOCK_SECOND / 4);

  network.init((uint8_t*)mac);
  uip_seteth_addr(mac);

  uip_init();
}

void UIPEthernetClass::configure(IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet) {
  uip_ipaddr_t ipaddr;

  uip_ip_addr(ipaddr, ip);
  uip_sethostaddr(ipaddr);

  uip_ip_addr(ipaddr, gateway);
  uip_setdraddr(ipaddr);

  uip_ip_addr(ipaddr, subnet);
  uip_setnetmask(ipaddr);

  _dnsServerAddress = dns;
}

void
UIPEthernetClass::set_uip_callback(fn_uip_cb_t fn)
{
  this->fn_uip_cb = fn;
}

void
UIPEthernetClass::uip_callback()
{
  struct uipethernet_state *s = &(uip_conn->appstate);

  if (this->fn_uip_cb)
    {
      // The sketch wants to handle all uIP events itself, using uIP functions.
      this->fn_uip_cb(s);			//->p, &s->user);
    }
}

void
UIPEthernetClass::set_uip_udp_callback(fn_uip_udp_cb_t fn)
{
  this->fn_uip_udp_cb = fn;
}

void
UIPEthernetClass::uip_udp_callback()
{
  struct uipudp_state *s = &(uip_udp_conn->appstate);

  if (this->fn_uip_udp_cb)
    {
      // The sketch wants to handle all uIP events itself, using uIP functions.
      this->fn_uip_udp_cb(s);                       //->p, &s->user);
    }
}

UIPEthernetClass UIPEthernet;

// uIP callback function
void
uipethernet_appcall(void)
{
  UIPEthernet.uip_callback();
}

void
uipudp_appcall(void) {
  UIPEthernet.uip_udp_callback();
}

/*---------------------------------------------------------------------------*/
uint16_t
UIPEthernetClass::chksum(uint16_t sum, const uint8_t *data, uint16_t len)
{
  uint16_t t;
  const uint8_t *dataptr;
  const uint8_t *last_byte;

  dataptr = data;
  last_byte = data + len - 1;

  while(dataptr < last_byte) {  /* At least two more bytes */
    t = (dataptr[0] << 8) + dataptr[1];
    sum += t;
    if(sum < t) {
      sum++;            /* carry */
    }
    dataptr += 2;
  }

  if(dataptr == last_byte) {
    t = (dataptr[0] << 8) + 0;
    sum += t;
    if(sum < t) {
      sum++;            /* carry */
    }
  }

  /* Return sum in host byte order. */
  return sum;
}

/*---------------------------------------------------------------------------*/

uint16_t
UIPEthernetClass::ipchksum(void)
{
  uint16_t sum;

  sum = chksum(0, &uip_buf[UIP_LLH_LEN], UIP_IPH_LEN);
  return (sum == 0) ? 0xffff : htons(sum);
}

/*---------------------------------------------------------------------------*/
uint16_t
UIPEthernetClass::upper_layer_chksum(uint8_t proto)
{
  uint16_t upper_layer_len;
  uint16_t sum;

#if UIP_CONF_IPV6
  upper_layer_len = (((u16_t)(BUF->len[0]) << 8) + BUF->len[1]);
#else /* UIP_CONF_IPV6 */
  upper_layer_len = (((u16_t)(BUF->len[0]) << 8) + BUF->len[1]) - UIP_IPH_LEN;
#endif /* UIP_CONF_IPV6 */

  /* First sum pseudoheader. */

  /* IP protocol and length fields. This addition cannot carry. */
  sum = upper_layer_len + proto;
  /* Sum IP source and destination addresses. */
  sum = chksum(sum, (u8_t *)&BUF->srcipaddr[0], 2 * sizeof(uip_ipaddr_t));

  uint8_t upper_layer_memlen;
  switch(proto)
  {
  case UIP_PROTO_ICMP:
  case UIP_PROTO_ICMP6:
    upper_layer_memlen = upper_layer_len;
    break;
  case UIP_PROTO_TCP:
    upper_layer_memlen = (BUF->tcpoffset >> 4) << 2;
    break;
  case UIP_PROTO_UDP:
    upper_layer_memlen = UIP_UDPH_LEN;
    break;
  }
  sum = chksum(sum, &uip_buf[UIP_IPH_LEN + UIP_LLH_LEN], upper_layer_memlen);
#ifdef UIPETHERNET_DEBUG_CHKSUM
  Serial.print("chksum uip_buf[");
  Serial.print(UIP_IPH_LEN + UIP_LLH_LEN);
  Serial.print("-");
  Serial.print(UIP_IPH_LEN + UIP_LLH_LEN + upper_layer_memlen);
  Serial.print("]: ");
  Serial.println(htons(sum),HEX);
#endif
  if (upper_layer_memlen < upper_layer_len)
    {
      sum = network.chksum(
          sum,
          uip_packet,
          UIP_IPH_LEN + UIP_LLH_LEN + upper_layer_memlen + ((packetstate & UIPETHERNET_SENDPACKET) > 0 ? UIP_OUTPACKETOFFSET : UIP_INPACKETOFFSET),
          upper_layer_len - upper_layer_memlen
      );
#ifdef UIPETHERNET_DEBUG_CHKSUM
      Serial.print("chksum uip_packet(");
      Serial.print(uip_packet);
      Serial.print(")[");
      Serial.print(UIP_IPH_LEN + UIP_LLH_LEN + upper_layer_memlen + ((packetstate & UIPETHERNET_SENDPACKET) > 0 ? UIP_OUTPACKETOFFSET : UIP_INPACKETOFFSET));
      Serial.print("-");
      Serial.print(UIP_IPH_LEN + UIP_LLH_LEN + ((packetstate & UIPETHERNET_SENDPACKET) > 0 ? UIP_OUTPACKETOFFSET : UIP_INPACKETOFFSET) + upper_layer_len);
      Serial.print("]: ");
      Serial.println(htons(sum),HEX);
#endif
    }
  return (sum == 0) ? 0xffff : htons(sum);
}

uint16_t
uip_ipchksum(void)
{
  return UIPEthernet.ipchksum();
}

uint16_t
uip_tcpchksum(void)
{
  uint16_t sum = UIPEthernet.upper_layer_chksum(UIP_PROTO_TCP);
  return sum;
}

uint16_t
uip_udpchksum(void)
{
  uint16_t sum = UIPEthernet.upper_layer_chksum(UIP_PROTO_UDP);
  return sum;
}

