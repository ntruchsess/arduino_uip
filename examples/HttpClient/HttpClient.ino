/*
 * UIPEthernet HttpClient example by TMRh20
 *
 * UIPEthernet is a TCP/IP stack that can be used with a enc28j60 based
 * Ethernet-shield.
 *
 * UIPEthernet uses the fine uIP stack by Adam Dunkels <adam@sics.se>
 *
 *      -----------------
 *
 * This HttpClient example gets its local ip-address via dhcp and sets
 * up a tcp socket-connection to google.com port 80 every 5 Seconds.
 * After sending a message it waits for a response. After receiving the
 * response the client disconnects and tries to reconnect after 5 seconds.
 *
 * Copyright (C) 2013 by Norbert Truchsess <norbert.truchsess@t-online.de>
 */

#include <UIPEthernet.h>

EthernetClient client;
signed long next;

void setup() {

  Serial.begin(9600);

  uint8_t mac[6] = {0x00,0x01,0x02,0x03,0x04,0x05};
  Ethernet.begin(mac);

  Serial.print("localIP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("subnetMask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("gatewayIP: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("dnsServerIP: ");
  Serial.println(Ethernet.dnsServerIP());

  next = 0;
}
uint32_t counter = 0;

void loop() {
 
  Ethernet.update();
  
  if (((signed long)(millis() - next)) > 0)
    {
      next = millis() + 5000;
      Serial.println("Client connect");
      // replace hostname with name of machine running tcpserver.pl
//      if (client.connect("server.local",5000))
      if (client.connect("www.google.com", 80))
//      if (client.connect("www.fiikus.net", 80))
        {
          Serial.println("Client connected");
          client.write("GET / HTTP/1.1\n");  
//          client.write("GET /asciiart/pizza.txt HTTP/1.1\n");
          client.write("Host: www.google.ca\n");
//          client.write("Host: fiikus.net\n");
          client.write("Connection: close\n");
          client.println();
          
          while((client.waitAvailable(2000)) > 0)
            {
              Serial.write(client.read());
              if(counter > 100){ Serial.println(" "); counter=0;}
               counter++;
            }
close:
          //disconnect client
          Serial.println("Client disconnect");
          client.stop();
        }
      else
        Serial.println("Client connect failed");
    }
}