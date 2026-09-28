#pragma once

#include <Arduino.h>
#include <Wire.h>

// Optional floor sensors, one per lane, so the board can score catches.
//
// Each lane gets a switch (or an IR break-beam module with an
// open-collector output) that pulls its pin to GND when a stick lands on
// the floor. The pins are on MCP23017 I/O expanders on the same I2C bus
// as the servo boards:
//
//   MCP23017 at 0x20: lanes 1-8 on GPA0-GPA7, lanes 9-16 on GPB0-GPB7
//   MCP23017 at 0x21: lanes 17-32 the same way (only with two servo boards)
//
// If no expander answers at 0x20 the game runs without scoring.

class LaneSensors {

public:

  // Returns true if at least the first expander was found
  bool begin() {

    _chips = 0;

    for (uint8_t i = 0; i < 2; i++) {

      if (!found(BASE_ADDRESS + i)) {
        break;
      }

      // All 16 pins inputs, internal pull-ups on
      writeReg(BASE_ADDRESS + i, IODIRA, 0xFF);
      writeReg(BASE_ADDRESS + i, IODIRB, 0xFF);
      writeReg(BASE_ADDRESS + i, GPPUA, 0xFF);
      writeReg(BASE_ADDRESS + i, GPPUB, 0xFF);

      _chips++;
    }

    return _chips > 0;
  }


  bool present() const {
    return _chips > 0;
  }


  // Number of lanes that have a sensor input (0, 16 or 32)
  int lanes() const {
    return _chips * 16;
  }


  // Bit i is set while lane i's sensor is active (pin pulled LOW)
  uint32_t read() {

    uint32_t bits = 0;

    for (uint8_t i = 0; i < _chips; i++) {

      Wire.beginTransmission(BASE_ADDRESS + i);
      Wire.write(GPIOA);

      if (Wire.endTransmission() != 0) {
        continue;
      }

      if (Wire.requestFrom((int)(BASE_ADDRESS + i), 2) != 2) {
        continue;
      }

      uint16_t pins = Wire.read();
      pins |= (uint16_t)Wire.read() << 8;

      // Pull-ups: idle HIGH, active LOW
      bits |= (uint32_t)(uint16_t)~pins << (16 * i);
    }

    return bits;
  }


private:

  static const uint8_t BASE_ADDRESS = 0x20;

  // MCP23017 registers (IOCON.BANK = 0, the power-on default)
  static const uint8_t IODIRA = 0x00;
  static const uint8_t IODIRB = 0x01;
  static const uint8_t GPPUA = 0x0C;
  static const uint8_t GPPUB = 0x0D;
  static const uint8_t GPIOA = 0x12;

  uint8_t _chips = 0;


  static bool found(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
  }


  static void writeReg(uint8_t address, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
  }
};
