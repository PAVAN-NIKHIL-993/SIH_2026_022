#pragma once
#include "Arduino.h"
#define WIFI_AP 2
struct IPAddress { String toString() const { return String("192.168.4.1"); } };
struct WiFiMock {
  std::string ssid, pass;
  int channel = 0, maxClients = 0;
  void mode(int) {}
  bool softAP(const char *s, const char *p, int ch = 1, int hidden = 0, int maxc = 4) {
    ssid = s; pass = p; channel = ch; maxClients = maxc; (void)hidden; return true;
  }
  IPAddress softAPIP() { return IPAddress(); }
};
extern WiFiMock WiFi;
