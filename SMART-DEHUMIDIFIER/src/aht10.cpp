#include "aht10.h"

// AHT10 command set (see Aosong datasheet)
static const uint8_t AHT_CMD_SOFTRESET  = 0xBA;
static const uint8_t AHT_CMD_CALIBRATE  = 0xE1; // AHT10 init/calibrate
static const uint8_t AHT_CMD_TRIGGER    = 0xAC;
static const uint8_t AHT_STATUS_BUSY    = 0x80;
static const uint8_t AHT_STATUS_CALIB   = 0x08;

void AHT10::begin(TwoWire &bus, uint8_t addr, uint32_t periodMs) {
  _bus = &bus;
  _addr = addr;
  _period = periodMs;
  _stage = Stage::IDLE;
  _ok = false;
  _fails = 0;
  _t = NAN; _h = NAN;

  // Soft reset, then send the calibration command (blocking, runs in setup())
  _bus->beginTransmission(_addr);
  _bus->write(AHT_CMD_SOFTRESET);
  _bus->endTransmission();
  delay(25);
  cmd3(AHT_CMD_CALIBRATE, 0x08, 0x00);
  uint8_t st = 0;
  if (readStatus(st) && (st & AHT_STATUS_CALIB)) {
    _ok = true;
    _lastOk = millis();
  }
}

bool AHT10::cmd3(uint8_t a, uint8_t b, uint8_t c) {
  _bus->beginTransmission(_addr);
  _bus->write(a); _bus->write(b); _bus->write(c);
  return (_bus->endTransmission() == 0);
}

bool AHT10::readStatus(uint8_t &st) {
  _bus->requestFrom(_addr, (uint8_t)1);
  if (!_bus->available()) return false;
  st = (uint8_t)_bus->read();
  return true;
}

bool AHT10::update(uint32_t now) {
  bool newData = false;

  switch (_stage) {
    case Stage::IDLE:
      if (now - _lastReq >= _period) {
        if (cmd3(AHT_CMD_TRIGGER, 0x33, 0x00)) {
          _lastReq = now;
          _stage = Stage::MEASURING;
        } else {                              // bus error -> 2 s backoff
          _lastReq = now;
          if (++_fails > 5) _ok = false;
        }
      }
      break;

    case Stage::MEASURING:                    // measurement takes ~80 ms
      if (now - _lastReq >= 90) {
        uint8_t st = 0;
        if (!readStatus(st)) {                // bus error -> bounded retry
          if (++_readTries >= 3) {            // then give up on this cycle
            _readTries = 0;
            _stage = Stage::IDLE;
            _lastReq = now;
            if (++_fails > 5) _ok = false;
          }
          break;
        }
        _readTries = 0;
        if (st & AHT_STATUS_BUSY) break;      // still busy, come back later
        if (readResult()) {                   // fresh numbers
          _fails = 0;
          _ok = true;
          _lastOk = now;
          newData = true;
        } else if (++_fails > 5) {
          _ok = false;
        }
        _stage = Stage::IDLE;
        _lastReq = now;                       // schedule next cycle from now
      }
      break;
  }

  // stale readings count as a failure too
  if (_ok && (now - _lastOk > _period * 10 + 5000)) _ok = false;
  return newData;
}

bool AHT10::readResult() {
  uint8_t b[6];
  _bus->requestFrom(_addr, (uint8_t)6);
  if (_bus->available() < 6) return false;
  for (auto &x : b) x = (uint8_t)_bus->read();

  if (b[0] & AHT_STATUS_BUSY) return false;

  // 20-bit humidity / 20-bit temperature, datasheet formulas
  uint32_t rawH = ((uint32_t)b[1] << 12) | ((uint32_t)b[2] << 4) | (b[3] >> 4);
  uint32_t rawT = (((uint32_t)b[3] & 0x0F) << 16) | ((uint32_t)b[4] << 8) | b[5];

  float h = rawH * 100.0f / 1048576.0f;
  float t = rawT * 200.0f / 1048576.0f - 50.0f;

  if (isnan(h) || isnan(t) || h < 0 || h > 105 || t < -40 || t > 90) return false;

  _h = h;
  _t = t;
  return true;
}