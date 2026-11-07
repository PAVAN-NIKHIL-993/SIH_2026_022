#pragma once
// Arduino TwoWire mock wired to the fake DS1307 in mock_chip.cpp.
#include <stdint.h>

// ============ fake DS1307, faithful to the Maxim/ADI datasheet ============
// 64 byte-wide registers behind one register pointer that auto-increments
// and wraps 0x3F -> 0x00. 0x00-0x06 = time (BCD; seconds bit7 = CH),
// 0x07 = control, 0x08-0x3F = battery-backed RAM. A write transaction's
// first data byte sets the pointer; the rest are stored. An absent chip -
// or one fed 3.3 V, below its 1.25 x VBAT power-fail trip - never ACKs.
struct FakeRtc {
  uint8_t reg[64];
  uint8_t ptr;
  int present;
  int writes[64];          // how many times each register was written
};
extern FakeRtc gRtc;

class TwoWire {
 public:
  void beginTransmission(int addr);
  size_t write(uint8_t b);
  uint8_t endTransmission(bool sendStop = true);
  uint8_t requestFrom(int addr, int n);
  int available();
  int read();
 private:
  int addr_ = 0, txn_ = 0, rxn_ = 0, rxi_ = 0;
  uint8_t tx_[80], rx_[80];
};
extern TwoWire Wire;
