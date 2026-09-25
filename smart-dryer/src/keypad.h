/**
 * @file keypad.h
 * @brief Optional 16-key hex keypad on a PCF8574 I2C backpack.
 *
 * Wired to the PCF8574: rows on P0-P3, columns on P4-P7. Shares Wire
 * (GPIO21/22) with AHT10 #1 - the PCF8574 answers at 0x20..0x26, the
 * AHT10 at 0x38, so no address conflict (buy PCF8574, NOT PCF8574A!).
 * KEYPAD_ENABLED 0 compiles it out completely.
 */
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"

#if KEYPAD_ENABLED

class I2CKeypad {
public:
  void begin(TwoWire &bus, uint8_t addr);   // scans once, sets ok()
  char update();                            // call from loop(); 15 ms debounce,
                                            // returns a key on NEW press only
  bool ok()   const { return _ok; }         // PCF8574 answered recently
  char last() const { return _lastKey; }    // last accepted key

private:
  char scan();                              // one matrix sweep, 0 if none
  static const char kMap[4][4];             // row/col -> key
  TwoWire *_bus = nullptr;
  uint8_t  _addr = 0x20;
  bool     _ok = false;
  char     _lastKey = 0;
  char     _raw = 0, _prev = 0;             // debounce state
  bool     _fired = false;                  // one event per press
  uint32_t _tEdge = 0, _tScan = 0;
};

extern I2CKeypad keypad;

#else

class I2CKeypad {                           // stub: compiled out
public:
  void begin(TwoWire &, uint8_t) {}
  char update() { return 0; }
  bool ok()   const { return false; }
  char last() const { return 0; }
};

extern I2CKeypad keypad;

#endif
