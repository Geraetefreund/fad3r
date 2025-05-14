//Fad3r

// F0 7D <cc1> <cc2> <cc3> F7 (remember to  convert dec to hex!)
// F0 7D 15 0B 01 F7  (Fader1=cc21, Fader2=cc11, Fader3=cc01)

#include <EEPROM.h>
//#include <Bounce.h>

//EEPROM layout:
const int EEPROM_address[3] = {0, 1, 2};

// Fader analog input pins
const int pins[3] = {A0, A1, A2}; // A0 = Fader 1, A1 = Fader 2, A2 = Fader 3

// MIDI CC mappings: Fader 1 → CC21, Fader 2 → CC11, Fader 3 → CC1
uint8_t cc[3] = {21, 11, 1};

// Previous analog values for change detection and smoothing
int oldValue[3] = {0, 0, 0};

// MIDI channel to send on (1–16)
uint8_t midiChan = 1;
bool sysexHandled = false;


// Calling usbMIDI.getType() in every loop iteration is too laggy
// simple millis timer to the rescue.
unsigned long lastSysExCheck = 0;
const unsigned long sysexInterval = 20; // ms

void setup() {
  for (int i = 0; i < 3; i++) {
    pinMode(pins[i], INPUT);
  }
  // read the cc values from EEPROM
  for (int i = 0; i < 3; i++) {
    cc[i] = EEPROM.read(EEPROM_address[i]);
  }
}

void loop() {
  usbMIDI.read(); // Keep MIDI input buffer active

  // check for SysEx only every 20ms. Poor MIDI buffer gets too excited otherwise...
  if (millis() - lastSysExCheck >= sysexInterval) {
    handleSysEx();
  }

  // Fader reading
  for (int i = 0; i < 3; i++) {
    int val = getFaderValue(i);
    if (val < 255) { // 255 means no significant change
      usbMIDI.sendControlChange(cc[i], val, midiChan);
    }
  }
  
}

// Reads fader value and checks for significant change
int getFaderValue(int index) {
  int value = analogRead(pins[index]); // 0–1023
  if (abs(value - oldValue[index]) >= 8) { // Change threshold
    oldValue[index] = value;
    return (value < 8) ? 0 : (value >> 3); // Scale to 0–127
  }
  return 255; // No change
}

void handleSysEx() {
  if (usbMIDI.getType() == usbMIDI.SystemExclusive && !sysexHandled) {
    const uint8_t* data = usbMIDI.getSysExArray();
    unsigned int len = usbMIDI.getSysExArrayLength();

    if (len == 6 && data[0] == 0xF0 && data[1] == 0x7D && data[5] == 0xF7) {
      cc[0] = data[2];
      cc[1] = data[3];
      cc[2] = data[4];

      EEPROM.update(EEPROM_address[0], cc[0]);
      EEPROM.update(EEPROM_address[1], cc[1]);
      EEPROM.update(EEPROM_address[2], cc[2]);
      
      sysexHandled = true;
    }
  }

  // Reset handler when no SysEx message is active anymore
  if (usbMIDI.getType() != usbMIDI.SystemExclusive) {
    sysexHandled = false;
  }
}

