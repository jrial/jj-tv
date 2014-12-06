/*
  JJ-TV. Turns the TV on when someone enters the room, and off again when
  no activity is perceived for a preset time. Possible use case: install in a
  kid's room so that the TV shuts off after a preset time after they either
  leave the room or fall asleep with the TV on.

  Version 1.0, December 2014

  Copyright (C) 2014  Juan Rial

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
  */

#include <IRremote.h>
#include <EEPROM.h>
#include <LowPower.h>
#include <PinChangeInt.h>

// How many minutes of inactivity before we turn the TV off?
// Note: not very accurate, but a good ballpark figure.
const int minutes = 45;

// Config

const int RECV_PIN = 11;
const int BUTTON_PIN = 2;
const int STATUS_PIN = 13;
const int MOTION_PIN = 12; // which is also an interrupt pin. :)

IRrecv irrecv(RECV_PIN);
IRsend irsend;

decode_results results;
int sleepCycles;
int elapsedCycles = 0;
bool recording = false;
bool motion = false;
bool tv_on = false;


// Storage for the recorded code
int codeType = -1; // The type of code
unsigned long codeValue; // The code value if not raw
unsigned int rawCodes[RAWBUF]; // The durations if raw
int codeLen; // The length of the code
int toggle = 0; // The RC5/6 toggle state

/*
  Eeprom storage locations:
  0: codeType (int)
  1: codeLen (int)
  2-5: codeValue (unsigned long)
  6-(6+codeLen-1): rawCodes[unsigned int]
  */

/*
  Helper functions for reading/writing long values to EEPROM memory
  Sourced from http://playground.arduino.cc/Code/EEPROMReadWriteLong
  Modified only to take unsigned longs as input/output
  */

//This function will write a 4 byte (32bit) long to the eeprom at
//the specified address to adress + 3.
void EEPROMWritelong(int address, unsigned long value) {
  //Decomposition from a long to 4 bytes by using bitshift.
  //One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);
  //Write the 4 bytes into the eeprom memory.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

unsigned long EEPROMReadlong(long address) {
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);
  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}


/*
  Store/Transmit IR codes. These methods are adaptations from their namesakes
  in the IRrecord example in Ken Shirriff's IRremote library. All the serial
  logging has been removed, and writing to EEPROM has been added to storeCode.
  */

// Stores the code for later playback
void storeCode(decode_results *results) {
  codeType = results->decode_type;
  int count = results->rawlen;
  if (codeType == UNKNOWN) {
    codeLen = results->rawlen - 1;
    EEPROM.write(0, codeType);
    EEPROM.write(1, codeLen);
    // To store raw codes:
    // Drop first value (gap)
    // Convert from ticks to microseconds
    // Tweak marks shorter, and spaces longer to cancel out IR receiver distortion
    for (int i = 1; i <= codeLen; i++) {
      if (i % 2) {
        // Mark
        rawCodes[i - 1] = results->rawbuf[i]*USECPERTICK - MARK_EXCESS;
      }
      else {
        // Space
        rawCodes[i - 1] = results->rawbuf[i]*USECPERTICK + MARK_EXCESS;
      }
      EEPROM.write(6 + i - 1, rawCodes[i - 1]);
    }
  }
  else {
    EEPROM.write(0, codeType);
    if (codeType == NEC) {
      if (results->value == REPEAT) {
        // Don't record a NEC repeat value as that's useless.
        return;
      }
    }
    else if (codeType == SONY) {
    }
    else if (codeType == RC5) {
    }
    else if (codeType == RC6) {
    }
    else {
    }
    codeValue = results->value;
    codeLen = results->bits;
    EEPROM.write(1, codeLen);
    EEPROMWritelong(2, codeValue);
  }
}

void sendCode(int repeat) {
  if (codeType == NEC) {
    if (repeat) {
      irsend.sendNEC(REPEAT, codeLen);
    }
    else {
      irsend.sendNEC(codeValue, codeLen);
    }
  }
  else if (codeType == SONY) {
    irsend.sendSony(codeValue, codeLen);
  }
  else if (codeType == RC5 || codeType == RC6) {
    if (!repeat) {
      // Flip the toggle bit for a new button press
      toggle = 1 - toggle;
    }
    // Put the toggle bit into the code to send
    codeValue = codeValue & ~(1 << (codeLen - 1));
    codeValue = codeValue | (toggle << (codeLen - 1));
    if (codeType == RC5) {
      irsend.sendRC5(codeValue, codeLen);
    }
    else {
      irsend.sendRC6(codeValue, codeLen);
    }
  }
  else if (codeType == UNKNOWN /* i.e. raw */) {
    // Assume 38 KHz
    irsend.sendRaw(rawCodes, codeLen, 38);
  }
}

// Interrupt handlers.
void intrRecord() {
  recording = true;
}

void intrMotion() {
  motion = true;
}


void setup() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(MOTION_PIN, INPUT);
  pinMode(STATUS_PIN, OUTPUT);
  digitalWrite(BUTTON_PIN, LOW);
  digitalWrite(MOTION_PIN, LOW);
  sleepCycles = 60 * minutes / 8;  // precision not terribly important
  // Read stored values from EEPROM
  int tmpCodeType = EEPROM.read(0);
  // EEPROM values are initialised at 255. If value differs,
  // it means we have a code stored, since 255 is not a valid code.
  // Initialise values.
  if (tmpCodeType != 255) {
    codeType = tmpCodeType;
    codeLen = EEPROM.read(1);
    if (codeType != UNKNOWN) {
      codeValue = EEPROMReadlong(2);
    }
    else {
      for (int i=0; i<codeLen; i++) {
        rawCodes[6+i] = EEPROM.read(6+i);
      }
    }
  }
}

void loop() {
  if (recording) {
    int start = millis();
    irrecv.enableIRIn();
    digitalWrite(STATUS_PIN, HIGH);
    while (recording) {
      if ((millis() - start) > 10000) {
        recording = false;
      }
      if (irrecv.decode(&results)) {
        storeCode(&results);
        recording = false;
      }
    }
    digitalWrite(STATUS_PIN, LOW);
  }

  if (motion) {
    elapsedCycles = 0;
    motion = false;
    digitalWrite(STATUS_PIN, HIGH);
    delay(50);
    digitalWrite(STATUS_PIN, LOW);
    if (!tv_on) {
      sendCode(0);
      tv_on = true;
    }
  }
  attachInterrupt(0, intrRecord, FALLING);
  PCintPort::attachInterrupt(MOTION_PIN, intrMotion, CHANGE);
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  detachInterrupt(0);
  PCintPort::PCdetachInterrupt(MOTION_PIN);
  elapsedCycles += 1;

  if (tv_on && (elapsedCycles >= sleepCycles)) {
    sendCode(0);
    tv_on = false;
  }

}
