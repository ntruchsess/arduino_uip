/*
 * SerialIP SendEMail example.
 *
 * Based upon uIP SMTP example by Adam Dunkels <adam@sics.se>
 * Ported to Arduino IDE by Adam Nielsen <malvineous@shikadi.net>
 *
 * Connects to an SMTP server and sends an e-mail.  Configure the constants
 * below to ensure you receive it!
 *
 * See the wiki page at <http://www.arduino.cc/playground/Code/SerialIP> for
 * instructions on establishing a connection with the Arduino, including
 * setting up IP addresses.
 *
 * Note that the outgoing connection will time out if you don't get SLIP
 * going quickly enough after the Arduino resets, so once you do have SLIP
 * active, reset the Arduino to start sending the e-mail from the beginning.
 *
 * If you see the TX light blinking every few seconds but don't get an
 * incoming connection, either your PC's firewall is blocking the request
 * or not sharing the Internet connection (if you're using an external
 * SMTP server.)  Under Linux you can allow the Arduino to connect to your
 * PC like this:
 *
 *  # iptables -I INPUT 1 -i sl0 -j ACCEPT
 *
 * But connection sharing is beyond the scope of this comment block :-)
 *
 */

#include <SerialIP.h>

// This is the SMTP server to connect to.  This default assumes an SMTP server
// on your PC.  Note this is a parameter list, so use commas not dots!
#define SMTP_SERVER 192,168,5,1

// E-mail address of whoever is going to get the e-mail.
#define RECIPIENT_MAIL "postmaster@localhost"

// Hostname used for HELO message.  Doesn't matter and can be made up, assuming
// your SMTP server trusts you :-)
#define HOSTNAME "arduino"

// IP and subnet that the Arduino will be using.  Commas again, not dots.
#define MY_IP 192,168,5,2
#define MY_SUBNET 255,255,255,0

// If you're using a real Internet SMTP server, the Arduino will need to route
// traffic via your PC, so set the PC's IP address here.  Note that this IP is
// for the PC end of the SLIP connection (not any other IP your PC might have.)
// Your PC will have to be configured to share its Internet connection over the
// SLIP interface for this to work.
//#define GATEWAY_IP 192,168,5,1

// Uncomment this and adjust the debug() function to print debugging messages
// somewhere (e.g. an LCD)
//#define DEBUG

#ifdef DEBUG
// This debug code writes messages to an LCD display, since we can't use the
// serial port of course.
#include <LiquidCrystal.h>
LiquidCrystal lcd(12, 11, 2, 3, 4, 5, 6, 7, 8, 9);
void debug_init()
{
  lcd.begin(16, 2);
}
void debug(const char *msg)
{
  lcd.setCursor(0, 0);
  lcd.print(msg);
  delay(750);
}
extern fn_my_cb_t x;
void cb(unsigned long t)
{
  lcd.setCursor(0, 1);
  lcd.print(t);
}
#else
// If debugging is disabled compile out the messages to save space
#define debug(x)
#endif

// Because some of the weird uIP declarations confuse the Arduino IDE, we
// need to manually declare a couple of functions.
void uip_callback(uip_tcp_appstate_t *s);
unsigned char smtp_send(uip_ipaddr_t *smtpserver,
  const char *to, const char *from, const char *msg, uint16_t msglen);

// This structure stores the state of the uIP connection while we're talking
// to the SMTP server.
static struct smtp_state {
  struct psock psock;
  char inputbuffer[16];
  const char *from;
  const char *to;
  const char *msg;
  uint16_t msglen;
} s;

void smtp_done(int status)
{
  // This is a placeholder function that will be called when the e-mail
  // transmission is complete.  status will be 0 on success, or nonzero
  // if there was a failure.
  if (status) {
    char msg[] = "Failure code    ";
    msg[13] = '0' + status;
    debug(msg);
  } else {
    debug("Mail sent :-)   ");
  }
}

void setup() {

  // See the HelloWorldServer example for details about this set up procedure.
  Serial.begin(115200);
  SerialIP.use_device(Serial);
  SerialIP.set_uip_callback(uip_callback);
  SerialIP.begin({MY_IP}, {MY_SUBNET});
#ifdef GATEWAY_IP
  SerialIP.set_gateway({GATEWAY_IP});
#endif

#ifdef DEBUG
  debug_init();
  debug("Debug active");
#endif

  // The actual message we'll be sending.  There's no escaping done, so if
  // you include a dot on a line by itself it will break things (this is
  // escaped by using two dots instead.)
  const char *msg = "From: Arduino <" RECIPIENT_MAIL ">\r\nTo: You <"
    RECIPIENT_MAIL ">\r\nSubject: Hello from your Arduino!\r\n\r\nIf you can "
    "read this message, your Arduino was able to successfully send an "
    "e-mail!\r\n";

  // Funny magic to make uip_ipaddr() work with a macro and set the SMTP
  // server address.
  uip_ipaddr_t smtp_server;
  #define SET_IP(var, ip)  uip_ipaddr(var, ip)
  SET_IP(smtp_server, SMTP_SERVER);

  // Queue up the e-mail.  This function will return immediately but the
  // e-mail will be sent gradually thanks to the tick() function in the main
  // loop().
  smtp_send(&smtp_server, RECIPIENT_MAIL, RECIPIENT_MAIL, msg, strlen(msg));
}

