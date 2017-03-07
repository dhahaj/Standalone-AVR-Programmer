/*
   Bootload images.
   These are the intel Hex files produced by the optiboot makefile,
   with a small amount of automatic editing to turn them into C strings,
   and a header attched to identify them
*/

#include "optiLoader.h"

#define VERBOSE 0
extern image_t *images[];
extern uint8_t NUMIMAGES;

/*
   readSignature

   @breif: Note that the highest signature byte is the same over all AVRs so we skip it
   @return uint16_t: read the bottom two signature bytes (if possible) and return them
*/
uint16_t readSignature (void) {
  SPI.setClockDivider(CLOCKSPEED_FUSES);
  uint16_t target_type = 0;
  Serial.print(F("\nReading signature:"));

  target_type = spi_transaction(0x30, 0x00, 0x01, 0x00);
  target_type <<= 8;
  target_type |= spi_transaction(0x30, 0x00, 0x02, 0x00);

  Serial.println(target_type, HEX);
  if (target_type == 0 || target_type == 0xFFFF)
    if (target_type == 0) Serial.println(F("  (no target attached?)"));
  return target_type;
}

/*
   findImage

   @breif: given 'signature' loaded with the relevant part of the device signature,
           search the hex images that we have programmed in flash, looking for one
           that matches.
   @param uint16_t signature: Signature of the chip
   @return image_t* findImage: Pointer to the image, 0 if not found
*/
image_t *findImage (uint16_t signature) {
  image_t *ip;
  Serial.println(F("Searching for image..."));

  for (byte i = 0; i < NUMIMAGES; i++) {
    ip = images[i];

    if (ip && (pgm_read_word(&ip->image_chipsig) == signature)) {
      Serial.print(F("  Found \""));
      flashprint(&ip->image_name[0]);
      Serial.print(F("\" for "));
      flashprint(&ip->image_chipname[0]);
      Serial.println();
      return ip;
    }
  }
  Serial.println(F(" Not Found"));
  return 0;
}

/*
   programmingFuses

   @breif: Program the fuse/lock bits
   @param const byte* fuses: Array consisting of the fuse bytes
   @return boolean: Alaways true
*/
boolean programFuses (const byte *fuses) {
  SPI.setClockDivider(CLOCKSPEED_FUSES);

  byte f;
  Serial.print(F("\nSetting fuses"));
  f = pgm_read_byte(&fuses[FUSE_PROT]);
  if (f) {
    Serial.print(F("\n  Set Lock Fuse to: "));
    Serial.print(f, HEX);
    Serial.print(F(" -> "));
    Serial.print(spi_transaction(0xAC, 0xE0, 0x00, f), HEX);
  }
  f = pgm_read_byte(&fuses[FUSE_LOW]);
  if (f) {
    Serial.print(F("  Set Low Fuse to: "));
    Serial.print(f, HEX);
    Serial.print(F(" -> "));
    Serial.print(spi_transaction(0xAC, 0xA0, 0x00, f), HEX);
  }
  f = pgm_read_byte(&fuses[FUSE_HIGH]);
  if (f) {
    Serial.print(F("  Set High Fuse to: "));
    Serial.print(f, HEX);
    Serial.print(F(" -> "));
    Serial.print(spi_transaction(0xAC, 0xA8, 0x00, f), HEX);
  }
  f = pgm_read_byte(&fuses[FUSE_EXT]);
  if (f) {
    Serial.print(F("  Set Ext Fuse to: "));
    Serial.print(f, HEX);
    Serial.print(F(" -> "));
    Serial.print(spi_transaction(0xAC, 0xA4, 0x00, f), HEX);
  }
  Serial.println();
  return true;
}

/*
   verifyFuses

   Verifies a fuse set
   @param const byte* fuses: Array containing the fuses to program
   @param const byte* fusemask: Byte array of the fusemasks
   @return boolean: True if fuses are verfied
*/
boolean verifyFuses (const byte *fuses, const byte *fusemask) {
  SPI.setClockDivider(CLOCKSPEED_FUSES);
  byte f;
  Serial.println(F("Verifying fuses..."));
  f = pgm_read_byte(&fuses[FUSE_PROT]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x58, 0x00, 0x00, 0x00);  // lock fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_PROT]);
    Serial.print(F("\tLock Fuse: "));
    Serial.print(f, HEX);
    Serial.print(F(" is "));
    Serial.print(readfuse, HEX);
    if (readfuse != f)
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_LOW]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x50, 0x00, 0x00, 0x00);  // low fuse
    Serial.print(F("\tLow Fuse: 0x"));
    Serial.print(f, HEX);
    Serial.print(F(" is 0x"));
    Serial.print(readfuse, HEX);
    readfuse &= pgm_read_byte(&fusemask[FUSE_LOW]);
    if (readfuse != f)
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_HIGH]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x58, 0x08, 0x00, 0x00);  // high fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_HIGH]);
    Serial.print(F("\tHigh Fuse: 0x"));
    Serial.print(f, HEX);
    Serial.print(F(" is 0x"));
    Serial.print(readfuse, HEX);
    if (readfuse != f)
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_EXT]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x50, 0x08, 0x00, 0x00);  // ext fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_EXT]);
    Serial.print(F("\tExt Fuse: 0x"));
    Serial.print(f, HEX);
    Serial.print(F(" is 0x"));
    Serial.print(readfuse, HEX);
    if (readfuse != f) return false;
  }
  Serial.println();
  return true;
}

