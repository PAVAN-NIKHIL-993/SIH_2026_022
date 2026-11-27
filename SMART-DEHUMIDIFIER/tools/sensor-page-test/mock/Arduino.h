#pragma once
// Host-side Arduino/ESP32 mock - just enough for the sensor-test sketches
// to compile and run their JSON builders (tools/sensor-page-test).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <string>
#include <functional>
#include <algorithm>
#include <ctime>

using std::min; using std::max; using std::abs;
#define PROGMEM
#define F(x) (x)
#define HEX 16
#define DEC 10
#define BIN 2
enum { INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2, INPUT_PULLDOWN = 3 };
enum { LOW = 0, HIGH = 1 };
enum { ADC_0db, ADC_2_5db, ADC_6db, ADC_11db };
#ifndef constrain
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif

// ---- Arduino String (value semantics over std::string) --------------------
class String {
 public:
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &x) : s(x) {}
  String(char c) : s(1, c) {}
  // Arduino's signatures: base / decimal places are 'unsigned char'
  String(unsigned char v, unsigned char base = DEC) { fromUL(v, base); }
  String(int v, unsigned char base = DEC) { if (base == DEC) s = std::to_string(v); else fromUL((unsigned)v, base); }
  String(unsigned v, unsigned char base = DEC) { fromUL(v, base); }
  String(long v, unsigned char base = DEC) { if (base == DEC) s = std::to_string(v); else fromUL((unsigned long)v, base); }
  String(unsigned long v, unsigned char base = DEC) { fromUL(v, base); }
  String(long long v) : s(std::to_string(v)) {}
  String(unsigned long long v) : s(std::to_string(v)) {}
  String(float v, unsigned char d = 2) { fromD(v, d); }
  String(double v, unsigned char d = 2) { fromD(v, d); }
  const char *c_str() const { return s.c_str(); }
  unsigned length() const { return (unsigned)s.size(); }
  void reserve(unsigned n) { s.reserve(n); }
  long toInt() const { return atol(s.c_str()); }
  float toFloat() const { return (float)atof(s.c_str()); }
  int indexOf(const String &x) const { auto p = s.find(x.s); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(char c) const { auto p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
  String &operator+=(const String &x) { s += x.s; return *this; }
  String &operator+=(const char *x) { s += x; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  String &operator+=(int v) { s += std::to_string(v); return *this; }
  String &operator+=(unsigned v) { s += std::to_string(v); return *this; }
  String &operator+=(long v) { s += std::to_string(v); return *this; }
  String &operator+=(unsigned long v) { s += std::to_string(v); return *this; }
  bool operator==(const String &x) const { return s == x.s; }
  bool operator==(const char *x) const { return s == x; }
  bool operator!=(const String &x) const { return s != x.s; }
 private:
  void fromUL(unsigned long v, int base) {
    if (v == 0) { s = "0"; return; }
    char b[70]; int i = 69; b[i] = 0;
    while (v) { int d = (int)(v % base); b[--i] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v /= base; }
    s = b + i;
  }
  void fromD(double v, unsigned d) { char b[64]; snprintf(b, sizeof b, "%.*f", (int)d, v); s = b; }
};
inline String operator+(const String &a, const String &b) { String r(a); r += b; return r; }
inline String operator+(const String &a, const char *b) { String r(a); r += b; return r; }
inline String operator+(const char *a, const String &b) { String r(a); r += b; return r; }
inline String operator+(const String &a, char b) { String r(a); r += b; return r; }
inline String operator+(const String &a, int b) { String r(a); r += b; return r; }
inline String operator+(const String &a, unsigned b) { String r(a); r += b; return r; }
inline String operator+(const String &a, long b) { String r(a); r += b; return r; }
inline String operator+(const String &a, unsigned long b) { String r(a); r += b; return r; }

// ---- time: every call advances the fake clock, so timeout loops always end
extern uint64_t gMockUs;
inline unsigned long micros() { gMockUs += 3; return (unsigned long)gMockUs; }
inline unsigned long millis() { gMockUs += 3; return (unsigned long)(gMockUs / 1000); }
inline void delay(unsigned long ms) { gMockUs += (uint64_t)ms * 1000; }
inline void delayMicroseconds(unsigned int us) { gMockUs += us; }
inline void yield() {}
inline void noInterrupts() {}
inline void interrupts() {}
inline long random(long hi) { return hi > 0 ? rand() % hi : 0; }
inline long random(long lo, long hi) { return hi > lo ? lo + rand() % (hi - lo) : lo; }

// ---- pins: level per pin, scenario-driven ---------------------------------
extern int gPinLevel[64];
extern int gPinToggle;                 // 1 = inputs toggle (button presses etc.)
inline void pinMode(int, int) {}
inline void digitalWrite(int p, int v) { if (p >= 0 && p < 64) gPinLevel[p] = v; }
inline int digitalRead(int p) {
  if (p < 0 || p >= 64) return LOW;
  if (gPinToggle && (rand() % 97) == 0) gPinLevel[p] ^= 1;
  return gPinLevel[p];
}
inline int analogRead(int) { return 2600 + rand() % 12; }
inline void analogSetPinAttenuation(int, int) {}

// ---- Serial ------------------------------------------------------------------
struct SerialMock {
  void begin(unsigned long) {}
  int printf(const char *f, ...) { va_list a; va_start(a, f); int n = vfprintf(stderr, f, a); va_end(a); return n; }
  void println() { fputc('\n', stderr); }
  void println(const char *x) { fprintf(stderr, "%s\n", x); }
  void println(const String &x) { fprintf(stderr, "%s\n", x.c_str()); }
  void println(int x) { fprintf(stderr, "%d\n", x); }
  void print(const char *x) { fputs(x, stderr); }
  void print(const String &x) { fputs(x.c_str(), stderr); }
  int available() { return 0; }
  int read() { return -1; }
};
extern SerialMock Serial;

struct EspMock { const char *getChipModel() { return "ESP32-S3 (host mock)"; } };
extern EspMock ESP;
