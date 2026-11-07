/**
 * @file display.h
 * @brief Optional 3.5" SPI TFT status display (ILI9488, 480x320, non-touch).
 *
 * Library-free driver on remapped SPI pins (see config.h: the ESP32's
 * default SPI pins are all taken by the dryer hardware, so the display
 * uses the free/repurposed GPIO pool). DISPLAY_ENABLED 0 compiles it out.
 * Sharing rule: the display occupies GPIO 12/0/2/17/23 - it replaces the
 * (not fitted) relays and buzzer. Set DISPLAY_ENABLED 0 to free them.
 */
#pragma once
#include <Arduino.h>
#include "config.h"

#if DISPLAY_ENABLED
namespace display {
  void begin();                  // SPI up, panel init, first frame
  void update();                 // 1 Hz gated - splash / menu / status
  void splash(bool on);          // v2.0 boot splash (ARCHITECTS OF SOLUTIONS)
}
#else
namespace display {
  inline void begin() {}
  inline void update() {}
  inline void splash(bool) {}
}
#endif
