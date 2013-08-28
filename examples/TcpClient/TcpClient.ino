/*
 * UIPEthernet TcpClient example.
 *
 * UIPEthernet is a TCP/IP stack that can be used with a enc28j60 based
 * Ethernet-shield.
 *
 * UIPEthernet uses the fine uIP stack by Adam Dunkels <adam@sics.se>
 *
 *      -----------------
 *
 * This TcpClient example sets up a local ip-address 192.168.0.6 and sets
 * up a tcp socket-connection to 192.168.0.1 port 5000 every 5 Seconds.
 * After sending a message it waits for a response. After receiving the
 * response the client disconnects and tries to reconnect after 5 seconds.
 *
 * Copyright (C) 2013 by Norbert Truchsess <norbert.truchsess@t-online.de>
 */

#include <UIPEthernet.h>
// The connection_data struct needs to be defined in an external file.
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
          //wait for the client to connect or time out after 5 sec
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
          //disconnect client
          client.stop();
        }
    }
}
