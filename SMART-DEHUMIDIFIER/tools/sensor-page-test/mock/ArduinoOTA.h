#pragma once
#include "Arduino.h"
typedef int ota_error_t;
struct ArduinoOTAMock {
  void setHostname(const char *) {}
  void onStart(std::function<void()>) {}
  void onEnd(std::function<void()>) {}
  void onProgress(std::function<void(uint32_t, uint32_t)>) {}
  void onError(std::function<void(ota_error_t)>) {}
  void begin() {}
  void handle() {}
};
extern ArduinoOTAMock ArduinoOTA;
