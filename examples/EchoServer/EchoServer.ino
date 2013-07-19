/*
 * UIPEthernet EchoServer example.
 *
 * UIPEthernet is a TCP/IP stack that can be used with a enc28j60 based
 * Ethernet-shield.
 *
 * UIPEthernet uses the fine uIP stack by Adam Dunkels <adam@sics.se>
 *
 *      -----------------
 *
 * This Hello World example sets up a server at 192.168.1.6 on port 1000.
 * Telnet here to access the service.  The uIP stack will also respond to
 * pings to test if you have successfully established a TCP connection to
 * the Arduino.
 *
 * This example was based upon uIP hello-world by Adam Dunkels <adam@sics.se>
 * Ported to the Arduino IDE by Adam Nielsen <malvineous@shikadi.net>
 * Adaption to Enc28J60 by Norbert Truchsess <norbert.truchsess@t-online.de>
 */

#include <UIPEthernet.h>
// The connection_data struct needs to be defined in an external file.
#include <UIPServer.h>
#include <UIPClient.h>

UIPServer server = UIPServer(1000);

void setup() {

  Serial.begin(9600);

  UIPEthernet.set_uip_callback(&UIPClient::uip_callback);

  uint8_t mac[6] = {0x00,0x01,0x02,0x03,0x04,0x05};
  IPAddress myIP(192,168,1,6);

  UIPEthernet.begin(mac,myIP);

  server.begin();
}

void loop() {

  if (UIPClient client = server.available()) {
    if (client) {
      while (client.available()) {
        int c = client.read();
        Serial.write(c);
        client.write(c);
      }
    }
  }
}
