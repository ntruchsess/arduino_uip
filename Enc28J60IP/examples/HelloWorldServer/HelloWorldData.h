/*
 * SerialIP Hello World example.
 *
 * This file contains the definition of the per-connection data that will be
 * allocated on each incoming connection.  Unfortunately it has to be in a
 * separate file because of the way the Arduino IDE works.  (Typedefs can't
 * be listed before function declarations.)
 *
 * You can edit this structure if you want more variables available when
 * processing a connection.  Be careful not to allocate too much data or
 * you might run out of memory (which manifests as the Arduino rebooting for
 * no apparent reason, or never making it to the loop() function after reset.)
 *
 */

typedef struct {
  char input_buffer[16];
  char name[20];
} connection_data;
