/*
 * Simple common network interface that all network drivers should implement.
 */

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <inttypes.h>

/*Initialize the network*/
void network_init(void);

/*Initialize the network with a mac addr*/
void network_init_mac(const uint8_t* macaddr);

/*Check for new packet, returns size of packet (if any). Returns 0 otherwise */
uint16_t network_read_start(void);

/*Read from the network, returns number of read bytes*/
uint16_t network_read_next(uint16_t len, uint8_t *uip_buf);

/*finish reading of packet.*/
void network_read_end(void);

/*Send uip_buf using the network*/
void network_send(void);

/*Prepare sending a packet*/
void network_send_start(void);

/*Write packet data*/
void network_send_next(uint16_t len, uint8_t *buf);

/*Send using the network, optionally rewriting the headers*/
void network_send_end(uint16_t len, uint8_t *buf);

/*Sets the MAC address of the device*/
void network_set_MAC(uint8_t* mac);

/*Gets the MAC address of the device*/
void network_get_MAC(uint8_t* mac);

/* get the state of the network link on the interface 1 up, 0 down */
uint8_t network_link_state(void);

#endif /* __NETWORK_H__ */
