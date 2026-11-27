#pragma once
// Minimal Arduino core mock for the host-side DS1307 driver test.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <cstdlib>

struct SerialMock {
  template <class... A> void printf(const char *f, A... a) { ::printf(f, a...); }
  void println(const char *s) { fputs(s, stdout); fputc('\n', stdout); }
};
[[maybe_unused]] static SerialMock serialMockObj;   // not every TU prints
#define Serial serialMockObj
static inline unsigned long millis() { return 0; }
static inline void delay(uint32_t ms) { (void)ms; }
