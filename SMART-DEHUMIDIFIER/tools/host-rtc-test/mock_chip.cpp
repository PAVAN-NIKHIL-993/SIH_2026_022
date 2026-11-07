#include "Arduino.h"
#include "Wire.h"
#include "control.h"
#include <time.h>

FakeRtc gRtc;
TwoWire Wire;

void TwoWire::beginTransmission(int addr) { addr_ = addr; txn_ = 0; }
size_t TwoWire::write(uint8_t b) { if (txn_ < (int)sizeof tx_) tx_[txn_++] = b; return 1; }
uint8_t TwoWire::endTransmission(bool sendStop) {
  (void)sendStop;                               // repeated START = same result
  if (addr_ != RTC_I2C_ADDR || !gRtc.present) return 2;   // address NACK
  if (txn_ == 0) return 0;                      // plain probe
  gRtc.ptr = tx_[0] & 0x3F;
  for (int i = 1; i < txn_; i++) {
    gRtc.reg[gRtc.ptr] = tx_[i];
    gRtc.writes[gRtc.ptr]++;
    gRtc.ptr = (uint8_t)((gRtc.ptr + 1) & 0x3F);
  }
  return 0;
}
uint8_t TwoWire::requestFrom(int addr, int n) {
  rxn_ = rxi_ = 0;
  if (addr != RTC_I2C_ADDR || !gRtc.present) return 0;
  for (int i = 0; i < n && i < (int)sizeof rx_; i++) {
    rx_[rxn_++] = gRtc.reg[gRtc.ptr];
    gRtc.ptr = (uint8_t)((gRtc.ptr + 1) & 0x3F);
  }
  return (uint8_t)rxn_;
}
int TwoWire::available() { return rxn_ - rxi_; }
int TwoWire::read() { return rxi_ < rxn_ ? rx_[rxi_++] : -1; }

// controllable system clock (settimeofday needs root in a container)
static time_t gFakeNow = 0;
void rtc_test_set_now(time_t t) { gFakeNow = t; }
time_t time(time_t *t) { if (t) *t = gFakeNow; return gFakeNow; }