void loop() {
  // Check the serial port and process any incoming data.
  SerialIP.tick();
}

// This is the "thread" that sends the actual mail.  It returns when the
// time comes to wait for a packet to be acknowledged so that other work
// can be done, then uip_callback() calls the function again when more
// IP data arrives.  The PSOCK_* macros take care of continuing execution
// where it left off on subsequent calls.
static PT_THREAD(smtp_thread(void))
{
  static const char *eol = "\r\n";
  PSOCK_BEGIN(&s.psock);

  debug("Wait for banner ");
  PSOCK_READTO(&s.psock, '\n');

  if (strncmp(s.inputbuffer, "220", 3) != 0) {
    PSOCK_CLOSE(&s.psock);
    debug("Banner had error");
    smtp_done(2);
    PSOCK_EXIT(&s.psock);
  }

  debug("Sending HELO    ");
  PSOCK_SEND_STR(&s.psock, "HELO " HOSTNAME "\r\n");

  debug("Wait for HELO ok");
  PSOCK_READTO(&s.psock, '\n');

  if (s.inputbuffer[0] != '2') {
    PSOCK_CLOSE(&s.psock);
    smtp_done(3);
    PSOCK_EXIT(&s.psock);
  }
  debug("HELO accepted   ");

  debug("Sending message ");
  PSOCK_SEND_STR(&s.psock, "MAIL FROM: ");
  PSOCK_SEND_STR(&s.psock, s.from);
  PSOCK_SEND_STR(&s.psock, eol);

  PSOCK_READTO(&s.psock, '\n');
  if (s.inputbuffer[0] != '2') {
    PSOCK_CLOSE(&s.psock);
    smtp_done(4); // MAIL FROM failed
    PSOCK_EXIT(&s.psock);
  }

  PSOCK_SEND_STR(&s.psock, "RCPT TO: ");
  PSOCK_SEND_STR(&s.psock, s.to);
  PSOCK_SEND_STR(&s.psock, eol);

  PSOCK_READTO(&s.psock, '\n');
  if (s.inputbuffer[0] != '2') {
    PSOCK_CLOSE(&s.psock);
    smtp_done(5); // RCPT TO failed
    PSOCK_EXIT(&s.psock);
  }

  PSOCK_SEND_STR(&s.psock, "DATA\r\n");

  PSOCK_READTO(&s.psock, '\n');
  if (s.inputbuffer[0] != '3') { // note different response code
    PSOCK_CLOSE(&s.psock);
    smtp_done(7); // DATA failed
    PSOCK_EXIT(&s.psock);
  }

  // Since the message is larger than the output buffer size, it will have
  // to be broken up into blocks otherwise it will be cut off.  We use
  // uip_mss() to find out the largest amount of data we can send without
  // losing anything.
  while (s.msglen > uip_mss()) {
    PSOCK_SEND(&s.psock, s.msg, uip_mss());
    s.msglen -= uip_mss();
    s.msg += uip_mss();
  }

  // Send whatever's left
  if (s.msglen) PSOCK_SEND(&s.psock, s.msg, s.msglen);

  // Followed by the SMTP "end of message" code
  PSOCK_SEND_STR(&s.psock, "\r\n.\r\n");

  PSOCK_READTO(&s.psock, '\n');
  if (s.inputbuffer[0] != '2') {
    PSOCK_CLOSE(&s.psock);
    smtp_done(8);  // Content rejected (e.g. spam filter)
    PSOCK_EXIT(&s.psock);
  }

  PSOCK_SEND_STR(&s.psock, "QUIT\r\n");

  PSOCK_CLOSE(&s.psock);
  smtp_done(0);

  PSOCK_END(&s.psock);
}

// This is the uIP callback.  It is called whenever an IP event happens
// (which includes regular timer events.)  It "resumes" the smtp_thread()
// function so that the Arduino doesn't get stuck in this function waiting
// for packets to be acknowleged.
void uip_callback(uip_tcp_appstate_t *s)
{
  if (uip_closed()) {
    return;
  }
  if (uip_aborted() || uip_timedout()) {
    smtp_done(1);
    return;
  }

  // Start (or resume) the SMTP communication.
  smtp_thread();
}

unsigned char smtp_send(uip_ipaddr_t *smtpserver,
  const char *to, const char *from, const char *msg, uint16_t msglen)
{
  struct uip_conn *conn;

  // Open a connection to the SMTP server on TCP port 25.
  conn = uip_connect(smtpserver, HTONS(25));
  if (conn == NULL) {
    // Too many concurrent outgoing connections.  Should never happen
    // unless you have multiple connections open at the same time.
    debug("Too many c'ns   ");
    return 0;
  }

  // If we're here then uip_connect() was able to schedule the connection.
  // The actual connect attempt will happen later, and uip_callback will
  // be notified when it succeeds.
  debug("Connect sched ok");

  // Allocate the input buffer, like in the Hello World example.
  PSOCK_INIT(&s.psock, s.inputbuffer, sizeof(s.inputbuffer));

  // Save the message that we'll send so we can access it later from
  // within smtp_thread().
  s.to = to;
  s.from = from;
  s.msg = msg;
  s.msglen = msglen;

  return 1;
}

