/*
  SerialIP.h - Arduino implementation of a uIP wrapper class.
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

#ifndef SerialIP_h
#define SerialIP_h

#include <Arduino.h>

extern "C" {
#include "utility/timer.h"
#include "utility/uip.h"
}

typedef struct {
	int a, b, c, d;
} IP_ADDR;

/*
#define IP_INCOMING_CONNECTION  0
#define IP_CONNECTION_CLOSED    1
#define IP_PACKET_ACKED         2
#define IP_INCOMING_DATA        3
#define IP_SEND_PACKET          4
*/
//typedef struct psock ip_connection_t;

typedef void (*fn_uip_cb_t)(uip_tcp_appstate_t *conn);

typedef void (*fn_my_cb_t)(unsigned long a);
extern fn_my_cb_t x;

// Since this is a HardwareSerial class it means you can't use
// SoftwareSerial devices with it, but this could be fixed by making both
// HardwareSerial and SoftwareSerial inherit from a common Serial ancestor
// which we call SerialDevice here.
//typedef Serial_ SerialDevice;


class SerialIPStack {//: public Print {
	public:
		SerialIPStack();

//		void use_device(SerialDevice& dev);
		void attach_functions(unsigned char (*)(char *), void (*)(unsigned char));
 		
		void begin(IP_ADDR myIP, IP_ADDR subnet);
		void set_gateway(IP_ADDR myIP);
		void listen(uint16_t port);

		// tick() must be called at regular intervals to process the incoming serial
		// data and issue IP events to the sketch.  It does not return until all IP
		// events have been processed.
		void tick();

		// Set a user function to handle raw uIP events as they happen.  The
		// callback function can only use uIP functions, but it can also use uIP's
		// protosockets.
		void set_uip_callback(fn_uip_cb_t fn);

		// Returns the number of bytes left in the send buffer.  When this reaches
		// zero all write/print data will be discarded.  Call queue() and return
		// from handle_ip_event() to send the data.  handle_ip_event() will be
		// called with IP_SEND_NEXT_PACKET when the packet has been received by the
		// remote host and you can send more data.
		//int sendbuffer_space();

		// Returns true when the sendbuffer is empty and ready for another packet.
		// Not necessary to use if you only send packets in response to
		// handle_ip_event(IP_SEND_NEXT_PACKET) and you use queue() when you're
		// done.
		//int sendbuffer_ready();

		// Don't mix this with write() in the same IP_SEND_NEXT_PACKET call.
		// This is *way* more efficient than using write() and queue() anyway.
		//void queueBuffer(uint8_t buf, int len);
		//void queueString(const char *buf);

		// Queue up the current sendbuffer.  It will be sent and flushed at the
		// next available opportunity.
		//void queue();

		// Print methods
		//virtual void write(uint8_t ch);
		/*virtual void write(const char *str);
		virtual void write(const uint8_t *buffer, size_t size);*/

	private:
		struct timer periodic_timer;
		//, arp_timer;
		struct serialip_state *cur_conn; // current connection (for print etc.)
		fn_uip_cb_t fn_uip_cb;
		void uip_callback();

	friend void serialip_appcall(void);

};

//void handle_ip_event(uint8_t type, ip_connection_t *conn, void **user);

extern SerialIPStack SerialIP;

#endif
