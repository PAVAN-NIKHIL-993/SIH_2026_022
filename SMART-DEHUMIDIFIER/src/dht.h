/**
 * @file dht.h
 * @brief DHT humidity/temperature sensors - library-free, one wire each.
 *
 * v2.0.9 layout (S3): a DHT22 on the cool-return path is chamber
 * source #2 (averages + RH-peak keep working, one AHT10 fewer to buy);
 * a DHT11 outdoors feeds weather / smart venting. Both models decode
 * through the same 40-bit frame - the DHT11 sends whole degrees/%,
 * the DHT22 tenths (the is11 flag picks the decode).
 *
 * Wiring per sensor: VCC->3V3, GND->GND, DATA->GPIO + 10 k pull-up
 * DATA->3V3 (resistor at the sensor end of long cables). One frame
 * blocks ~5 ms; chamber cadence 3 s (spec min 2 s), outdoor 10 s.
 * 3 missed frames -> ok=false (last values kept for display).
 * pin < 0 = not fitted -> every call is a no-op (classic variant).
 */
#pragma once
#include <Arduino.h>
#include "config.h"

namespace dht {
  struct Dev {
    int8_t   pin    = -1;        // DATA GPIO (-1 = not fitted)
    bool     is11   = false;     // true = DHT11 decode
    float    t      = NAN;       // deg C, last good frame
    float    h      = NAN;       // %RH
    bool     ok     = false;
    uint32_t gen    = 0;         // bumps on every good frame
    uint8_t  miss   = 0;         // consecutive failed frames
    uint32_t last   = 0;
    uint32_t everyMs= 10000;
    void update();               // cadence gate + one frame
  };
  extern Dev chamber;            // DHT22, cool-return path (chamber #2)
  extern Dev outdoor;            // DHT11, outside in shade (weather)
  void begin();                  // pins up + boot log
  void update();                 // both devices
}
