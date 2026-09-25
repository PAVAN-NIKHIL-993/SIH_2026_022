/**
 * @file web.h
 * @brief ESP32 access-point web server: dashboard + JSON API + captive DNS.
 */
#pragma once
#include <WebServer.h>
#include <DNSServer.h>
namespace web {
  void begin();
  void handle();
  void clockBoot();     // restore last-saved wall clock after full power-down
  void clockSetManual(time_t ep);   // keypad menu / manual set: set + persist
  void clockTick();     // persist the clock every TIME_SAVE_MS (call in loop)
}
  void clockBoot();     // restore last-saved wall clock after full power-down
  void clockSetManual(time_t ep);   // keypad menu / manual set: set + persist
  void clockTick();     // persist the clock every TIME_SAVE_MS (call in loop)
}