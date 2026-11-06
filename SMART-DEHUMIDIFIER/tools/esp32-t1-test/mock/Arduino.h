#pragma once
// Host mock of the Arduino-ESP32 core for tools/esp32-t1-test.
//  - virtual microsecond clock (gUs); delay() advances it
//  - GPIO latch + DHT22 EMULATORS on chosen pins: the host's start pulse is
//    detected and the sensor answers with the real waveform (configurable
//    response delay and bit widths), so the sketch's bit-banging is tested
//  - LEDC capture (core 3.x pin API or core 2.x channel API)
//  - critical-section bookkeeping: delay()/Serial inside one = violation
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <string>
#include <functional>
#include <algorithm>

using std::isnan;
#define PROGMEM
#define F(x) (x)
#define DEC 10
#define INPUT 0x01
#define OUTPUT 0x03
#define PULLUP 0x04
#define INPUT_PULLUP 0x05
#define OPEN_DRAIN 0x10
#define OUTPUT_OPEN_DRAIN 0x13
#define LOW 0x0
#define HIGH 0x1
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 3
#endif

// ---- Arduino String (value semantics over std::string) --------------------
class String {
 public:
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &x) : s(x) {}
  String(char c) : s(1, c) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}
  String(float v, unsigned char d = 2) { fromD(v, d); }
  String(double v, unsigned char d = 2) { fromD(v, d); }
  const char *c_str() const { return s.c_str(); }
  unsigned length() const { return (unsigned)s.size(); }
  void reserve(unsigned n) { s.reserve(n); }
  long toInt() const { return atol(s.c_str()); }
  float toFloat() const { return (float)atof(s.c_str()); }
  String &operator+=(const String &x) { s += x.s; return *this; }
  String &operator+=(const char *x) { s += x; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  String &operator+=(int v) { s += std::to_string(v); return *this; }
  String &operator+=(unsigned v) { s += std::to_string(v); return *this; }
  String &operator+=(long v) { s += std::to_string(v); return *this; }
  String &operator+=(unsigned long v) { s += std::to_string(v); return *this; }
  bool operator==(const char *x) const { return s == x; }
 private:
  void fromD(double v, unsigned d) { char b[64]; snprintf(b, sizeof b, "%.*f", (int)d, v); s = b; }
};

// ---- clock + critical sections ---------------------------------------------
extern uint64_t gUs;
extern int gCritical, gCritViolations;
extern uint64_t gCritStart, gCritMaxUs;
// 32-bit like the ESP32's unsigned long: micros() wraps every 71.6 min,
// millis() every 49.7 days - long runs exercise the wrap-safety
inline uint32_t micros() { return (uint32_t)gUs; }
inline uint32_t millis() { return (uint32_t)(gUs / 1000); }
inline void delay(unsigned long ms) { if (gCritical) gCritViolations++; gUs += (uint64_t)ms * 1000; }
inline void delayMicroseconds(unsigned us) { gUs += us; }
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void portENTER_CRITICAL(portMUX_TYPE *) { if (gCritical++ == 0) gCritStart = gUs; }
inline void portEXIT_CRITICAL(portMUX_TYPE *) {
  if (--gCritical == 0 && gUs - gCritStart > gCritMaxUs) gCritMaxUs = gUs - gCritStart;
}

// ---- GPIO + DHT22 emulation --------------------------------------------------
struct DhtEmu {
  bool on = false;            // an emulated sensor sits on this pin
  bool present = true;        // false = unplugged (line just stays HIGH)
  double t = 25, h = 50;      // what it will report
  int tgo = 30;               // release -> response delay (datasheet 20-200 us)
  int zeroUs = 27, oneUs = 70;
  bool corrupt = false;       // wrong checksum
  bool hostLow = false;       // host is pulling the line low
  uint64_t lowStart = 0;
  uint64_t edge[90];          // level toggles (absolute us), starts HIGH
  int nEdge = 0;
  int frames = 0;
  int prev = HIGH;            // last level read (fast-mode bookkeeping)
};
extern DhtEmu gDht[64];
extern int gMode[64], gOut[64];
extern bool gDhtJump;         // fast mode: a read jumps to just after the next edge

