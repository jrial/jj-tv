JJ-TV
=====

This is a simple Arduino sketch for an automated TV-off device. The idea is to
place it in a kid's room, have the TV turn on as soon as they enter the room,
and turn it off if no activity is detected for a certain period of time.

The activity will be detected through a PIR sensor, both for entering the room
and "falling asleep with the telly on".

The circuit consists of:

* An Atmega328 (Arduino for prototyping)
* A PIR sensor to detect motion
* An IR receiver for recording the TV's poweroff IR code
* An IR LED, amplified through an amplification circuit (output pins on Atmega
  are too weak and range suffers)
* A pushbutton to initiate "record" mode.

When the pushbutton is pressed, the circuit enters "record" mode and waits for
an IR code to appear on the IR sensor. When it does, the code gets stored to
both SRAM and EEPROM memory. At boot, a check is done to determine whether the
EEPROM contains a code, and if so, it's loaded into SRAM.

The circuit will now idle until it detects motion. When it does, it transmits
the recorded code, turning the television on. It then idles, regularly checking
whether motion was detected. If after a given period of time, no motion is
detected, it will consider the person to either have left the room, or fallen
asleep, and will transmit the code again, turning the television off.

In order to maximise battery life, the chip is put in powerdown mode as much as
possible, and power-wasting components such as the Arduino's voltage regulator
are avoided. The final circuit will probably just use the bare chip, powered
directly from batteries.

Schematic forthcoming. Current prototype wired as follows:

* Power connected to RAW and GND
* IR Reveiver sense pin on pin 11
* PIR sense pin on pin 12
* Push button on pin 2 (interrupt pin) and VCC, with pulldown resistor between
  pin 2 and GND
* IR LED on pin 3 (required by the library)

The push button and IR LED may be switched around to optimise power consumption.
Programming is done less often than motion sensing, and interrupts on pins other
than 2, and presumably 3, need the library to do some checks in order to
determine the type of change (high, low, rising, falling).
