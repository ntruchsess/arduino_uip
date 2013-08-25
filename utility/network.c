#include "network.h"
#include "uip.h"
#include "enc28j60.h"
#include <avr/io.h>
#include <util/delay.h>

uint16_t network_read_start(void){
  return enc28j60PacketReceiveStart();
}

uint16_t
network_read_next(uint16_t len, uint8_t * buf)
{
  return enc28j60PacketReceiveNext(len, buf);
}

void network_read_end(void){
  enc28j60PacketReceiveEnd();
}

void network_send(){
  enc28j60PacketSendStart();
  enc28j60PacketSendNext(uip_len,uip_buf);
  enc28j60PacketSendEnd();
}

void network_send_start(void){
  enc28j60PacketSendStart();
}

void network_send_next(uint16_t len, uint8_t *buf){
  enc28j60PacketSendNext(len, buf);
}

void network_send_end(uint16_t len, uint8_t *buf){
  if (len > 0)
    {
      enc28j60PacketSendReset();
      enc28j60WriteBuffer(len,buf);
    }
  enc28j60PacketSendEnd();
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