/**
  readNextOctet

  @param const byte* p: Pointer to array
  @param boolean as_hex: Read in hex format
  @return const byte*: Pointer(Array) to the read page
*/
const byte* readNextOctet(const byte* p, boolean as_hex, byte& b) {
  if (as_hex) {
    b = hexton(pgm_read_byte(p++));
    b = (b << 4) + hexton(pgm_read_byte(p++));
  } else b = pgm_read_byte(p++);
  return p;
}

/*
   readImagePage

   Read a page of intel hex image from a string in pgm memory.
   @param const byte* hex: Pointer to a byte array to store the read bytes.
   @param boolean as_hex:
   @param uint16_t pageaddr: The page address to read from.
   @param uint8_t pagesize: The size of the page.
   @param byte* page:
   @return const byte*: Pointer to an array containing the read image page.
*/
const byte* readImagePage(const byte* hex, boolean as_hex, uint16_t pageaddr, uint8_t pagesize, byte* page) {
  uint8_t len;
  uint8_t page_idx = 0;
  const byte* beginning = hex;

  byte b, cksum = 0;

  // 'empty' the page by filling it with 0xFF's
  for (uint8_t i = 0; i < pagesize; i++)
    page[i] = 0xFF;

  uint16_t expected_address = 0;
  while (true) {
    uint16_t lineaddr;

    // read one line!
    if (as_hex) {
      if (pgm_read_byte(hex++) != ':') {
        error("No colon?");
        break;
      }
    }
    // Read the byte count into 'len'
    hex = readNextOctet(hex, as_hex, len);
    cksum = len;

    // read high address byte
    hex = readNextOctet(hex, as_hex, b);
    cksum += b;
    lineaddr = b;

    // read low address byte
    hex = readNextOctet(hex, as_hex, b);
    cksum += b;
    lineaddr = (lineaddr << 8) + b;

    if (lineaddr >= (pageaddr + pagesize)) {
      return beginning;
    }

    if (expected_address == 0)
      expected_address = lineaddr;
    else if (lineaddr > expected_address) {
      page_idx += (lineaddr - expected_address);
      expected_address = lineaddr;
    }

    // record type
    hex = readNextOctet(hex, as_hex, b);
    cksum += b;
#if (VERBOSE)
    Serial.print(F("Record type ")); Serial.println(b, HEX);
#endif
    if (b == 0x1) break; // end record!
#if (VERBOSE)
    Serial.print(F("\nLine address = 0x")); Serial.println(lineaddr, HEX);
    Serial.print(F("Page address = 0x")); Serial.println(pageaddr, HEX);
    Serial.print(F("\nPage index = ")); Serial.println(page_idx, HEX);
#endif
    for (byte i = 0; i < len; i++) {
      hex = readNextOctet(hex, as_hex, b);
      cksum += b;

#if (VERBOSE)
      Serial.print(b, HEX);
      Serial.write(' ');
#endif
      if (page_idx > pagesize) {
        error("Too much code");
        break;
      }
      page[page_idx] = b;
      page_idx++;
    }  /*  For Loop  */

    // chksum
    hex = readNextOctet(hex, as_hex, b);
    cksum += b;
    if (cksum != 0) {
      error("Bad checksum: ");
      Serial.print(cksum, HEX);
    }

    if (as_hex)
      if (pgm_read_byte(hex++) != '\n') {
        error("No end of line");
        break;
      }
#if (VERBOSE)
    Serial.println();
    Serial.println(page_idx, DEC);
#endif
    if (page_idx == pagesize) break;
    expected_address += len;
  }  /**  End While  **/

#if (VERBOSE)
  Serial.print(F("\n  Total bytes read: "));
  Serial.println(page_idx, DEC);
#endif

  return hex;
}

// Send one byte to the page buffer on the chip
void flashWord (uint8_t hilo, uint16_t addr, uint8_t data) {
#if (VERBOSE)
  Serial.print(data, HEX);  Serial.print(':');
  Serial.print(spi_transaction(0x40 + 8 * hilo, addr >> 8 & 0xFF, addr & 0xFF, data), HEX);
  Serial.print(" ");
#else
  spi_transaction(0x40 + 8 * hilo, addr >> 8 & 0xFF, addr & 0xFF, data);
#endif
}

