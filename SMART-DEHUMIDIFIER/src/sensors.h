/**
 * @file sensors.h
 * @brief Chamber sensors + the derived averages used for control.
 * Source #1: AHT10 at the TOP of the chamber (I2C0) - the hottest
 * point, so the safety cut reacts first there.
 * Source #2 (v2.0.9): S3 = DHT22 on the cool-return path (GPIO10);
 * classic = AHT10 #2 on I2C1. Same API either way, so averages,
 * RH-peak tracking and the safety cut work identically.
 */
#pragma once
#include <Arduino.h>
#include "config.h"
#include "aht10.h"
#if SENS2_DHT
#include "dht.h"
#endif

class SensorModule {
public:
  bool begin();                 // true if at least one sensor answered
  void update();                // call from loop() - non-blocking

  bool  anyOk()  const { return s1ok() || s2ok(); }
  bool  s1ok()   const { return _s1.ok(); }
  float t1()     const { return _s1.tempC(); }
  float h1()     const { return _s1.humRH(); }
#if SENS2_DHT
  bool  s2ok()   const { return dht::chamber.ok; }
  float t2()     const { return dht::chamber.t; }
  float h2()     const { return dht::chamber.h; }
#else
  bool  s2ok()   const { return _s2.ok(); }
  float t2()     const { return _s2.tempC(); }
  float h2()     const { return _s2.humRH(); }
#endif

  float tAvg()   const { return _tAvg; }   // mean of healthy sensors (heat PID)
  float hAvg()   const { return _hAvg; }
  float hMax()   const { return _hMax; }   // wettest sensor (fan law, completion)
  float tMax()   const { return _tMax; }   // hottest sensor (safety cut)

private:
  void recompute();
  AHT10 _s1;                    // chamber top (I2C0)
#if !SENS2_DHT
  AHT10 _s2;                    // classic: bottom of chamber (I2C1)
#endif
  float _tAvg = NAN, _hAvg = NAN, _hMax = NAN, _tMax = NAN;
};
