/*
  Enc28J60IP.cpp - Arduino implementation of a uIP wrapper class.
  Copyright (c) 2010 Adam Nielsen <malvineous@shikadi.net>
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
#include "Enc28J60IP.h"
#include "uip-conf.h"

extern "C" {
#include "uip.h"
#include "uip_arp.h"
#include "network.h"
#include "timer.h"
}

#define ETH_HDR ((struct uip_eth_hdr *)&uip_buf[0])

// Because uIP isn't encapsulated within a class we have to use global
// variables, so we can only have one TCP/IP stack per program.

Enc28J60IPStack::Enc28J60IPStack() :
	fn_uip_cb(NULL)
{
}


void Enc28J60IPStack::begin(IP_ADDR myIP, IP_ADDR subnet)
{
	uip_ipaddr_t ipaddr;

	timer_set(&this->periodic_timer, CLOCK_SECOND / 4);

	uip_eth_addr eth = {{0x00,0x01,0x02,0x03,0x04,0x05}};
	network_init_mac(eth.addr);
	uip_setethaddr(eth);

	uip_init();

	uip_ipaddr(ipaddr, myIP.a, myIP.b, myIP.c, myIP.d);
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, subnet.a, subnet.b, subnet.c, subnet.d);
	uip_setnetmask(ipaddr);

}

void Enc28J60IPStack::set_gateway(IP_ADDR myIP)
{
  uip_ipaddr_t ipaddr;
  uip_ipaddr(ipaddr, myIP.a, myIP.b, myIP.c, myIP.d);
  uip_setdraddr(ipaddr);
}

void Enc28J60IPStack::listen(uint16_t port)
{
  uip_listen(HTONS(port));
}

void Enc28J60IPStack::tick()
{
	uip_len = network_read();

	if(uip_len > 0) {
		if(ETH_HDR->type == HTONS(UIP_ETHTYPE_IP)) {
			uip_arp_ipin();
			uip_input();
			if(uip_len > 0) {
				uip_arp_out();
				network_send();
			}
		} else if(ETH_HDR->type == HTONS(UIP_ETHTYPE_ARP)) {
			uip_arp_arpin();
			if(uip_len > 0) {
				network_send();
			}
		}

	} else if (timer_expired(&periodic_timer)) {
		timer_reset(&periodic_timer);
		for (int i = 0; i < UIP_CONNS; i++) {
			uip_periodic(i);
			// If the above function invocation resulted in data that
			// should be sent out on the network, the global variable
			// uip_len is set to a value > 0.
			if (uip_len > 0) {
				uip_arp_out();
				network_send();
			}
		}

#if UIP_UDP
		for (int i = 0; i < UIP_UDP_CONNS; i++) {
			uip_udp_periodic(i);
			// If the above function invocation resulted in data that
			// should be sent out on the network, the global variable
			// uip_len is set to a value > 0. */
			if (uip_len > 0) {
			    network_send();
			}
		}
#endif /* UIP_UDP */
	}
}

void Enc28J60IPStack::set_uip_callback(fn_uip_cb_t fn)
{
	this->fn_uip_cb = fn;
}

void Enc28J60IPStack::uip_callback()
{
	struct enc28j60ip_state *s = &(uip_conn->appstate);
	//Enc28J60IP.cur_conn = s;
	if (this->fn_uip_cb) {
		// The sketch wants to handle all uIP events itself, using uIP functions.
		this->fn_uip_cb(s);//->p, &s->user);
	} else {
		// The sketch wants to use our simplified interface.
		// This is still in the planning stage :-)
		/*	struct Enc28J60IP_state *s = &(uip_conn->appstate);

		Enc28J60IP.cur_conn = s;

		if (uip_connected()) {
			s->obpos = 0;
			handle_ip_event(IP_INCOMING_CONNECTION, &s->user);
		}

		if (uip_rexmit() && s->obpos) {
			// Send the same buffer again
			Enc28J60IP.queue();
			//uip_send(this->send_buffer, this->bufpos);
			return;
		}

		if (
			uip_closed() ||
			uip_aborted() ||
			uip_timedout()
		) {
			handle_ip_event(IP_CONNECTION_CLOSED, &s->user);
			return;
		}

		if (uip_acked()) {
			s->obpos = 0;
			handle_ip_event(IP_PACKET_ACKED, &s->user);
		}

		if (uip_newdata()) {
			handle_ip_event(IP_INCOMING_DATA, &s->user);
		}

		if (
			uip_newdata() ||
			uip_acked() ||
			uip_connected() ||
			uip_poll()
		) {
			// We've got space to send another packet if the user wants
			handle_ip_event(IP_SEND_PACKET, &s->user);
		}*/
	}
}

