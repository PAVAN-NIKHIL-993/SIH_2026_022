/**
 * @file battery.h
 * @brief Battery voltage / percentage via the resistor divider + low-side
 *        transistor pull-down on the ESP32 ADC, plus bypass-mode control.
 *
 * Hardware:
 *   battery(+) --[100k]--+----> GPIO36 (ADC)
 *                        |
 *                      [15k]
 *                        |
 *   GPIO19 --[1k]--| GATE   (2N7000 N-MOSFET, or NPN + 1k base resistor)
 *                  | D
 *                  +---- divider bottom node above
 *                  S ---> GND
 *
 * GPIO19 high  -> divider pulled to GND -> ADC sees the battery voltage.
 * GPIO19 low   -> divider floating      -> ~zero standby current.
 */
#pragma once
#include <Arduino.h>
#include "config.h"

struct BatteryProfile {
  const char *name;
  float v100;   // volts at 100 %
  float v0;     // volts at 0 %
};

class BatteryMonitor {
public:
  void begin();
  void update();                       // call from loop(), samples every 5 s

  float   volts()  const { return _v; }
  uint8_t percent()const { return _pct; }
  bool    bypass() const { return _bypass; }
  bool    valid()  const { return _valid; }

  /** Driven by the control loop: engages the bypass supply when the
   *  battery falls to/below the configured percentage. */
  void setBypass(bool on);

  /** Chemistry from saved settings (0=3S Li-ion .. 3=4S LiFePO4). */
  void setType(uint8_t t) { _type = t; }
  uint8_t type() const { return _type; }

  static const BatteryProfile &profile(uint8_t id);
  static uint8_t percentFor(float v, uint8_t type);

private:
  float readVoltsOnce();
  float _v = 0;
  uint8_t _pct = 0;
  uint8_t _type = DEF_BATT_TYPE;
  bool  _bypass = false;
  bool  _valid = false;
  uint32_t _last = 0;
};