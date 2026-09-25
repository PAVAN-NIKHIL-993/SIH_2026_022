#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <cstdlib>

// ============ fake DS1302, faithful to the Maxim datasheet =============
// command byte (LSB first): bit7=1, bit6=0(clock), bits5-1=reg, bit0=R/W
// writes latch on SCLK RISING; reads: chip drives I/O on SCLK FALLING
// time registers 0-6 BCD auto-increment; reg7 = control (bit7 = WP)
// CH (halt) flag = bit 7 of the SECONDS register

struct FakeChip {
  uint8_t regs[8];
  uint8_t ram[31];
  int present;
};
extern FakeChip gChip;

extern int gPin[64];

struct MockP {

  int rst, sclk;
  int rises;              // SCLK rises since CE high
  int addr, readMode, isRam, regIdx, bitPos, payload, haveAddr;
};
extern MockP P;

static inline void ceEdge(bool hi) {
  if (!hi && P.rst) { P.rises = 0; P.haveAddr = 0; P.bitPos = 0; P.payload = 0; }
  P.rst = hi;
}

static void sclkEdge(bool hi) {
  if (!gChip.present) { P.sclk = hi; return; }
  if (getenv("MOCK_DBG")) fprintf(stderr, "edge %s: before rises=%d psclk=%d\n", hi?"RISE":"FALL", P.rises, P.sclk);
  if (hi && !P.sclk) {                        // ---- rising edge ----
    P.rises++;
    if (P.rises == 1) return;                 // START bit
    if (P.rises <= 9) {                       // command bits, LSB first
      P.addr = (P.addr >> 1) | (gPin[47] ? 0x80 : 0);
      if (P.rises == 9) {
        P.readMode = P.addr & 1;              // bit0 = R/W (datasheet)
        P.isRam = (P.addr >> 6) & 1;          // bit6 = RAM space
        P.regIdx = (P.addr >> 1) & 0x1F;      // bits5-1
        P.bitPos = 0;
        P.haveAddr = 1;
      }
      return;
    }
    if (!P.haveAddr) return;                  // STOP bit
    if (P.rst && !P.readMode) {               // writes latch on the rise
      P.payload = (P.payload >> 1) | (gPin[47] ? 0x80 : 0);
      if (P.bitPos == 7) {
        if (P.isRam) { if (P.regIdx < 31) gChip.ram[P.regIdx] = (uint8_t)P.payload; }
        else { int r = P.regIdx & 7;
               if (!(r != 7 && (gChip.regs[7] & 0x80))) gChip.regs[r] = (uint8_t)P.payload; }
        P.regIdx++; P.bitPos = 0; P.payload = 0;
      } else P.bitPos++;
    }
  } else if (!hi && P.sclk) {                 // ---- falling edge ----
    if (getenv("MOCK_DBG")) fprintf(stderr, "fall rises=%d rst=%d ha=%d rm=%d bp=%d reg=%d\n", P.rises, P.rst, P.haveAddr, P.readMode, P.bitPos, P.regIdx);
    if (P.rst && P.haveAddr && P.readMode && P.rises >= 10) {
      uint8_t val = P.isRam ? ((P.regIdx < 31) ? gChip.ram[P.regIdx] : 0)
                            : gChip.regs[P.regIdx & 7];
      gPin[47] = (val >> P.bitPos) & 1;
      if (++P.bitPos == 8) { P.regIdx++; P.bitPos = 0; }
    }
  }
  P.sclk = hi;
}

struct SerialMock {
  template <class... A> void printf(const char *f, A... a) { puts(f); }
  void println(const char *s) { fputs(s, stdout); fputc('\n', stdout); }
};
[[maybe_unused]] static SerialMock serialMockObj;   // not every TU prints
#define Serial serialMockObj
enum { INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2, INPUT_PULLDOWN = 3, HIGH = 1, LOW = 0 };
static inline void pinMode(int p, int m) { (void)p; (void)m; }
static inline void digitalWrite(int p, int v) {
  gPin[p] = v;
  if (p == 42) sclkEdge(v);
  if (p == 40) ceEdge(v);
}
static inline int digitalRead(int p) { return gPin[p]; }
static inline unsigned long millis() { return 0; }
static inline void delay(uint32_t ms) { (void)ms; }
static inline void delayMicroseconds(unsigned int us) { (void)us; }
