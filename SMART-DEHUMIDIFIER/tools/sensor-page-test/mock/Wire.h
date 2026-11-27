#pragma once
#include "Arduino.h"
// Scriptable I2C bus: gI2cPresent[addr] = device answers; reads return a
// per-device byte stream (see mock.cpp) - enough to walk every code path.
extern bool gI2cPresent[128];
class TwoWire {
 public:
  bool begin(int = -1, int = -1, uint32_t = 0) { return true; }
  void setClock(uint32_t) {}
  void beginTransmission(int a) { addr = a; txn = 0; }
  size_t write(uint8_t b) { if (txn < 64) tx[txn++] = b; return 1; }
  uint8_t endTransmission(bool = true);
  uint8_t requestFrom(int a, int n);
  int available() { return rxn - rxi; }
  int read() { return rxi < rxn ? rx[rxi++] : -1; }
 private:
  int addr = 0, txn = 0, rxn = 0, rxi = 0;
  uint8_t tx[64], rx[64];
};
extern TwoWire Wire;
extern TwoWire Wire1;               // classic ESP32: AHT10 #2 on its own bus
