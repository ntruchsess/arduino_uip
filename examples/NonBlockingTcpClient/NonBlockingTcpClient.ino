/*
 * UIPEthernet NonBlockingTcpClient example.
 *
 * UIPEthernet is a TCP/IP stack that can be used with a enc28j60 based
 * Ethernet-shield.
 *
 * UIPEthernet uses the fine uIP stack by Adam Dunkels <adam@sics.se>
 *
 *      -----------------
 *
 * This NonBlockingTcpClient example gets its local ip-address via dhcp and
 * attempts to connect via tcp socket-connection to 192.168.0.1 port 5000.
 * After initiating the connection it waits for up to 5 seconds for the
 * connection to be established.
 * If connected successfully it sends a message and waits for a response.
 * After receiving the response the client disconnects and tries to reconnect
 * after 5 seconds.
 * If the connection-attempt does not succeed within 5 seconds the connection
 * is cleaned up (calling stop()).
 *
 * Copyright (C) 2013 by Norbert Truchsess <norbert.truchsess@t-online.de>
 */

#include <UIPEthernet.h>

UIPClientExt client;
signed long next;

void setup() {

  Serial.begin(9600);

  uint8_t mac[6] = {0x00,0x01,0x02,0x03,0x04,0x05};
  Ethernet.begin(mac);

  Serial.print(F("localIP: "));
  Serial.println(Ethernet.localIP());
  Serial.print(F("subnetMask: "));
  Serial.println(Ethernet.subnetMask());
  Serial.print(F("gatewayIP: "));
  Serial.println(Ethernet.gatewayIP());
  Serial.print(F("dnsServerIP: "));
  Serial.println(Ethernet.dnsServerIP());

  next = 0;
}

void loop() {

  if (((signed long)(millis() - next)) > 0)
    {
      next = millis() + 5000;
      Serial.println(F("Client connect"));
      // connectNB returns without waiting for the connection to be established (non-blocking behaviour)
      if (client.connectNB(IPAddress(192,168,0,1),5000))
        {
          Serial.println(F("waiting for connection to be established"));
          // as long the connection is being established the boolean operator of client returns true,
          // but client.connected() return false (0).
          do
            {
              if (!client)
                {
                  Serial.println(F("connection refused"));
                  break;
                }
              if (client.connected())
                {
                  Serial.println(F("Client connected"));
                  break;
                }
              if ((signed long)(millis() - next) > 0)
                {
                  Serial.println(F("timed out"));
                  break;
                }
            }
          while (true);

          if (client.connected())
            {
             client.println(F("DATA from Client"));
              while(client.available()==0)
                {
                  if (next - millis() < 0)
                    goto close;
                }
              int size;
              while((size = client.available()) > 0)
                {
                  uint8_t* msg = (uint8_t*)malloc(size);
                  size = client.read(msg,size);
                  Serial.write(msg,size);
                  free(msg);
                }
    close:
              //disconnect client
              Serial.println(F("Client disconnect"));
              client.stop();
            }
          else
            {
              Serial.println(F("Client connect failed"));
            }
        }
      else
        Serial.println(F("Client connect failed"));
    }
}
