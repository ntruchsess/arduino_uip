/*
 *  Here's an Arduino uIP demo which shows you how to:
 *   - acquire network configuration via DHCP
 *   - perform a DNS request and save the resulting IP in
 *     a IPAddress object
 *   - perform a HTTP GET request against a remote server
 *     (a Phant.io server in this example)
 *   - save a bit of memory by using a macro around PROGMEM
 *
 * ENC28J60 wiring on ATmega328P based boards:
 *  - SCK -> 13
 *  - SO  -> 12
 *  - SI  -> 11
 *  - CS  -> 10
 *   
 *  by Lorenzo Cafaro (lorenzo@ibisco.net)
 *
 */

#include <UIPEthernet.h>
#include <Dns.h>

// See: https://www.arduino.cc/en/Reference/PROGMEM
#define PGMprintln(x) println(F(x))
#define PGMprint(x) print(F(x))

// MAC address configuration
const uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Destination host and port
const char dsthost[] = "data.sparkfun.com";
const uint16_t dstport = 80;

// Our three main objects
EthernetClient client;
DNSClient DNSclient;
IPAddress dstip;

void setup() {
  Serial.begin(9600);

  // Initializing Ethernet
  Serial.PGMprintln("Initializing Ethernet, waiting for DHCP...");
  Ethernet.begin(mac);
  Serial.PGMprint("IP: ");
  Serial.println(Ethernet.localIP());
  Serial.PGMprint("Subnet mask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.PGMprint("Gateway: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.PGMprint("DNS: ");
  Serial.println(Ethernet.dnsServerIP());

  // Initializing DNS
  Serial.PGMprintln("Initializing DNS client...");
  DNSclient.begin(Ethernet.dnsServerIP());
  if (DNSclient.getHostByName(dsthost, dstip)) {
    Serial.PGMprint("Resolved remote host to: ");
    Serial.println(dstip);
  } else {
    Serial.PGMprintln("Error: could not resolve remote host.");
  }
}

void loop() {
  // Perform your sensor readings here
  uint16_t value = analogRead(A0);
  Serial.PGMprint("Sending value=");
  Serial.print(value);
  
  // Connecting to the remote host
  if (client.connect(dstip, dstport)) {

    // Prepare the buffers
    char getbuf[96] = {0};
    char hostbuf[32] = {0};
    sprintf(getbuf, "GET /input/PUBLIC_KEY?private_key=PRIVATE_KEY&value=%d", value);
    sprintf(hostbuf, "\n Host: %s\n\n", dsthost);

    // Send the data
    client.print(getbuf);
    client.print(hostbuf);

    // Disconnect
    client.stop();

    Serial.PGMprintln("... sent!");

  } else {
    Serial.PGMprintln("... client connection failed!");
  }

  delay(5000);
}
