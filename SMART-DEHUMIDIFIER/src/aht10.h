/**
 * @file aht10.h
 * @brief Minimal, dependency-free, NON-BLOCKING AHT10 driver.
 *
 * The AHT10 has a factory-fixed I2C address (0x38) - it cannot be changed -
 * so TWO sensors must sit on TWO separate I2C buses. The ESP32 has two
 * hardware I2C peripherals, which is exactly why sensor #1 lives on
 * Wire (21/22) and sensor #2 on Wire1 (25/26).
 *
 * Usage:
 *   AHT10 s1;  s1.begin(Wire);
 *   loop: if (s1.update(millis())) { float t = s1.tempC(); ... }
 */
#pragma once

#include <Arduino.h>
#include <Wire.h>

class AHT10 {
public:
  void begin(TwoWire &bus, uint8_t addr = 0x38, uint32_t periodMs = 2000);

  /** Call as often as possible. Returns true exactly once per new reading. */
  bool update(uint32_t now);

  bool    ok()      const { return _ok; }
  float   tempC()   const { return _t; }
  float   humRH()   const { return _h; }
  uint32_t lastOkMs() const { return _lastOk; }

private:
  enum class Stage : uint8_t { IDLE, MEASURING };
  bool cmd3(uint8_t a, uint8_t b, uint8_t c);
  bool readStatus(uint8_t &st);
  bool readResult();

  TwoWire *_bus = nullptr;
  uint8_t  _addr = 0x38;
  uint32_t _period = 2000;
  uint32_t _lastReq = 0;
  uint32_t _lastOk = 0;
  Stage    _stage = Stage::IDLE;
  bool     _ok = false;
  uint8_t  _fails = 0;
  uint8_t  _readTries = 0;
  float    _t = NAN, _h = NAN;
};
