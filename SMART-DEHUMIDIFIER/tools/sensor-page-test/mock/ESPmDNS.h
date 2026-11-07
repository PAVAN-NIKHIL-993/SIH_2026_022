#pragma once
#include "Arduino.h"
struct MDNSMock {
  bool begin(const char *) { return true; }
  void addService(const char *, const char *, int) {}
};
extern MDNSMock MDNS;
