/**
 * @file control.h
 * @brief Dryer state machine + control laws + settings (persisted in NVS).
 *
 * State flow:
 *   IDLE -> RUNNING -> COOLDOWN -> DONE (relay opens, power cut)
 *     ^                                |
 *     +---------- "Power On" ----------+
 *   any state -> FAULT (sensor loss / over-temperature / battery empty)
 *
 * Heaters : BTS7960, PID holds chamber temp at settings.setTemp
 * Fans    : L298N, speed follows humidity band settings.humLow/humHigh
 * Relay   : cuts power to heater+fans when the cycle completes (or on fault)
 */
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "sensors.h"
#include "battery.h"
enum class DState : uint8_t { IDLE = 0, RUNNING, COOLDOWN, DONE, FAULT };
const char *stateName(DState s);

// ---- E-code fault system (owner spec, v2.0.14) --------------------------
// E01..E20 mirror the owner's documentation. sev 0 = WARNING (cycle runs
// on, operator nudged), sev 1 = CRITICAL (cycle halted, power cut,
// manual reset via Power On). Reserved (needs hardware): E08 door-sensor
// self-test, E09 blower RPM proof, E10 heater-current sense.
struct FaultRec {
  uint8_t  code = 0;             // 0 = none; 1..20 = E01..E20
  uint8_t  sev  = 0;
  bool     active = false;
  uint32_t sinceMs = 0;
  float    val = NAN;            // detection value (e.g. 92.4 C)
  float    limit = NAN;          // threshold it crossed
};
const char *eName(uint8_t code); // "HEATER OVERHEATING" ...

struct Settings {
  uint32_t magic;         // 'SDRY'
  uint16_t ver;           // bump when the struct changes
// on, operator nudged), sev 1 = CRITICAL (cycle halted, power cut,
// manual reset via Power On). Reserved (needs hardware): E08 door-sensor
// self-test, E09 blower RPM proof, E10 heater-current sense.
struct FaultRec {
  uint8_t  code = 0;             // 0 = none; 1..20 = E01..E20
  uint8_t  sev  = 0;
  bool   requireHum;      // also wait for humTarget before finishing
  uint32_t dryMinutes;    // manual drying duration
  uint8_t fanMin;         // % circulation speed inside the band
  uint8_t fanIn;          // % intake fan scaling of the computed duty
  uint8_t fanOut;         // % exhaust fan scaling of the computed duty
  uint8_t fanSlope;       // % duty added per RH point above humHigh
  uint8_t heaterMax;      // % soft cap on coil duty
  uint16_t cooldownSec;   // purge time before the relay opens
  uint8_t bypassPct;      // battery % that engages bypass

  uint8_t battType;       // 0=3S Li-ion 1=4S Li-ion 2=12V SLA 3=4S LiFePO4
  int16_t tzMinutes;      // local UTC offset in minutes (330 = IST)
  bool   smartVent;       // pause venting when outside RH >= chamber RH
  bool   boostHeat;       // full power to setTemp, then PID holds
  float  kp, ki, kd;      // heater PID gains
  bool   requireWeight;   // dry-to-weight: also wait for weight to settle
  float  weightRateG;     // g/min - "settled" means |rate| below this
  uint16_t weightMinY;    // minutes the rate must stay low before ending
  float  scaleCal;        // HX711 calibration: raw units per gram
  int32_t scaleOffset;    // HX711 tare offset (raw)
  float  targetG;         // g   target batch weight (0 = off): within 5 %
  // ---- v2.0.16: stick & paste calculator (owner spec) ------------------
  // The moisture is a property of the PASTE, so it lives here as a
  // recipe parameter. stickCount > 0 -> targetG is COMPUTED:
  //   target = N x stickWetG x (1 - (pasteWater% - targetMoist%)/100)
  uint16_t stickCount;    // sticks in the batch (0 = calculator off)
  float  stickWetG;       // g   average WET stick mass (typ. 2.5)
  float  pasteWaterPct;   // %   water in the raw paste (typ. 30-40)
  float  targetMoistPct;  // %   residual moisture wanted (typ. 8-10)
                          //     of it at time-up = complete, else warning
  uint8_t fanTrigRH;      // %RH burst trigger (spec: 60)
  uint8_t fanTrigMin;     // min RH high before the fan fires (spec: 1)
  uint8_t fanBurstS;      // s   fan run time per burst at 100 % (spec: 60)
  uint8_t mode;           // 0=AGARBATTI 1=USER DEFINED 2=SILICAGEL
};

Settings defaultSettings();
  float  humHigh;         // %RH    - fans ramp above
  float  humTarget;       // %RH    - optional completion criterion
  bool   requireHum;      // also wait for humTarget before finishing
  uint32_t dryMinutes;    // manual drying duration
  uint8_t fanMin;         // % circulation speed inside the band
  uint8_t fanIn;          // % intake fan scaling of the computed duty
  uint8_t fanOut;         // % exhaust fan scaling of the computed duty
  uint8_t fanSlope;       // % duty added per RH point above humHigh
  uint8_t heaterMax;      // % soft cap on coil duty
  uint16_t cooldownSec;   // purge time before the relay opens
  int16_t  fan;      // %
  int16_t  vb10;     // battery V  x10
  int16_t  bat;      // battery %
  int16_t  wt10;     // batch weight x10 g (INT16_MIN = no scale)
  int16_t  ot10 = INT16_MIN;   // outdoor temp x10 (DHT11; MIN = none)
  int16_t  oh10 = INT16_MIN;   // outdoor RH   x10 (DHT11; MIN = none)
};

