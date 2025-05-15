//Fad3r
//v0.3 / 2025-05-15

#include <EEPROM.h>

/* EEPROM layout - Teensy LC has 128 avaialble
Preset      Addr      Contents
0           0         CC1 CC2 CC3
            3         CH1 CH2 CH3
            6         Curve1, Curve2, Curve3
1           9         ...
...         ...       ...
Active      36        Current preset */

const int NUM_FADERS = 3;
const int NUM_PRESETS = 4;
const int BYTES_PER_PRESET = 9; // 3 CC + 3 CH + 3 curve
const int ACTIVE_PRESET_ADDR = NUM_PRESETS * BYTES_PER_PRESET; // stores location pointing to current preset

// Fader analog input pins
const int pins[NUM_FADERS] = {A0, A1, A2}; // A0 = Fader 1, A1 = Fader 2, A2 = Fader 3

// MIDI CC and MIDI channel mappings
uint8_t cc[NUM_FADERS];
uint8_t channel[NUM_FADERS];
uint8_t curve[NUM_FADERS]; // 0 = linear, 1 = logarithmic, 2 - exponential

uint8_t currentPreset = 0;

bool sysexHandled = false;

// Previous analog values for change detection and smoothing
int oldValue[NUM_FADERS] = {0, 0, 0};

// Calling usbMIDI.getType() in every loop iteration is too laggy
// simple millis timer to the rescue.
unsigned long lastSysExCheck = 0;
const unsigned long sysexInterval = 20; // ms

void setup() {
  for (int i = 0; i < NUM_FADERS; i++) {
    pinMode(pins[i], INPUT);
  }

  currentPreset = EEPROM.read(ACTIVE_PRESET_ADDR);
  if (currentPreset >= NUM_PRESETS) currentPreset = 0;

  loadPreset(currentPreset);
}

void loop() {
  usbMIDI.read(); // Keep MIDI input buffer active

  // check for SysEx only every 20ms. Poor MIDI buffer gets too excited otherwise...
  if (millis() - lastSysExCheck >= sysexInterval) {
    handleSysEx();
  }

  // Fader reading and sending
  for (int i = 0; i < NUM_FADERS; i++) {
    int val = getFaderValue(i);
    if (val < 255) { // 255 means no significant change
      usbMIDI.sendControlChange(cc[i], val, channel[i]);
    }
  }
}

int applyCurve(int raw, uint8_t type) {
  float norm = raw / 1023.0;
  float curved = 0.0;

  switch (type) {
    case 0x00: // linear
    curved = norm;
    break;

    case 0x01: // logarithmic
    curved = log10(9 * norm +1); //log(1) = 0, log(10) = 1
    break;

    case 0x02: // exponential
    curved = (exp(norm) -1) / (exp(1) -1);
    break;
  }

  int result = int(curved * 127);
  return constrain(result, 0, 127);
}

// Reads fader value and checks for significant change
int getFaderValue(int index) {
  int value = analogRead(pins[index]); // 0–1023
  if (abs(value - oldValue[index]) >= 8) { // Change threshold
    oldValue[index] = value;
    //return (value < 8) ? 0 : (value >> 3); // Scale to 0–127
    return applyCurve(value, curve[index]);
  }
  return 255; // No change
}

void loadPreset(uint8_t preset) {
  if (preset >= NUM_PRESETS) return;
  int base = preset * BYTES_PER_PRESET;
  for (int i = 0; i < NUM_FADERS; i++) {
    cc[i] = EEPROM.read(base + i);
    channel[i] = EEPROM.read(base + NUM_FADERS + i);
    curve[i] = EEPROM.read(base + 2 * NUM_FADERS +i);
  }
}


void applyPreset(uint8_t preset) {
  loadPreset(preset);
  currentPreset = preset;
  EEPROM.update(ACTIVE_PRESET_ADDR, preset);
}

void savePreset(uint8_t preset) {
  if (preset >= NUM_PRESETS) return;
  int base = preset * BYTES_PER_PRESET;
  for (int i = 0; i < NUM_FADERS; i++) {
    EEPROM.update(base + i, cc[i]);
    EEPROM.update(base + NUM_FADERS + i, channel[i]);
    EEPROM.update(base + 2 * NUM_FADERS + i, curve[i]);
  }
}

void dumpPreset(uint8_t preset) {
  if (preset >= NUM_PRESETS) return;
  uint8_t data[14]; // SysEx: F0 7D 43 <Preset> CC1 CC2 CC3 CH1 CH2 CH3 CV1 CV2 CV3 F7
  data[0] = 0xF0;
  data[1] = 0x7D;
  data[2] = 0x43; // Dump command
  data[3] = preset;
  int base = preset * BYTES_PER_PRESET;
  for (int i = 0; i < NUM_FADERS; i++) {
    data[4 + i] = EEPROM.read(base + i);  // CC
    data[7 + i] = EEPROM.read(base + NUM_FADERS + i); // CH
    data[10 + i] = EEPROM.read(base + 2 * NUM_FADERS +i);
  }
  data[13] = 0xF7;
  usbMIDI.sendSysEx(14, data, true);
}

void handleSysEx() {
  static uint8_t lastCommand = 0x00;
  static uint8_t lastPreset = 0xFF;

  if (usbMIDI.getType() == usbMIDI.SystemExclusive) {
    const uint8_t* data = usbMIDI.getSysExArray();
    unsigned int len = usbMIDI.getSysExArrayLength();

    if (len >= 5 && data[0] == 0xF0 && data[1] == 0x7D && data[len - 1] == 0xF7) {
      uint8_t command = data[2];
      uint8_t preset = data[3];

      // Avoid re-processing the same message repeatedly
      if (command == lastCommand && preset == lastPreset) return;

      lastCommand = command;
      lastPreset = preset;

      switch (command) {
        case 0x40: applyPreset(preset); break;
        case 0x41: savePreset(preset); break;
        case 0x42:
          if (len == 14 && preset < NUM_PRESETS) {
            int base = preset * BYTES_PER_PRESET;
            for (int i = 0; i < NUM_FADERS; i++) {
              EEPROM.update(base + i, data[4 + i]);
              EEPROM.update(base + NUM_FADERS + i, data[7 + i]);
              EEPROM.update(base + 2 * NUM_FADERS + i, data[10 +i]);
            }
            applyPreset(preset);
          }
          break;
        case 0x43: dumpPreset(preset); break;
        case 0x44: {
          uint8_t reply[5] = {0xF0, 0x7D, 0x44, currentPreset, 0xF7};
          usbMIDI.sendSysEx(5, reply, true);
        } break;
      }
    }
  } else {
    // Reset detection when SysEx is no longer active
    sysexHandled = false;
    // Reset last command to allow next SysEx message to be processed
    lastCommand = 0x00;
    lastPreset = 0xFF;
  }
}

