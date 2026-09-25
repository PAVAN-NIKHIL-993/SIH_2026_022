#include "keypad.h"

#if KEYPAD_ENABLED

I2CKeypad keypad;

// row on P0-P3 (drive one LOW at a time), column read back on P4-P7.
// Keys:  rows = digits blocks, cols = 4th column is A/B/C/D.
const char I2CKeypad::kMap[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'},
};

void I2CKeypad::begin(TwoWire &bus, uint8_t addr) {
  _bus = &bus;
  _addr = addr;
  _bus->beginTransmission(_addr);
  _bus->write(0xFF);                       // all pins high (idle)
  _ok = (_bus->endTransmission() == 0);
}

// one sweep: drive each row low in turn, read the columns
char I2CKeypad::scan() {
  for (uint8_t r = 0; r < 4; r++) {
    uint8_t out = 0xF0 | (uint8_t)~(1u << r);   // rows: only r low; cols high
    _bus->beginTransmission(_addr);
    _bus->write(out);
    if (_bus->endTransmission() != 0) { _ok = false; return 0; }
    _bus->requestFrom(_addr, (uint8_t)1);
    if (!_bus->available()) { _ok = false; return 0; }
    uint8_t in = _bus->read();
    _ok = true;
    uint8_t cols = (in >> 4) ^ 0x0F;       // pressed column reads LOW -> 1
    if (cols) {
      for (uint8_t c = 0; c < 4; c++)
        if (cols & (1u << c)) return kMap[r][c];
    }
  }
  return 0;
}

char I2CKeypad::update() {
  uint32_t now = millis();
  if (_tScan != 0 && now - _tScan < 15) return 0;   // scan every 15 ms
  _tScan = now ? now : 1;

  char k = scan();

  if (k != _prev) {                        // raw state changed: restart timer
    _prev = k;
    _tEdge = now;
  } else if (k != 0 && k == _prev && !_fired &&
             now - _tEdge >= 15) {         // stable for 15 ms -> accept
    _fired = true;
    _lastKey = k;
    return k;
  }
  if (k == 0) _fired = false;              // released: re-arm
  return 0;
}

#else

I2CKeypad keypad;                          // stub instance (methods inline no-ops)

#endif
#endif