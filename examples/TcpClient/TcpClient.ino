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

UIPClient client;
unsigned long next;

void setup() {

  Serial.begin(9600);

  UIPEthernet.set_uip_callback(&UIPClient::uip_callback);

  uint8_t mac[] = {0x00,0x01,0x02,0x03,0x04,0x05};
  UIPEthernet.begin(mac,IPAddress (192,168,0,6));

  next = 0;
}

void loop() {

  if (((signed long)(millis() - next)) > 0)
    {
      next = millis() + 5000;
      if (client.connect(IPAddress(192,168,0,1),5000))
        {
          while(!client)
            {
              if (((signed long)(millis()-next)) > 0)
                {
                  client.stop();
                  return;
                }
            }
          const char msg[] = "data from client\0";
          client.println(msg);
          String response = client.readString();
          Serial.println(response);
          client.stop();
        }
    }
}
