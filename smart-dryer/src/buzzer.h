/**
 * @file buzzer.h
 * @brief Buzzer pattern engine - the owner's 37-alert indication spec
 *        (v2.0.15) on a plain ACTIVE buzzer (one tone: cadence carries
 *        the identity; the 13-frequency palette needs a passive piezo).
 *
 * bz::play(BP)       one-shot pattern (higher priority preempts)
 * bz::startRepeat()  nagging pattern, re-fires every periodMs
 * bz::stopRepeat()   silence that pattern
 *
 * The classic v2.0 law stays (POWER-ON 2/s x3s, RUN 3s, END 5s, ERROR
 * 5s, DOOR 1s, MODE 3s) - it is the doc's Step 0/1/5/11 language.
 * Full table: docs/PARAMETERS.md.
 */
#pragma once
#include <Arduino.h>
#include "config.h"

enum class BP : uint8_t {
  NONE = 0,
  // -- owner spec #1..#37 (n/a: #10 tray prompts, #20 pause, #31/#32 charging)
  KEY, INVALID, TICK, MODE, SAVED, BACK,                    // 1-6
  DOOR_AJAR, NO_TRAYS, UNSTABLE, READY, BATT_LOW, BATT_CRIT,// 7-12(+13)
  SETPOINT, FAN_ON, DOOR_OPEN_RUN, MIDWAY, APPROACH, TIMEOUT5, //14-19
  WARN, CRITICAL, CRITICAL_R, RECOVERED, E05_LOAD,          // 23-26
  AP_UP, CLIENT, LOG_SAVED, STORE_FULL, BROWNOUT_RET,       // 27-30,21
  ANOMALY, MAINT, INIT_FAIL, COOL_DONE, SHUTDOWN, FACT_RESET, //22,33-37
  // -- classic law
  POWER_ON, CYCLE_START, CYCLE_DONE, ERROR, DOOR, MODE_CHANGE
};

namespace bz {
  void play(BP p);                        // one-shot
  void startRepeat(BP p, uint32_t periodMs);
  void stopRepeat(BP p);
  void stopAllRepeats();
  bool repeating(BP p);
  // classic v2.0 API (kept - old call sites)
  void powerOn(); void cycleStart(); void cycleDone(); void error();
  void door(); void modeChange();
}

#if BUZZER_ENABLED

class Buzzer {
public:
  void begin();
  void update();                       // call from loop()
  void beep(uint8_t n, uint16_t onMs, uint16_t offMs = 100);
                                       // legacy v2.0 cadence API (kept -
                                       // old call sites: stop/warn/key/menu)
  bool busy() const { return _p != BP::NONE || _bTot != 0; }
  // engine (used by bz::)
  void playPat(BP p);
  void addRepeat(BP p, uint32_t periodMs);
  void delRepeat(BP p);
  void stopAll();
  bool repeating(BP p);
  void pump();                         // re-fire due repeats
private:
  void drive(bool on);
  BP        _p = BP::NONE;             // playing pattern
  uint8_t   _seg = 0, _repLeft = 0;
  uint32_t  _tEdge = 0;
  bool      _on = false;
  // legacy beep() mini-cadence (n x onMs with offMs gaps)
  uint8_t   _bTot = 0, _bDone = 0;
  uint16_t  _bOnMs = 100, _bOffMs = 100;
  bool      _bOn = false;
  struct R { BP p; uint32_t period; uint32_t last; } _r[4] = {};
};

extern Buzzer buzzer;

#else

class Buzzer {
public:
  void begin() {}
  void update() {}
  void beep(uint8_t, uint16_t, uint16_t = 100) {}
  bool busy() const { return false; }
  void playPat(BP) {}
  void addRepeat(BP, uint32_t) {}
  void delRepeat(BP) {}
  void stopAll() {}
  bool repeating(BP) { return false; }
  void pump() {}
};

extern Buzzer buzzer;

#endif
  POWER_ON, CYCLE_START, CYCLE_DONE, ERROR, DOOR, MODE_CHANGE
};

namespace bz {
  void play(BP p);                        // one-shot
  void startRepeat(BP p, uint32_t periodMs);
  void stopRepeat(BP p);
  void stopAllRepeats();
  bool repeating(BP p);
  // classic v2.0 API (kept - old call sites)
  void powerOn(); void cycleStart(); void cycleDone(); void error();
  void door(); void modeChange();
}

#if BUZZER_ENABLED

class Buzzer {
public:
  void begin();
  void update();                       // call from loop()
  bool busy() const { return _p != BP::NONE; }
  // engine (used by bz::)
  void playPat(BP p);
  void addRepeat(BP p, uint32_t periodMs);
  void delRepeat(BP p);
  void stopAll();
  bool repeating(BP p);
  void pump();                         // re-fire due repeats
private:
  void drive(bool on);
  BP        _p = BP::NONE;             // playing pattern
  uint8_t   _seg = 0, _repLeft = 0;
  uint32_t  _tEdge = 0;
  bool      _on = false;
  struct R { BP p; uint32_t period; uint32_t last; } _r[4] = {};
};

extern Buzzer buzzer;

#else

class Buzzer {
public:
  void begin() {}
  void update() {}
  bool busy() const { return false; }
  void playPat(BP) {}
  void addRepeat(BP, uint32_t) {}
  void delRepeat(BP) {}
  void stopAll() {}
  bool repeating(BP) { return false; }
  void pump() {}
};

extern Buzzer buzzer;

#endif