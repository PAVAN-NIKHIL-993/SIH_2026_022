#pragma once
#include "Arduino.h"
enum { WIFI_OFF, WIFI_STA, WIFI_AP, WIFI_AP_STA };
struct IPAddress { String toString() const { return "192.168.4.1"; } };
struct WiFiMock {
  void mode(int) {}
  bool softAP(const char *, const char *, int = 1, int = 0, int = 4) { return true; }
  IPAddress softAPIP() { return IPAddress(); }
};
extern WiFiMock WiFi;