// Latest outdoor weather - pushed by the phone's browser (it has mobile
  float  kp, ki, kd;      // heater PID gains
  bool   requireWeight;   // dry-to-weight: also wait for weight to settle
  float  weightRateG;     // g/min - "settled" means |rate| below this
  uint16_t weightMinY;    // minutes the rate must stay low before ending
  float  scaleCal;        // HX711 calibration: raw units per gram
  int32_t scaleOffset;    // HX711 tare offset (raw)
  float  targetG;         // g   target batch weight (0 = off): within 5 %
  // ---- v2.0.16: stick & paste calculator (owner spec) ------------------
  // The moisture is a property of the PASTE, so it lives here as a
  // recipe parameter. stickCount > 0 -> targetG is COMPUTED:
  //   target = N x stickWetG x (1 - (pasteWater% - targetMoist%)/100)
  uint16_t stickCount;    // sticks in the batch (0 = calculator off)

bool weatherFresh();           // received recently enough to trust
const char *outdoorSrc();      // "sensor" | "live" | "manual" | "stale" | "none"
bool getOutdoor(float &t, float &h);   // merged outdoor values (phone forecast/manual)

class Dryer {
public:
  uint8_t mode;           // 0=AGARBATTI 1=USER DEFINED 2=SILICAGEL
};

Settings defaultSettings();
Settings loadSettings();            // NVS if valid, else defaults
  void powerOn();                    // DONE/FAULT -> IDLE, relay closed again
  void addMinutes(int m);            // extend/shorten remaining time live
  void applySettings(const Settings &s);   // from the web UI
  void faultNow(const char *why);          // external safety stop (door!)
  void faultNowE(uint8_t ecode, const char *why,
                 float val = NAN, float limit = NAN);   // E-coded stop
  void warnE(uint8_t ecode, const char *why,
             float val = NAN, float limit = NAN);       // E-coded warning
  const FaultRec &faultRec() const { return _fr; }      // last critical
  const FaultRec &warnRec()  const { return _wr; }      // last warning
  bool warnActive() const { return _wr.active && _wr.code != 0; }
  void clearWarn(uint8_t code);        // condition fixed -> CLEARED + relief
  bool scaleLost()  const { return _scaleLost; }        // E15 degraded mode
  void applyMode(uint8_t m);               // 0 agarbatti / 1 user / 2 silica
  uint8_t mode() const { return _cfg.mode; }
  float    wtStartG()  const { return _wtStart; }     // weight at start
  float    finalG()    const { return _finalG; }      // weight at DONE
  float    moistureG() const                                 // grams removed
           { return (_wtStart - _finalG); }
  uint32_t endElapsedS() const { return _endElapsed; } // duration at DONE
  float    suggestMin() const { return _suggestMin; } // +min to target
  void setManualHeat(uint8_t pct);   // web knob: exact duty, auto-releases

  DState     state()    const { return _st; }
  const char*faultWhy() const { return _why; }
  uint8_t    heatDuty() const { return _heatDuty; }
  uint8_t    fanDuty()  const { return _fanDuty; }   // the law's demand
  uint8_t    fanInDuty()  const { return _fanInD; }  // actually applied
  uint8_t    fanOutDuty() const { return _fanOutD; }
  bool       boosting()  const { return _boosting; }  // max-power heat-up on
  bool       manualOn()  const;                       // knob window active
  uint8_t    manualPct() const { return _manPct; }    // last knob position
  uint16_t   manualLeftS() const;                     // 0 = back on automatic
  bool       relayOn()  const { return _relayOn; }
  uint32_t   elapsedS() const { return _elapsed; }      // RUNNING time
  uint32_t   remainingS() const;                        // 0 when not running
  int16_t  bat;      // battery %
  int16_t  wt10;     // batch weight x10 g (INT16_MIN = no scale)
  int16_t  ot10 = INT16_MIN;   // outdoor temp x10 (DHT11; MIN = none)
  int16_t  oh10 = INT16_MIN;   // outdoor RH   x10 (DHT11; MIN = none)
};

