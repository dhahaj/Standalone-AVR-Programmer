/**
  Standalone AVR ISP programmer

  This sketch allows an Arduino to program a flash program
  into any AVR if you can fit the HEX file into program memory.
  No computer is necessary. Two LEDs for status notification.
  Press button to program a new chip. Piezo beeper for error/success
  This is ideal for very fast mass-programming of chips!

  It is based on AVRISP

    Using the following pins:
   10: slave reset
   11: MOSI
   12: MISO
   13: SCK
   9: 8 MHz clock output - connect this to the XTAL1 pin of the AVR
      if you want to program a chip that requires a crystal without
     soldering a crystal in
  ---------------------------------------------------------------------- */

#include "optiLoader.h"
#include "SPI.h"

// Global Variables
int pmode = 0;
byte pageBuffer[128]; // One page of flash

// Pins to target
#define SCK   13
#define MISO  12
#define MOSI  11
#define RESET 10
#define CLOCK 9     // self-generate 8mhz clock - handy!

#define BUTTON  A1
#define LED_PIN A3

#define PGM_EN_DELAY 35 // Defines the delay between powerup and sending the Programming Enable instruction

void setup() {

  Serial.begin(9600);  // Initialize serial for status msgs
  Serial.println(F("Standalone PS82/85 ER programmer"));

  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PROGMODE, OUTPUT);
  pinMode(LED_ERR, OUTPUT);
  pulse(LED_PROGMODE, 2);
  pulse(LED_ERR, 4);

  pinMode(BUTTON, INPUT_PULLUP);     // button for next programming

  pinMode(CLOCK, OUTPUT);
  // set up high freq PWM on pin 9 (timer 1). 50% duty cycle -> 8 MHz
  OCR1A = 0;
  ICR1 = 1;
  // OC1A output, fast PWM
  TCCR1A = _BV(WGM11) | _BV(COM1A1);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10); // no clock prescale
}

void loop(void) {
  Serial.println(F("\nType 'p' or hit BUTTON for next chip"));
  while (true)
    if ((! digitalRead(BUTTON)) || (Serial.read() == 'p')) break;

  target_poweron();

  uint16_t signature = readSignature();
  image_t *targetimage;

  if (!signature) {
    error("Signature fail");
    return;
  }

  targetimage = findImage(signature);
  if (!targetimage) {  // look for an image
    error("Image fail");
    return;
  }

  eraseChip();

  // get fuses ready to program
  if (!programFuses(targetimage->image_progfuses)) {
    error("Programming Fuses fail");
    return;
  }

  boolean verified = verifyFuses(targetimage->image_progfuses, targetimage->fusemask);
  if (!verified) {
    error("Failed to verify fuses");
    return;
  }

  end_pmode();
  start_pmode();

  const byte* hex = targetimage->hexcode;
  boolean as_hex = pgm_read_byte(hex) == ':';
  uint16_t pageaddr = 0;
  uint8_t pagesize = pgm_read_byte(&targetimage->image_pagesize);
  uint16_t chipsize = pgm_read_word(&targetimage->chipsize);

  if (as_hex) Serial.println(F("Image interpreted as text"));
  else Serial.println(F("Image interpreted as binary"));

#if (VERBOSE)
  Serial.println(chipsize, DEC);
#endif
  const byte* original_hex = hex;
  while (pageaddr < chipsize) {
    const byte* hexpos = readImagePage(hex, as_hex, pageaddr, pagesize,
                                       pageBuffer);
    boolean blankpage = true;
    for (uint8_t i = 0; i < pagesize; i++) {
      if (pageBuffer[i] != 0xFF) blankpage = false;
    }
    if (!blankpage)
      if (!flashPage(pageBuffer, pageaddr, pagesize)) {
        error("Flash programming failed");
        return;
      }
    hex = hexpos;
    pageaddr += pagesize;
  }

  // Set fuses to 'final' state
  if (!programFuses(targetimage->image_normfuses)) {
    error("Programming Fuses fail");
    return;
  }

  end_pmode();
  start_pmode();

  Serial.println(F("\nVerifying flash..."));
  if (!verifyImage(original_hex, as_hex) )
    error("Failed to verify chip");
  else
    Serial.println(F("\tFlash verified correctly!"));

  if (!verifyFuses(targetimage->image_normfuses, targetimage->fusemask) ) {
    error("Failed to verify fuses");
  } else {
    Serial.println(F("Fuses verified correctly!"));
  }
  target_poweroff();
  tone(LED_PIN, 523, 100);
  delay(400);
  tone(LED_PIN, 523, 200);
  delay(200);
  tone(LED_PIN, 698, 800);
}

/**
  error

  @breif Displays an error msg, waits on the program button, and blinks the LEDs
  @param const char* string: The message to display
*/
void error(const char* string) {
  while (!digitalRead(BUTTON));
  Serial.println(string);
  target_poweroff();
  while (digitalRead(BUTTON)) {
    digitalWrite(LED_ERR, HIGH);
    digitalWrite(LED_PROGMODE, LOW);
    // tone(LED_PIN, 622, 500);
    delay(500);
    digitalWrite(LED_ERR, LOW);
    digitalWrite(LED_PROGMODE, HIGH);
    // tone(LED_PIN, 460, 500);
    delay(500);
  }
  while (!digitalRead(BUTTON));
  digitalWrite(LED_ERR, LOW);
  digitalWrite(LED_PROGMODE, LOW);
}

void start_pmode(void) {
  pinMode(13, INPUT); // restore to default

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV128);

  // debug("...spi_init done");

  // following delays may not work on all targets...
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, HIGH);
  pinMode(SCK, OUTPUT);
  digitalWrite(SCK, LOW);
  delay(50);
  digitalWrite(RESET, LOW);
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);
  delay(PGM_EN_DELAY);

  //debug("...spi_transaction");
  spi_transaction(0xAC, 0x53, 0x00, 0x00); // Send the Programming Enable instruction
  //debug("...Done");
  pmode = 1;
}

void end_pmode(void) {
  SPCR = 0;               /* reset SPI */
  digitalWrite(MISO, 0);  /* Make sure pullups are off too */
  pinMode(MISO, INPUT);
  digitalWrite(MOSI, 0);
  pinMode(MOSI, INPUT);
  digitalWrite(SCK, 0);
  pinMode(SCK, INPUT);
  digitalWrite(RESET, 0);
  pinMode(RESET, INPUT);
  pmode = 0;
}

/*
   target_poweron

   @breif Begin programming.
*/
boolean target_poweron(void) {
  // pinMode(LED_PROGMODE, OUTPUT); // Already set as output from the Setup() method!
  digitalWrite(LED_PROGMODE, HIGH);
  digitalWrite(RESET, LOW);  // reset it right away.
  pinMode(RESET, OUTPUT);
  delay(100);
  Serial.print(F("Starting Program Mode"));
  start_pmode();
  Serial.println(F(" [OK]"));
  return true;
}

boolean target_poweroff(void) {
  end_pmode();
  digitalWrite(LED_PROGMODE, LOW);
  return true;
}
