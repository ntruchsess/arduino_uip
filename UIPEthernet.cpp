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
#include "uip-conf.h"

extern "C"
{
#include "uip.h"
#include "uip_arp.h"
#include "network.h"
#include "timer.h"
}

#define ETH_HDR ((struct uip_eth_hdr *)&uip_buf[0])

// Because uIP isn't encapsulated within a class we have to use global
// variables, so we can only have one TCP/IP stack per program.

UIPEthernetClass::UIPEthernetClass() : fn_uip_cb(NULL)
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
  ret = ip_addr_uip(a);
  return ret;
}

IPAddress UIPEthernetClass::subnetMask()
{
  IPAddress ret;
  uip_ipaddr_t a;
  uip_getnetmask(a);
  ret = ip_addr_uip(a);
  return ret;
}

IPAddress UIPEthernetClass::gatewayIP()
{
  IPAddress ret;
  uip_ipaddr_t a;
  uip_getdraddr(a);
  ret = ip_addr_uip(a);
  return ret;
}

IPAddress UIPEthernetClass::dnsServerIP()
{
  return _dnsServerAddress;
}

void
UIPEthernetClass::tick()
{
  uip_len = network_read();

  if (uip_len > 0)
    {
      if (ETH_HDR ->type == HTONS(UIP_ETHTYPE_IP))
        {
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
          uip_arp_arpin();
          if (uip_len > 0)
            {
              network_send();
            }
        }

    }
  if (timer_expired(&periodic_timer))
    {
      timer_reset(&periodic_timer);
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

void UIPEthernetClass::init(const uint8_t* mac) {
  timer_set(&this->periodic_timer, CLOCK_SECOND / 4);

  network_init_mac(mac);
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

