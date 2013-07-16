#include "Enc28J60IP.h"

void callback(uip_tcp_appstate_t *conn);

void setup() {
  IP_ADDR myAddress = {192,168,1,5};
  IP_ADDR subnet = {255,255,255,0};
  Enc28J60IP.set_uip_callback(&callback);
  Enc28J60IP.begin(myAddress,subnet);
}

void loop() {
  Enc28J60IP.tick();
}

void callback(uip_tcp_appstate_t *conn) {
}
