// This example shows the use of non-blocking connect.
// DNS lookup (if you use a hostname instead of an IP) is still blocking, though.

// Every 5 seconds a HTTP request is sent to 192.168.1.1
// Reply data is sent out via serial

// The LED flash speed indicates the connection state:
// Fast = Connecting
// Medium = HTTP request sent, waiting for reply
// Slow = Got reply, now idle

#include <UIPEthernet.h>

#define CONNSTATE_IDLE 0
#define CONNSTATE_CONNECTING 1
#define CONNSTATE_DATASENT 2

#define LED_PIN 3

typedef unsigned long millis_t;

static EthernetClient client;
static byte flashSpeed;

void setup()
{
	Serial.begin(115200);

	uint8_t mac[6] = {0x00,0x01,0x02,0x03,0x04,0x05};
	Ethernet.begin(mac);

	Serial.print(F("IP: "));
	Serial.println(Ethernet.localIP());
	Serial.print(F("Subnet: "));
	Serial.println(Ethernet.subnetMask());
	Serial.print(F("Gateway: "));
	Serial.println(Ethernet.gatewayIP());
	Serial.print(F("DNS: "));
	Serial.println(Ethernet.dnsServerIP());
	
	flashSpeed = 200;
	
	pinMode(LED_PIN, OUTPUT);
}

void loop()
{
	net();
	flashLED();
}

static void flashLED()
{
	static millis_t lastFlash;
	static byte ledState = LOW;
	
	millis_t now = millis();
	if(now - lastFlash > flashSpeed)
	{
		lastFlash = now;
		digitalWrite(LED_PIN, ledState);
		ledState = !ledState;
	}
}

static void net()
{
	static millis_t lastRequest;
	static millis_t requestStart;
	static byte connState = CONNSTATE_IDLE;

	switch(connState)
	{
		case CONNSTATE_IDLE:
		{
			millis_t now = millis();
			if(now - lastRequest > 5000) // Send a request every 5 seconds
			{
				lastRequest = now;
				if(net_connect())
				{
					requestStart = millis();
					connState = CONNSTATE_CONNECTING;
					flashSpeed = 30; // Fast flash when connecting
				}
			}
		}
			break;
		case CONNSTATE_CONNECTING:
			connState = net_send();
			if(connState == CONNSTATE_DATASENT)
				flashSpeed = 100; // Medium flash when data sent and waiting for a reply
			else if(connState == CONNSTATE_IDLE)
				flashSpeed = 200; // Gone back to idle, must have failed to connect
			break;
		case CONNSTATE_DATASENT:
			if(net_getData())
			{
				connState = CONNSTATE_IDLE;
				flashSpeed = 200; // Slow flash when request complete

				// Request time
				Serial.println();
				Serial.println();
				Serial.print(F("Request time: "));
				Serial.println(millis() - requestStart);
			}
			break;
		default:
			break;
	}
}

// Start connecting
static bool net_connect()
{
	Serial.println(F("Connecting"));
	return client.connect(IPAddress(192,168,1,1), 80, true);
}

static byte net_send()
{
	uip_tcpstate_t state = client.connectTick(); // Update connection status etc
	if(state == UIP_TCP_FAILED) // Failed to connect (timed out or something)
	{
		Serial.println(F("Connection failed"));
		return CONNSTATE_IDLE;
	}
	else if(state == UIP_TCP_CONNECTED) // Connection established, send some data
	{
		Serial.println(F("Connected, sending request"));

		client.println(F("GET /index.html HTTP/1.1"));
		client.println(F("Host: 192.168.1.1"));
		client.println(F("Connection: Close"));
		client.println();

		Serial.println(F("Request sent"));
		
		return CONNSTATE_DATASENT;
	}

	// Still connecting...
	return CONNSTATE_CONNECTING;
}

static bool net_getData()
{
	// See if data is available
	if(!client.available())
		return false;

	// Get and show data
	int size;
	while((size = client.available()) > 0)
	{
		byte* msg = (byte*)malloc(size);
		size = client.read(msg, size);
		Serial.write(msg, size);
		free(msg);
	}

	// Close connection
	client.stop();

	return true;
}