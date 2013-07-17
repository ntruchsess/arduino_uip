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

/*Read from the network, returns number of read bytes*/
uint16_t network_read(void);

/*Send using the network*/
void network_send(void);

/*Sets the MAC address of the device*/
void network_set_MAC(uint8_t* mac);

/*Gets the MAC address of the device*/
void network_get_MAC(uint8_t* mac);

/* get the state of the network link on the interface 1 up, 0 down */
uint8_t network_link_state(void);

#endif /* __NETWORK_H__ */
