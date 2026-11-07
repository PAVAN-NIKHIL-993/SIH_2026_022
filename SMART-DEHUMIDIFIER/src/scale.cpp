#include "scale.h"

#if SCALE_ENABLED

LoadScale scale;

void LoadScale::begin() {
  pinMode(PIN_SCALE_DOUT, INPUT);
  pinMode(PIN_SCALE_CLK,  OUTPUT);
  digitalWrite(PIN_SCALE_CLK, LOW);       // HX711 active (high > 60 us = power down)
  delay(1);
  _ok = (digitalRead(PIN_SCALE_DOUT) == LOW);   // data already pending = chip present
  Serial.printf("[scale] HX711 %s (CLK%d DOUT%d)\n",
                _ok ? "found" : "not responding (optional)",
                PIN_SCALE_CLK, PIN_SCALE_DOUT);
}

// one conversion: DOUT must be LOW (data ready), then 25 clock pulses.
// 24 data bits MSB-first on the falling edges, 25th sets gain 128 again.
bool LoadScale::readRaw(int32_t &v) {
  uint32_t t0 = millis();
  while (digitalRead(PIN_SCALE_DOUT) == HIGH) {
    if (millis() - t0 > 40) return false;      // no chip / not ready
  }
  int32_t d = 0;
  for (int i = 0; i < 24; i++) {
    digitalWrite(PIN_SCALE_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_SCALE_CLK, LOW);
    delayMicroseconds(1);
    d = (d << 1) | (digitalRead(PIN_SCALE_DOUT) ? 1 : 0);
  }
  digitalWrite(PIN_SCALE_CLK, HIGH);           // 25th pulse: next read ch A / x128
  delayMicroseconds(1);
  digitalWrite(PIN_SCALE_CLK, LOW);
  if (d & 0x800000) d |= (int32_t)0xFF000000;  // 24-bit two's complement
  v = d;
  return true;
}

void LoadScale::update() {
  uint32_t now = millis();
  if (_lastTry != 0 && now - _lastTry < 500) return;    // 2 Hz
  _lastTry = now ? now : 1;

  int32_t v;
  if (!readRaw(v)) {                                    // miss
    if (_ok && now - _lastOk > 10000) {                 // silent > 10 s
      _ok = false;
      Serial.println("[warn] weight scale stopped responding (optional)");
    }
    return;
  }
  bool wasOk = _ok;
  _ok = true; _lastOk = now; _raw = v;
  if (!wasOk) Serial.println("[warn] weight scale OK again");

  _g = ((float)(_raw - _offset)) / _factor;

  // rate over the rolling 5-minute window (sample every 30 s)
  static uint32_t lastSample = 0;
  if (lastSample == 0 || now - lastSample >= 30000) {
    lastSample = now;
    _hG[_hIdx] = _g; _hT[_hIdx] = now;
    _hIdx = (_hIdx + 1) % 10;
    if (_hN < 10) _hN++;
    if (_hN >= 3) {                                     // need >= 1 min span
      uint8_t newest = (_hIdx + 10 - 1) % 10;
      uint8_t oldest = _hIdx;                           // next write slot = oldest
      float dtMin = (_hT[newest] - _hT[oldest]) / 60000.0f;
      if (dtMin >= 0.9f) _rate = (_hG[newest] - _hG[oldest]) / dtMin;
    }
  }
}

void LoadScale::tare() {
  if (!_ok) return;
  _offset = _raw;                        // zero at the current raw reading
  _g = 0.0f;
  _hN = 0; _hIdx = 0; _rate = NAN;       // restart the rate window
  Serial.printf("[scale] tared (raw offset %ld)\n", (long)_offset);
}

void LoadScale::calibrate(float knownGrams) {
  if (!_ok || knownGrams <= 0) return;
  float f = (float)(_raw - _offset) / knownGrams;
  if (f > 0.05f && f < 200000.0f) {
    _factor = f;
    Serial.printf("[scale] calibrated: %.1f units/g (known %.0f g)\n",
                  (double)f, (double)knownGrams);
  } else {
    Serial.println("[scale] calibration rejected - put the weight on first");
  }
}

#else

LoadScale scale;      // stub instance (methods are inline no-ops)

#endif
