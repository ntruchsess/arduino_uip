/*
 * UIPEthernet HttpClientTest example by TMRh20
 *
 * UIPEthernet is a TCP/IP stack that can be used with a enc28j60 based
 * Ethernet-shield.
 *
 * UIPEthernet uses the fine uIP stack by Adam Dunkels <adam@sics.se>
 *
 *      -----------------
 *
 * This example will test for hardware failures or errors in data transmissions. This example should be 
 * accompanied by a text file named nums2.txt, which contains about 100,000 characters (bytes) of data. The nums2.txt
 * file is to be placed onto a webserver, accessible by this device.
 *
 * With the IP addresses configured appropriately, the following sketch will continuously download the file,
 * pausing for 5 seconds to display the number of cycles and any errors detected.
 */

#include <UIPEthernet.h>

EthernetClient client;
signed long next;

void setup() {

  Serial.begin(115200);

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
uint32_t charCounter = 0;
uint8_t countAlong = 0;
uint32_t failCount = 0;

uint32_t totalRcvd = 0;
uint32_t errorCount = 0;
uint32_t cycleCount = 0;
uint32_t failedCycles = 0;


void loop() {
 
  Ethernet.update();
  
  if (((signed long)(millis() - next)) > 0) {
      Serial.println(F("Client connect"));

      //Use the IP or hostname of the web server hosting the nums2.txt file
      IPAddress loc(10,10,1,59);
      if (client.connect("Pie-is-Round", 80)) {
          Serial.println(F("Client connected"));
          client.write("GET /nums2.txt HTTP/1.1\n");  
          client.write("Host: www.some-host\n");
          client.write("Connection: close\n");
          client.println();
          
          if(client.waitAvailable(2000)>0){
            client.findUntil("0909","");
            countAlong=1;
          }
          
          while((client.waitAvailable(3500)) > 0) {
              char c = client.read();
              if(c != (countAlong + 48) && totalRcvd < 100000){
                 Serial.println(); Serial.println(F("********F WARN ****"));
                 Serial.print(F("C ")); Serial.println(c);
                 Serial.print(F("CA ")); Serial.println(countAlong);
                 ++errorCount;
              }else{
                 Serial.print(c); 
              }
              ++countAlong;
              if(countAlong > 9){countAlong = 0;}
              if(charCounter >= 100){ Serial.println(); charCounter=0;}
              ++charCounter;
              ++totalRcvd;
          }
            if(totalRcvd != 100001){ failedCycles++; }else{cycleCount++;}
            Serial.println();
            Serial.println(F("****** Total RCV ********"));
            Serial.println(totalRcvd);
            Serial.print(F(" Errors ")); Serial.println(errorCount);
            Serial.print(F(" Cycles OK ")); Serial.println(cycleCount);
            Serial.print(F(" Cycles Failed ")); Serial.println(failedCycles);
            
close:
          //disconnect client
          Serial.println(F("Client disconnect"));
          client.stop();        
      
      }else{ // !client.connect()        
        Serial.println(F("Client connect failed"));
        
      }
      totalRcvd=0;
      charCounter=0; 
      next = millis() + 5000;
    }
}
