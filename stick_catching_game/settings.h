#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <stddef.h>

// Everything the web panel can change. It is saved to flash, so it
// survives a reboot. The defaults below are used on the first boot and
// when you press RESTORE DEFAULTS in the web panel.

// =====================================================
// LIMITS
// =====================================================

// One PCA9685 drives 16 servos. A second board (address 0x41)
// adds 16 more.
const int STICKS_PER_BOARD = 16;
const int MAX_STICKS = 32;

// Largest per-hook trim, in degrees either way
const int MAX_TRIM = 45;


// =====================================================
// DEFAULTS
// =====================================================

// Number of sticks you built (1 to 16, or up to 32 with two boards)
const int DEFAULT_STICKS = 6;

// Servo angle that holds a stick, and the angle that drops it
const int DEFAULT_HOLD_ANGLE = 0;
const int DEFAULT_RELEASE_ANGLE = 90;

// Time given to a hook to move before the next gap starts (ms)
const int DEFAULT_RELEASE_TIME = 200;

// Pause between START and the first drop (ms)
const int DEFAULT_START_DELAY = 5000;

// Random gap between drops (ms)
const int DEFAULT_MIN_DELAY = 500;
const int DEFAULT_MAX_DELAY = 2000;

// Marathon: rounds in a row, and the pause to reload the sticks (ms)
const int DEFAULT_MARATHON_ROUNDS = 5;
const int DEFAULT_RELOAD_PAUSE = 15000;


// =====================================================
// SETTINGS
// =====================================================

enum GameMode : uint8_t {
  MODE_CLASSIC = 0,    // one stick at a time
  MODE_SPEEDUP = 1,    // gaps get shorter every round
  MODE_DOUBLE = 2,     // two sticks drop together
  MODE_MARATHON = 3    // several rounds with a reload pause
};

const int MODE_COUNT = 4;

struct Settings {
  uint32_t magic;
  uint8_t sticks;
  uint8_t holdAngle;
  uint8_t releaseAngle;
  uint8_t mode;
  int8_t trim[MAX_STICKS];
  uint16_t releaseTime;
  uint16_t startDelay;
  uint16_t minDelay;
  uint16_t maxDelay;
  uint16_t reloadPause;
  uint8_t marathonRounds;
  uint8_t sound;
  uint32_t checksum;
};

// Changes whenever the layout above changes, so old data is ignored
const uint32_t SETTINGS_MAGIC = 0x53544B33;  // "STK3"


// FNV-1a over everything except the checksum itself
inline uint32_t settingsChecksum(const Settings& s) {

  const uint8_t* p = (const uint8_t*)&s;
  uint32_t h = 2166136261u;

  for (size_t i = 0; i < offsetof(Settings, checksum); i++) {
    h ^= p[i];
    h *= 16777619u;
  }

  return h;
}


inline void settingsDefaults(Settings& s) {

  // Zero the padding too, so the checksum is stable
  memset(&s, 0, sizeof(s));

  s.sticks = DEFAULT_STICKS;
  s.holdAngle = DEFAULT_HOLD_ANGLE;
  s.releaseAngle = DEFAULT_RELEASE_ANGLE;
  s.mode = MODE_CLASSIC;
  s.releaseTime = DEFAULT_RELEASE_TIME;
  s.startDelay = DEFAULT_START_DELAY;
  s.minDelay = DEFAULT_MIN_DELAY;
  s.maxDelay = DEFAULT_MAX_DELAY;
  s.reloadPause = DEFAULT_RELOAD_PAUSE;
  s.marathonRounds = DEFAULT_MARATHON_ROUNDS;
  s.sound = 1;
}


// Keep every value in a safe range. availableSticks is 16 with one
// servo board and 32 with two.
inline void settingsClamp(Settings& s, int availableSticks) {

  s.sticks = constrain((int)s.sticks, 1, availableSticks);
  s.holdAngle = constrain((int)s.holdAngle, 0, 180);
  s.releaseAngle = constrain((int)s.releaseAngle, 0, 180);

  if (s.mode >= MODE_COUNT) {
    s.mode = MODE_CLASSIC;
  }

  for (int i = 0; i < MAX_STICKS; i++) {
    s.trim[i] = constrain((int)s.trim[i], -MAX_TRIM, MAX_TRIM);
  }

  if (s.minDelay > s.maxDelay) {
    uint16_t t = s.minDelay;
    s.minDelay = s.maxDelay;
    s.maxDelay = t;
  }

  s.releaseTime = constrain((int)s.releaseTime, 50, 2000);
  s.startDelay = constrain((int)s.startDelay, 1000, 30000);
  s.minDelay = constrain((int)s.minDelay, 50, 30000);
  s.maxDelay = constrain((int)s.maxDelay, 50, 30000);
  s.reloadPause = constrain((int)s.reloadPause, 3000, 60000);
  s.marathonRounds = constrain((int)s.marathonRounds, 2, 20);
  s.sound = s.sound ? 1 : 0;
}


// Returns false (and fills in defaults) when nothing valid is saved yet
inline bool settingsLoad(Settings& s) {

  EEPROM.begin(sizeof(Settings));
  EEPROM.get(0, s);

  if (s.magic == SETTINGS_MAGIC && s.checksum == settingsChecksum(s)) {
    return true;
  }

  settingsDefaults(s);
  return false;
}


// Only writes to flash when something actually changed
inline void settingsSave(Settings& s) {

  s.magic = SETTINGS_MAGIC;
  s.checksum = settingsChecksum(s);

  EEPROM.put(0, s);
  EEPROM.commit();
}