// Latest outdoor weather - pushed by the phone's browser (it has mobile
// data even while on the dryer hotspot). ESP itself never goes online.
struct Weather {
  float    tempC   = NAN;
  float    humRH   = NAN;
  float    rainPct = NAN;
  float    windKmh = NAN;
  uint8_t  code    = 100;      // WMO weather code (100 = unknown)
  Settings   _cfg;
  DState     _st = DState::IDLE;
  char       _why[40] = {0};
  float      _finalG = NAN;               // v2.0.15 completion summary
  uint32_t   _endElapsed = 0, _doneAt = 0;
  bool       _spReached = false, _midway = false, _anomWarned = false;
  float      _lastG = NAN;
  FaultRec   _fr, _wr;                     // E-code records (v2.0.14)
  bool       _scaleWasOk = false;          // E15 baseline
  uint32_t   _scaleLostSince = 0, _voltLowSince = 0,
             _voltHighSince = 0, _tHighWarnSince = 0;
  char       _endReason[40] = "completed";   // cycle-history entry reason
  bool       _scaleLost = false;             // E15: running without the scale
  bool       _singleWarned = false;          // one chamber sensor left
  uint32_t   _tStart = 0, _elapsed = 0, _cdStart = 0;
  uint8_t    _heatDuty = 0, _fanDuty = 0, _fanInD = 0, _fanOutD = 0;
  // v2.0 fan bursts + watchdogs + target weight
  uint32_t   _trigSince = 0;      // RH above the trigger since (burst timer)
  uint32_t   _fanBurstUntil = 0;  // fan 100 % until this millis
  uint32_t   _rhErrSince = 0;     // RH above humHigh since (fan error 5 min)
  uint32_t   _flatSince = 0;      // heater-failure: heat-up flatline since
  float      _flatT0 = NAN;       // temperature when the flatline started
  float      _wtStart = NAN;      // batch weight captured at start
  float      _suggestMin = -1;    // suggested extra minutes to target
  bool       _tWarned = false;    // T-5-min target warning fired
  bool       _relayOn = false;
  float      _integ = 0, _lastE = 0;
  bool       _boosting = false;
  uint32_t   _manUntil = 0;          // millis() deadline of the knob window
  uint8_t    _manPct = 0;            // knob duty the user asked for
  uint32_t   _sensFailSince = 0;
  uint32_t   _wtGoodSince = 0;      // weight-settle window start
  uint32_t   _lastTick = 0, _lastLog = 0;

  static LogRec _log[LOG_MAX];
  void begin(const Settings &s);
  void tick();                       // call from loop() - 1 s control cadence

  void start();                      // IDLE/DONE/FAULT -> RUNNING
  void stop();                       // user stop -> purge -> power cut
  void powerOn();                    // DONE/FAULT -> IDLE, relay closed again
  void addMinutes(int m);            // extend/shorten remaining time live
  void applySettings(const Settings &s);   // from the web UI
  void faultNow(const char *why);          // external safety stop (door!)
  void faultNowE(uint8_t ecode, const char *why,
                 float val = NAN, float limit = NAN);   // E-coded stop
  void warnE(uint8_t ecode, const char *why,
             float val = NAN, float limit = NAN);       // E-coded warning
  const FaultRec &faultRec() const { return _fr; }      // last critical
  const FaultRec &warnRec()  const { return _wr; }      // last warning
  bool warnActive() const { return _wr.active && _wr.code != 0; }
  void clearWarn(uint8_t code);        // condition fixed -> CLEARED + relief
  bool scaleLost()  const { return _scaleLost; }        // E15 degraded mode
  void applyMode(uint8_t m);               // 0 agarbatti / 1 user / 2 silica
  uint8_t mode() const { return _cfg.mode; }
  float    wtStartG()  const { return _wtStart; }     // weight at start
  float    finalG()    const { return _finalG; }      // weight at DONE
  float    moistureG() const                                 // grams removed
           { return (_wtStart - _finalG); }
  uint32_t endElapsedS() const { return _endElapsed; } // duration at DONE
  float    suggestMin() const { return _suggestMin; } // +min to target
  void setManualHeat(uint8_t pct);   // web knob: exact duty, auto-releases

