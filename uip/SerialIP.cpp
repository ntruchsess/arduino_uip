/*
  SerialIP.cpp - Arduino implementation of a uIP wrapper class.
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
#include "SerialIP.h"
#include "uip-conf.h"

extern "C" {
#include "uip.h"
#include "uip_arp.h"
#include "slipdev.h"
#include "timer.h"
}

// Because uIP isn't encapsulated within a class we have to use global
// variables, so we can only have one TCP/IP stack per program.  But at least
// we can set which serial port to use, for those boards with more than one.
//SerialDevice *slip_device;

SerialIPStack::SerialIPStack() :
	fn_uip_cb(NULL)
{
}

/*
void SerialIPStack::use_device(SerialDevice& dev)
{
	::slip_device = &dev;
}

*/



// This function allows you to attach any functions to chip-select the MMC card
void SerialIPStack::attach_functions( unsigned char (* rx)(char *), void (* tx)(unsigned char))
{
   attach_functs(rx,tx);
}


void SerialIPStack::begin(IP_ADDR myIP, IP_ADDR subnet)
{
	uip_ipaddr_t ipaddr;

	timer_set(&this->periodic_timer, CLOCK_SECOND / 4);

	slipdev_init();
	uip_init();

	uip_ipaddr(ipaddr, myIP.a, myIP.b, myIP.c, myIP.d);
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, subnet.a, subnet.b, subnet.c, subnet.d);
	uip_setnetmask(ipaddr);

}

void SerialIPStack::set_gateway(IP_ADDR myIP)
{
  uip_ipaddr_t ipaddr;
  uip_ipaddr(ipaddr, myIP.a, myIP.b, myIP.c, myIP.d);
  uip_setdraddr(ipaddr);
}

void SerialIPStack::listen(uint16_t port)
{
  uip_listen(HTONS(port));
}

void SerialIPStack::tick()
{
	uip_len = slipdev_poll();
	if(uip_len > 0) {
		uip_input();
		// If the above function invocation resulted in data that
		// should be sent out on the network, the global variable
		// uip_len is set to a value > 0.
		if (uip_len > 0) slipdev_send();

	} else if (timer_expired(&periodic_timer)) {
		timer_reset(&periodic_timer);
		for (int i = 0; i < UIP_CONNS; i++) {
			uip_periodic(i);
			// If the above function invocation resulted in data that
			// should be sent out on the network, the global variable
			// uip_len is set to a value > 0.
			if (uip_len > 0) slipdev_send();
		}

#if UIP_UDP
		for (int i = 0; i < UIP_UDP_CONNS; i++) {
			uip_udp_periodic(i);
			// If the above function invocation resulted in data that
			// should be sent out on the network, the global variable
			// uip_len is set to a value > 0. */
			if (uip_len > 0) slipdev_send();
		}
#endif /* UIP_UDP */
	}
}

void SerialIPStack::set_uip_callback(fn_uip_cb_t fn)
{
	this->fn_uip_cb = fn;
}

void SerialIPStack::uip_callback()
{
	struct serialip_state *s = &(uip_conn->appstate);
	//SerialIP.cur_conn = s;
	if (this->fn_uip_cb) {
		// The sketch wants to handle all uIP events itself, using uIP functions.
		this->fn_uip_cb(s);//->p, &s->user);
	} else {
		// The sketch wants to use our simplified interface.
		// This is still in the planning stage :-)
		/*	struct serialip_state *s = &(uip_conn->appstate);

		SerialIP.cur_conn = s;

		if (uip_connected()) {
			s->obpos = 0;
			handle_ip_event(IP_INCOMING_CONNECTION, &s->user);
		}

		if (uip_rexmit() && s->obpos) {
			// Send the same buffer again
			SerialIP.queue();
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

SerialIPStack SerialIP;

// uIP callback function
void serialip_appcall(void)
{
	SerialIP.uip_callback();
}



/*
 * Code to interface the serial port with the SLIP handler.
 *
 * See slipdev.h for further explanation.
 */

//extern SerialDevice *slip_device;