inline void dhtRelease(DhtEmu &e) {
  e.hostLow = false;
  e.nEdge = 0;
  if (!e.present || gUs - e.lowStart < 800) return;       // too short = ignored
  uint8_t b[5];
  uint16_t hh = (uint16_t)lround(e.h * 10), tt = (uint16_t)lround(fabs(e.t) * 10);
  if (e.t < 0) tt |= 0x8000;
  b[0] = hh >> 8; b[1] = hh & 0xFF; b[2] = tt >> 8; b[3] = tt & 0xFF;
  b[4] = (uint8_t)(b[0] + b[1] + b[2] + b[3]);
  if (e.corrupt) b[4] ^= 0x01;
  uint64_t x = gUs + e.tgo;
  e.edge[e.nEdge++] = x;          // -> LOW  (response 80 us)
  e.edge[e.nEdge++] = x += 80;    // -> HIGH (response 80 us)
  e.edge[e.nEdge++] = x += 80;    // -> LOW  (bit 0 starts)
  for (int i = 0; i < 40; i++) {
    bool one = (b[i / 8] >> (7 - i % 8)) & 1;
    e.edge[e.nEdge++] = x += 50;                       // -> HIGH
    e.edge[e.nEdge++] = x += one ? e.oneUs : e.zeroUs; // -> LOW
  }
  e.edge[e.nEdge++] = x += 50;    // -> HIGH: released, idle
  e.frames++;
}

inline int dhtLevel(const DhtEmu &e, uint64_t at) {
  if (e.hostLow) return LOW;
  int n = 0;
  while (n < e.nEdge && e.edge[n] <= at) n++;
  return (n % 2) ? LOW : HIGH;
}

inline void pinMode(int p, int mode) {
  if (p < 0 || p >= 64) return;
  gMode[p] = mode;
  if (gDht[p].on && !(mode & 0x02) && gDht[p].hostLow) dhtRelease(gDht[p]);
}
inline void digitalWrite(int p, int v) {
  if (p < 0 || p >= 64) return;
  gOut[p] = v;
  DhtEmu &e = gDht[p];
  if (!e.on) return;
  if (v == LOW && (gMode[p] & 0x02)) {
    if (!e.hostLow) { e.hostLow = true; e.lowStart = gUs; e.nEdge = 0; }
  } else if (e.hostLow) {
    dhtRelease(e);              // open-drain HIGH / push-pull HIGH = release
  }
}
inline int digitalRead(int p) {
  if (p < 0 || p >= 64) return LOW;
  DhtEmu &e = gDht[p];
  if (!e.on) return gOut[p];
  int lvl;
  if (gDhtJump && !e.hostLow && dhtLevel(e, gUs) == e.prev) {
    // fast mode: a poll loop is waiting for a change - skip the identical
    // reads and land exactly ON the next edge (capped at 64 us, so timeouts
    // still trip on schedule). The read that sees the change does not move
    // the clock again, so measured pulse widths stay exact.
    uint64_t next = 0;
    for (int i = 0; i < e.nEdge; i++) if (e.edge[i] > gUs) { next = e.edge[i]; break; }
    gUs = next ? std::min<uint64_t>(next, gUs + 64) : gUs + 64;
    lvl = dhtLevel(e, gUs);
  } else {
    lvl = dhtLevel(e, gUs);
    gUs += 1;                   // dense mode: ~1 us per polling iteration
  }
  e.prev = lvl;
  return lvl;
}

// ---- LEDC --------------------------------------------------------------------
extern uint32_t gDuty[64], gLedcFreq;
extern int gLedcBits, gLedcPin, gChPin[16];
#if ESP_ARDUINO_VERSION_MAJOR >= 3
inline bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t bits) {
  gLedcPin = pin; gLedcFreq = freq; gLedcBits = bits; gDuty[pin] = 0; return true;
}
inline bool ledcWrite(uint8_t pin, uint32_t duty) { gDuty[pin] = duty; return true; }
#else
inline double ledcSetup(uint8_t ch, double freq, uint8_t bits) {
  gLedcFreq = (uint32_t)freq; gLedcBits = bits; (void)ch; return freq;
}
inline void ledcAttachPin(uint8_t pin, uint8_t ch) { gChPin[ch] = pin; gLedcPin = pin; gDuty[pin] = 0; }
inline void ledcWrite(uint8_t ch, uint32_t duty) { if (gChPin[ch] >= 0) gDuty[gChPin[ch]] = duty; }
#endif

// ---- Serial ------------------------------------------------------------------
extern bool gQuiet;
struct SerialMock {
  void begin(unsigned long) {}
  __attribute__((format(printf, 2, 3)))       // -Wformat checks the sketch
  int printf(const char *f, ...) {
    if (gCritical) gCritViolations++;
    if (gQuiet) return 0;
    va_list a; va_start(a, f); int n = vfprintf(stderr, f, a); va_end(a); return n;
  }
  void println(const char *x) { if (gCritical) gCritViolations++; if (!gQuiet) fprintf(stderr, "%s\n", x); }
  void print(const char *x) { if (gCritical) gCritViolations++; if (!gQuiet) fputs(x, stderr); }
};
extern SerialMock Serial;