  DState     state()    const { return _st; }
  const char*faultWhy() const { return _why; }
  uint8_t    heatDuty() const { return _heatDuty; }
  uint8_t    fanDuty()  const { return _fanDuty; }   // the law's demand
  uint8_t    fanInDuty()  const { return _fanInD; }  // actually applied
  uint8_t    fanOutDuty() const { return _fanOutD; }
  bool       boosting()  const { return _boosting; }  // max-power heat-up on
  bool       manualOn()  const;                       // knob window active
  uint8_t    manualPct() const { return _manPct; }    // last knob position
  uint16_t   manualLeftS() const;                     // 0 = back on automatic
  bool       relayOn()  const { return _relayOn; }
  uint32_t   elapsedS() const { return _elapsed; }      // RUNNING time
  uint32_t   remainingS() const;                        // 0 when not running

  // ---- data log (RAM ring buffer) ----
  uint16_t   logCount() const { return _logN; }
  uint16_t   logStart() const { return 0; }
  const LogRec &logAt(uint16_t i) const;                // 0 = oldest
  void      clearLog() { _logN = 0; _logHead = 0; }

private:
  void pidStep();
  void fanStep();
  void outputsAllOff();
  void setRelay(bool on);
  void raiseFault(const char *why);

  Settings   _cfg;
  DState     _st = DState::IDLE;
  char       _why[40] = {0};
  float      _finalG = NAN;               // v2.0.15 completion summary
  uint32_t   _endElapsed = 0, _doneAt = 0;
  bool       _spReached = false, _midway = false, _anomWarned = false;
  float      _lastG = NAN;
  FaultRec   _fr, _wr;                     // E-code records (v2.0.14)
  bool       _scaleWasOk = false;          // E15 baseline
  uint32_t   _scaleLostSince = 0, _voltLowSince = 0,
             _voltHighSince = 0, _tHighWarnSince = 0;
  char       _endReason[32] = "completed";   // cycle-history entry reason
  bool       _scaleLost = false;             // E15: running without the scale
  bool       _singleWarned = false;          // one chamber sensor left
  uint32_t   _tStart = 0, _elapsed = 0, _cdStart = 0;
  uint8_t    _heatDuty = 0, _fanDuty = 0, _fanInD = 0, _fanOutD = 0;
  // v2.0 fan bursts + watchdogs + target weight
  uint32_t   _trigSince = 0;      // RH above the trigger since (burst timer)
  uint32_t   _fanBurstUntil = 0;  // fan 100 % until this millis
  uint32_t   _rhErrSince = 0;     // RH above humHigh since (fan error 5 min)
  uint32_t   _flatSince = 0;      // heater-failure: heat-up flatline since
  float      _flatT0 = NAN;       // temperature when the flatline started
  float      _wtStart = NAN;      // batch weight captured at start
  float      _suggestMin = -1;    // suggested extra minutes to target
  bool       _tWarned = false;    // T-5-min target warning fired
  bool       _relayOn = false;
  float      _integ = 0, _lastE = 0;
  bool       _boosting = false;
  uint32_t   _manUntil = 0;          // millis() deadline of the knob window
  uint8_t    _manPct = 0;            // knob duty the user asked for
  uint32_t   _sensFailSince = 0;
  uint32_t   _wtGoodSince = 0;      // weight-settle window start
  uint32_t   _lastTick = 0, _lastLog = 0;

  static LogRec _log[LOG_MAX];
  uint16_t _logN = 0, _logHead = 0;
};

extern SensorModule    sensors;
extern BatteryMonitor  battery;
extern Dryer           dryer;
extern Settings        cfg;
extern Weather         weather;