Enc28J60IPStack Enc28J60IP;

// uIP callback function
void enc28j60ip_appcall(void)
{
	Enc28J60IP.uip_callback();
}

//void uip_log(char *msg) {
//  Serial.println(msg);
//}
//
//#include <stdarg.h>
//
//void debugprintf(char *fmt, ... ){
//        char tmp[128]; // resulting string limited to 128 chars
//        va_list args;
//        va_start (args, fmt );
//        vsnprintf(tmp, 128, fmt, args);
//        va_end (args);
//        Serial.print(tmp);
//}
//
//void printStats(void) {
//  Serial.print("ip:\ndrop: ");
//  Serial.println(uip_stat.ip.drop);
//  Serial.print("recv: ");
//  Serial.println(uip_stat.ip.recv);
//  Serial.print("sent: ");
//  Serial.println(uip_stat.ip.sent);
//  Serial.print("vhlerr: ");
//  Serial.println(uip_stat.ip.vhlerr);
//  Serial.print("hblenerr: ");
//  Serial.println(uip_stat.ip.hblenerr);
//  Serial.print("lblenerr: ");
//  Serial.println(uip_stat.ip.lblenerr);
//  Serial.print("fragerr: ");
//  Serial.println(uip_stat.ip.fragerr);
//  Serial.print("chkerr: ");
//  Serial.println(uip_stat.ip.chkerr);
//  Serial.print("protoerr: ");
//  Serial.println(uip_stat.ip.protoerr);
//  Serial.print("icmp:\ndrop: ");
//  Serial.println(uip_stat.icmp.drop);
//  Serial.print("recv: ");
//  Serial.println(uip_stat.icmp.recv);
//  Serial.print("sent: ");
//  Serial.println(uip_stat.icmp.sent);
//  Serial.print("typeerr: ");
//  Serial.println(uip_stat.icmp.typeerr);
//  Serial.print("tcp:\ndrop: ");
//  Serial.println(uip_stat.tcp.drop);
//  Serial.print("recv: ");
//  Serial.println(uip_stat.tcp.drop);
//  Serial.print("sent: ");
//  Serial.println(uip_stat.tcp.sent);
//  Serial.print("chkerr: ");
//  Serial.println(uip_stat.tcp.chkerr);
//  Serial.print("ackerr: ");
//  Serial.println(uip_stat.tcp.ackerr);
//  Serial.print("rst: ");
//  Serial.println(uip_stat.tcp.rst);
//  Serial.print("rexmit: ");
//  Serial.println(uip_stat.tcp.rexmit);
//  Serial.print("syndrop: ");
//  Serial.println(uip_stat.tcp.syndrop);
//  Serial.print("synrst: ");
//  Serial.println(uip_stat.tcp.synrst);
//  #if UIP_UDP
//  Serial.print("udp:\ndrop: ");
//  Serial.println(uip_stat.udp.drop);
//  Serial.print("recv: ");
//  Serial.println(uip_stat.udp.recv);
//  Serial.print("sent: ");
//  Serial.println(uip_stat.udp.sent);
//  Serial.print("chkerr: ");
//  Serial.println(uip_stat.udp.chkerr);
//  #endif /* UIP_UDP */
//}

