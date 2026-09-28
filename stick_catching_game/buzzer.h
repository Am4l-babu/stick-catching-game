#pragma once

#include <Arduino.h>

// Non-blocking beeps on a passive buzzer.
//
// play() queues a short tune and returns at once; update() (called from
// loop()) starts each note when the previous one ends, so the game and
// the web server never wait for a sound.

struct Note {
  uint16_t freq;   // Hz, 0 = rest
  uint16_t ms;
};


class Buzzer {

public:

  void begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
  }


  void setEnabled(bool on) {
    _enabled = on;
    if (!on) {
      stop();
    }
  }


  bool enabled() const {
    return _enabled;
  }


  // Replaces whatever is playing
  void play(const Note* notes, uint8_t count) {

    if (!_enabled) {
      return;
    }

    stop();

    _count = min<uint8_t>(count, MAX_NOTES);

    for (uint8_t i = 0; i < _count; i++) {
      _notes[i] = notes[i];
    }
  }


  void stop() {
    _count = 0;
    _next = 0;
    _noteEnd = 0;
    noTone(_pin);
  }


  void update() {

    if (_next >= _count) {
      return;
    }

    unsigned long now = millis();

    if (_next > 0 && (long)(now - _noteEnd) < 0) {
      return;
    }

    const Note& n = _notes[_next++];

    if (n.freq) {
      tone(_pin, n.freq, n.ms);
    } else {
      noTone(_pin);
    }

    _noteEnd = now + n.ms;

    if (_next >= _count) {
      _next = _count = 0;
    }
  }


  // ---- Game sounds (same as the browser simulator) ----

  void tick() {
    static const Note n[] = {{660, 70}};
    play(n, 1);
  }

  void start() {
    static const Note n[] = {{520, 90}, {780, 140}};
    play(n, 2);
  }

  void go() {
    static const Note n[] = {{1040, 300}};
    play(n, 1);
  }

  void release() {
    static const Note n[] = {{230, 50}};
    play(n, 1);
  }

  void miss() {
    static const Note n[] = {{150, 280}};
    play(n, 1);
  }

  void stopped() {
    static const Note n[] = {{300, 120}, {180, 160}};
    play(n, 2);
  }

  void roundDone() {
    static const Note n[] = {{523, 110}, {659, 110}, {784, 110}, {1047, 220}};
    play(n, 4);
  }

  void saved() {
    static const Note n[] = {{1320, 40}};
    play(n, 1);
  }


private:

  static const uint8_t MAX_NOTES = 8;

  uint8_t _pin = 0;
  bool _enabled = true;
  Note _notes[MAX_NOTES];
  uint8_t _count = 0;
  uint8_t _next = 0;
  unsigned long _noteEnd = 0;
};
