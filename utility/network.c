#include "uip.h"
#include "enc28j60.h"
#include <avr/io.h>
#include <util/delay.h>

uint16_t network_read(void){
	return ((uint16_t) enc28j60PacketReceive(UIP_BUFSIZE, (uint8_t *)uip_buf));
}

void network_send(void){
	enc28j60PacketSend(uip_len, (uint8_t *)uip_buf);
}

void network_init_mac(const uint8_t* macaddr)
{
	//Initialise the device
	enc28j60Init(macaddr);

	//Configure leds
	enc28j60PhyWrite(PHLCON,0x476);
}

void network_get_MAC(uint8_t* macaddr)
{
	// read MAC address registers
	// NOTE: MAC address in ENC28J60 is byte-backward
	*macaddr++ = enc28j60Read(MAADR5);
	*macaddr++ = enc28j60Read(MAADR4);
	*macaddr++ = enc28j60Read(MAADR3);
	*macaddr++ = enc28j60Read(MAADR2);
	*macaddr++ = enc28j60Read(MAADR1);
	*macaddr++ = enc28j60Read(MAADR0);
}

void network_set_MAC(uint8_t* macaddr)
{
	// write MAC address
	// NOTE: MAC address in ENC28J60 is byte-backward
	enc28j60Write(MAADR5, *macaddr++);
	enc28j60Write(MAADR4, *macaddr++);
	enc28j60Write(MAADR3, *macaddr++);
	enc28j60Write(MAADR2, *macaddr++);
	enc28j60Write(MAADR1, *macaddr++);
	enc28j60Write(MAADR0, *macaddr++);
}