// Basically, write the pagebuff (with pagesize bytes in it) into
// page $pageaddr
boolean flashPage (byte *pagebuff, uint16_t pageaddr, uint8_t pagesize) {
  SPI.setClockDivider(CLOCKSPEED_FLASH);

  Serial.print(F("Flashing page ")); Serial.println(pageaddr, HEX);
  for (uint16_t i = 0; i < pagesize / 2; i++) {

#if (VERBOSE)
    Serial.print(pagebuff[2 * i], HEX); Serial.print(' ');
    Serial.print(pagebuff[2 * i + 1], HEX); Serial.print(' ');
    if ( i % 16 == 15) Serial.println();
#endif

    flashWord(LOW, i, pagebuff[2 * i]);
    flashWord(HIGH, i, pagebuff[2 * i + 1]);
  }

  // page addr is in bytes, byt we need to convert to words (/2)
  pageaddr = (pageaddr / 2) & 0xFFC0;

  uint16_t commitreply = spi_transaction(0x4C, (pageaddr >> 8) & 0xFF,
                                         pageaddr & 0xFF, 0);

  Serial.print(F("  Commit Page: 0x"));  Serial.print(pageaddr, HEX);
  Serial.print(F(" -> 0x")); Serial.println(commitreply, HEX);
  if (commitreply != pageaddr) return false;
  busyWait();
  return true;
}

// verifyImage does a byte-by-byte verify of the flash hex against the chip.
// Thankfully this does not have to be done by pages!
// returns true if the image is the same as the hextext.
// returns false on any error.
boolean verifyImage(const byte* hex, boolean as_hex) {
  uint8_t len;
  byte b, cksum = 0;

  SPI.setClockDivider(CLOCKSPEED_FLASH);

  while (true) {
    uint16_t lineaddr;

    // read one line!
    if (as_hex) {
      if (pgm_read_byte(hex++) != ':') {
        error("No colon");
        return false;
      }
    }
    hex = readNextOctet(hex, as_hex, len);
    cksum = len;
    hex = readNextOctet(hex, as_hex, b);
    cksum += b;
    lineaddr = b;
    hex = readNextOctet(hex, as_hex, b);
    cksum += b;
    lineaddr = (lineaddr << 8) + b;
    hex = readNextOctet(hex, as_hex, b);
    cksum += b;

#if (VERBOSE)
    Serial.print(F("Record type ")); Serial.println(b, HEX);
#endif
    if (b == 0x1) break; // end record!

    for (byte i = 0; i < len; i++) {
      hex = readNextOctet(hex, as_hex, b);
      cksum += b;
#if (VERBOSE)
      Serial.print(F)"$"));
      Serial.print(lineaddr, HEX);
      Serial.print(F(":0x"));
      Serial.print(b, HEX);
      Serial.write(F(" ? "));
#endif
      // verify this byte!
      if (lineaddr % 2) {
      // for 'high' bytes:
      if (b != (spi_transaction(0x28, lineaddr >> 9, lineaddr / 2, 0) & 0xFF)) {
          Serial.print(F("verification error at address 0x"));
          Serial.print(lineaddr, HEX);
          Serial.print(F(" Should be 0x"));
          Serial.print(b, HEX);
          Serial.print(F(" not 0x"));
          Serial.println(spi_transaction(0x28, lineaddr >> 9, lineaddr / 2, 0) & 0xFF, HEX);
          return false;
        }
      } else {
        // for 'low bytes'
        if (b != (spi_transaction(0x20,
                                  lineaddr >> 9, lineaddr / 2, 0) & 0xFF)) {
          Serial.print(F("verification error at address 0x"));
          Serial.print(lineaddr, HEX);
          Serial.print(F(" Should be 0x"));
          Serial.print(b, HEX);
          Serial.print(F(" not 0x"));
          Serial.println(spi_transaction(0x20, lineaddr >> 9,
                                         lineaddr / 2, 0) & 0xFF, HEX);
          return false;
        }
      }
      lineaddr++;
    }  /**  End For Loop  **/

    hex = readNextOctet(hex, as_hex, b);
    cksum += b;
    if (cksum != 0) {
      error("Bad checksum: ");
      Serial.print(cksum, HEX);
      return false;
    }
    if (as_hex)
      if (pgm_read_byte(hex++) != '\n') {
        error("No end of line");
        return false;
      }
  }
  return true;
}

// Send the erase command, then busy wait until the chip is erased
void eraseChip(void) {
  SPI.setClockDivider(CLOCKSPEED_FUSES);
  spi_transaction(0xAC, 0x80, 0, 0);    // chip erase
  busyWait();
}

// Simply polls the chip until it is not busy any more - for erasing and programming
void busyWait(void) {
  byte busybit;
  do {
    busybit = spi_transaction(0xF0, 0x0, 0x0, 0x0);
  } while (busybit & 0x01);
}

/**
  spi_transaction

  @breif Performs an SPI transaction.
  @param uint8_t a,b,c,d: 4 Bytes to send
*/
uint32_t spi_transaction (uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  uint32_t m = SPI.transfer(a),
           n = SPI.transfer(b),
           o = SPI.transfer(c),
           p = SPI.transfer(d);
  // m = SPI.transfer(a);n = SPI.transfer(b);o = SPI.transfer(c);p = SPI.transfer(d);
  return (m << 24) + (n << 16) + (o << 8) + p;
}
