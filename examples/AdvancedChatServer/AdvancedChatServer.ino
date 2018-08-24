/*
 Advanced Chat Server

 A simple server that distributes any incoming messages to all
 connected clients but the client the message comes from.
 To use telnet to  your device's IP address and type.
 You can see the client's input in the serial monitor as well.
 Using an Arduino Wiznet Ethernet shield.

 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * Analog inputs attached to pins A0 through A5 (optional)

 created 18 Dec 2009
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe
 redesigned to make use of operator== 25 Nov 2013
 by Norbert Truchsess

 */

#include <SPI.h>
#include <UIPEthernet.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,0,6);

// telnet defaults to port 23
EthernetServer server(23);

EthernetClient clients[4];

void setup() {
  // initialize the ethernet device
  Ethernet.begin(mac, ip);
  // start listening for clients
  server.begin();
 // Open serial communications and wait for port to open:
  Serial.begin(9600);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }


  Serial.print("Chat server address:");
  Serial.println(Ethernet.localIP());
}

void loop() {
  // wait for a new client:
  EthernetClient client = server.available();

  if (client) {

    boolean newClient = true;
    for (byte i=0;i<4;i++) {
      //check whether this client refers to the same socket as one of the existing instances:
      if (clients[i]==client) {
        newClient = false;
        break;
      }
    }

    if (newClient) {
      //check which of the existing clients can be overridden:
      for (byte i=0;i<4;i++) {
        if (!clients[i] && clients[i]!=client) {
          clients[i] = client;
          // clead out the input buffer:
          client.flush();
          // clead out the input buffer:
          client.flush();
          Serial.println("We have a new client");
          client.println("Hello, client!");
          client.print("my IP: ");
          client.println(Ethernet.localIP());
          break;
        }
      }
    }

    if (client.available() > 0) {
      // read the bytes incoming from the client:
      char thisChar = client.read();
      // echo the bytes back to all other connected clients:
      for (byte i=0;i<4;i++) {
        if (clients[i] && clients[i]!=client) {
          clients[i].write(thisChar);
        }
      }
      // echo the bytes to the server as well:
      Serial.write(thisChar);
    }
  }
  for (byte i=0;i<4;i++) {
    if (!(clients[i].connected())) {
      // client.stop() invalidates the internal socket-descriptor, so next use of == will allways return false;
      clients[i].stop();
    }
  }
}
