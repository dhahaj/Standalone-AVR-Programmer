/**
  Low level support functions
*/

#include "optiLoader.h"

/*
   flashprint

   @breif:  print a text string direct from flash memory to Serial
   @param const char p[]: A char array to print
*/
void flashprint (const char p[]) {
  byte c;
  while (0 != (c = pgm_read_byte(p++))) Serial.write(c);
}

/*
   hexton
   
   @breif: Turn a Hex digit (0..9, A..F) into the equivalent binary value (0-16)
   @return byte h: The byte to convert.
*/
byte hexton (byte h) {
  if (h >= '0' && h <= '9')
    return (h - '0');
  if (h >= 'A' && h <= 'F')
    return ((h - 'A') + 10);
  error("Bad hex digit!");
  return 0;
}

#define PTIME 30

/*
   pulse
   
   @breif Turn a pin on and off a few times; indicates life via LED.
   @param int pin: The pin to pulse.
   @param int times: The number of pulses to perform.
*/
void pulse (int pin, int times) {
  do {
    digitalWrite(pin, HIGH);
    delay(PTIME);
    digitalWrite(pin, LOW);
    delay(PTIME);
  } while (times--);
}
