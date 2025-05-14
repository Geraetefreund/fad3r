//Fad3r
//v0.2 / 2025-05-14

#include <EEPROM.h>

/* EEPROM layout - Teensy LC has 128 avaialble
Preset      Addr      Contents
0           0         CC1 CC2 CC3
            3         CH1 CH2 CH3
1           6         ...
...         ...       ...
Active      24        Current preset
*/
const int NUM_FADERS = 3;
const int NUM_PRESETS = 4;
const int BYTES_PER_PRESET = 6; // 3 CC + 3 CH
const int ACTIVE_PRESET_ADDR = NUM_PRESETS * BYTES_PER_PRESET; // stores location pointing to current preset

const int EEPROM_address[3] = {0, 1, 2};

// Fader analog input pins
const int pins[NUM_FADERS] = {A0, A1, A2}; // A0 = Fader 1, A1 = Fader 2, A2 = Fader 3

// MIDI CC and MIDI channel mappings
uint8_t cc[NUM_FADERS];
uint8_t channel[NUM_FADERS];

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

// Reads fader value and checks for significant change
int getFaderValue(int index) {
  int value = analogRead(pins[index]); // 0–1023
  if (abs(value - oldValue[index]) >= 8) { // Change threshold
    oldValue[index] = value;
    return (value < 8) ? 0 : (value >> 3); // Scale to 0–127
  }
  return 255; // No change
}

void loadPreset(uint8_t preset) {
  if (preset >= NUM_PRESETS) return;
  int base = preset * BYTES_PER_PRESET;
  for (int i = 0; i < NUM_FADERS; i++) {
    cc[i] = EEPROM.read(base + i);
    channel[i] = EEPROM.read(base + NUM_FADERS + i);
  }
}

// --- ADDED
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
  }
}

void dumpPreset(uint8_t preset) {
  if (preset >= NUM_PRESETS) return;
  uint8_t data[10];
  data[0] = 0xF0;
  data[1] = 0x7D;
  data[2] = 0x43; // Dump command
  data[3] = preset;
  int base = preset * BYTES_PER_PRESET;
  for (int i = 0; i < NUM_FADERS; i++) {
    data[4 + i] = EEPROM.read(base + i);  // CC
    data[4 + NUM_FADERS + i] = EEPROM.read(base + NUM_FADERS + i); // CH
  }
  data[10 - 1] = 0xF7;
  usbMIDI.sendSysEx(10, data, true);
}

void handleSysEx() {
  if (usbMIDI.getType() == usbMIDI.SystemExclusive && !sysexHandled) {
    const uint8_t* data = usbMIDI.getSysExArray();
    unsigned int len = usbMIDI.getSysExArrayLength();

    if (len >= 5 && data[0] == 0xF0 && data[1] == 0x7D && data[len -1] == 0xF7) {
      uint8_t command = data[2];
      uint8_t preset = data[3];

      switch (command) {
        case 0x40: // Load preset
          if (preset < NUM_PRESETS) {
            applyPreset(preset);  // --- CHANGED
          }
          break;

        case 0x41: // Save current to preset
          if (preset < NUM_PRESETS) {
            savePreset(preset);
          }
          break;

        case 0x42: // Set CCs and channels for preset
          if (len == 11 && preset < NUM_PRESETS) {
            int base = preset * BYTES_PER_PRESET;
            for (int i = 0; i < NUM_FADERS; i++) {
              EEPROM.update(base + i, data[4 + i]);                       // --- FIXED: used to be base + 1
              EEPROM.update(base + NUM_FADERS + i, data[4 + NUM_FADERS + i]);
            }
            applyPreset(preset); // --- ADDED
          }
          break;

        case 0x43: // Dump preset
          if (preset < NUM_PRESETS) {
            dumpPreset(preset);
          }
          break;
      }

      sysexHandled = true;
    }
  }

  // Reset handler when no SysEx message is active anymore
  if (usbMIDI.getType() != usbMIDI.SystemExclusive) {
    sysexHandled = false;
  }
}

