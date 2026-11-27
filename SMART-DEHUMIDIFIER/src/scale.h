/**
 * @file scale.h
 * @brief Optional weigh scale: 2 x half-bridge load cells + HX711 24-bit
 *        ADC - the "dry to weight" sensor (calibration by the batch).
 *
 * Library-free bit-bang driver. RATE output mode (10 Hz), channel A
 * gain 128, 25 pulses per read. Tare + calibration factor live in the
 * Settings (NVS) so they survive reboots; the runtime object is fed
 * them by main at boot and after every settings save.
 *
 * PINS (see config.h): S3 variant uses dedicated GPIO 1 (CLK) + 2 (DOUT).
 * The classic ESP32 has NO free output pin while the display is fitted,
 * so the scale is compile-out there by default (options in config.h).
 */
#pragma once
#include <Arduino.h>
#include "config.h"

#if SCALE_ENABLED

class LoadScale {
public:
  void begin();                     // pins up; HX711 powers & self-checks
  void update();                    // call from loop; ~2 Hz reads, non-blocking-ish
  bool ok() const { return _ok; }   // HX711 is responding
  float grams() const { return _g; }        // calibrated + tared, NAN before first fix
  float rate() const { return _rate; }      // g/min over the last ~5 min, NAN if unknown
  void tare();                      // zero at the current reading
  void calibrate(float knownGrams); // set factor with a known weight on top
  float calFactor() const { return _factor; }
  void setFactor(float f) { _factor = f > 0.0f ? f : 1.0f; }
  void setOffset(int32_t o) { _offset = o; }
  int32_t offset() const { return _offset; }
private:
  bool readRaw(int32_t &v);         // one 24-bit conversion
  int32_t _raw = 0, _offset = 0;
  float   _factor = 1.0f;           // raw units per gram
  float   _g = NAN, _rate = NAN;
  uint32_t _lastTry = 0, _lastOk = 0;
  bool    _ok = false;
  // rolling history for the rate: 10 samples x 30 s = 5 min window
  float   _hG[10] = {0};
  uint32_t _hT[10] = {0};
  uint8_t _hN = 0, _hIdx = 0;
};

extern LoadScale scale;

#else

class LoadScale {                   // stub: compiled out
public:
  void begin() {}
  void update() {}
  bool ok() const { return false; }
  float grams() const { return NAN; }
  float rate() const { return NAN; }
  void tare() {}
  void calibrate(float) {}
  float calFactor() const { return 1.0f; }
  void setFactor(float) {}
  void setOffset(int32_t) {}
  int32_t offset() const { return 0; }
};

extern LoadScale scale;

#endif
