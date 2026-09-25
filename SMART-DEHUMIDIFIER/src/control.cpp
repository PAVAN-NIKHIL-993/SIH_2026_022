#include "control.h"
#include "pwm.h"
#include "cyclelog.h"
#include "buzzer.h"
#include "scale.h"
#include "door.h"
#include "dht.h"

// ---------------------------------------------------------------------
//  Module instances (defined here, used everywhere)
// ---------------------------------------------------------------------
SensorModule   sensors;
BatteryMonitor battery;
Dryer          dryer;
Settings       cfg;
Weather        weather;

bool weatherFresh() {
  return weather.rxMs != 0 && (millis() - weather.rxMs) < WX_STALE_MS;
}

// Effective outdoor conditions. Priority:
//   1. fresh forecast relayed by the phone (or manual entry)
//   2. stale forecast (better than nothing, flagged as such)
const char *outdoorSrc() {
  if (weatherFresh())            return weather.manual ? "manual" : "live";
  if (weather.rxMs)              return "stale";
  return "none";
}

bool getOutdoor(float &t, float &h) {
  // priority: MEASURED DHT11 (live) > phone-relayed weather > none
  if (dht::outdoor.ok && !isnan(dht::outdoor.t) && !isnan(dht::outdoor.h)) {
    t = dht::outdoor.t;  h = dht::outdoor.h;  return true;
  }
  if (weatherFresh() || weather.rxMs) {
    if (!isnan(weather.humRH)) { t = weather.tempC;  h = weather.humRH;  return true; }
  }
  return false;
}

LogRec Dryer::_log[LOG_MAX];

static const uint32_t kMagic = 0x53445259;   // "SDRY"
static const uint16_t kVer  = 8;   // v8: +stick/paste calculator fields
static const char *kPrefs = "dryer";

// ---------------------------------------------------------------------
//  Settings: defaults / NVS load / save
// ---------------------------------------------------------------------
Settings defaultSettings() {
  Settings s{};
  s.magic = kMagic; s.ver = kVer;
  s.setTemp   = DEF_SET_TEMP;
  s.tempHyst  = DEF_TEMP_HYST;
  s.maxTemp   = DEF_MAX_TEMP;
  s.humLow    = DEF_HUM_LOW;
  s.humHigh   = DEF_HUM_HIGH;
  s.humTarget = DEF_HUM_TARGET;
  s.requireHum= DEF_REQUIRE_HUM;
  s.dryMinutes= DEF_DRY_MINUTES;
  s.fanMin    = DEF_FAN_MIN;
  s.fanIn     = DEF_FAN_IN;
  s.fanOut    = DEF_FAN_OUT;
  s.fanSlope  = DEF_FAN_SLOPE;
  s.heaterMax = DEF_HEATER_MAX;
  s.cooldownSec = DEF_COOLDOWN_S;
  s.bypassPct = DEF_BYPASS_PCT;
  s.cutoffPct = DEF_CUTOFF_PCT;
  s.battType  = DEF_BATT_TYPE;
  s.tzMinutes = DEF_TZ_MINUTES;
  s.smartVent = DEF_SMART_VENT;
  s.boostHeat = DEF_BOOST_HEAT;
  s.kp = DEF_KP; s.ki = DEF_KI; s.kd = DEF_KD;
  s.requireWeight = false;         // dry-to-weight gate (opt-in)
  s.weightRateG   = 2.0f;          // g/min below this = "settled"
  s.weightMinY    = 10;            // minutes stable before ending
  s.scaleCal      = 1.0f;          // recalibrate via the website
  s.scaleOffset   = 0;
  s.targetG       = DEF_TARGET_G;  // v2.0 target weight (0 = off)
  s.stickCount    = 0;             // v2.0.16 calculator off by default
  s.stickWetG     = 2.5f;          // avg wet stick (g)
  s.pasteWaterPct = 35.0f;         // water in the paste (30-40 typical)
  s.targetMoistPct= 10.0f;         // finished residual moisture (8-10)
  s.fanTrigRH     = DEF_FAN_TRIG_RH;
  s.fanTrigMin    = DEF_FAN_TRIG_MIN;
  s.fanBurstS     = DEF_FAN_BURST_S;
  s.mode          = 0;             // AGARBATTI
  return s;
}

Settings loadSettings() {
  Preferences p;
  p.begin(kPrefs, true);
  Settings s{};
  size_t n = p.getBytes("cfg", &s, sizeof(s));
  p.end();
  if (n == sizeof(s) && s.magic == kMagic && s.ver == kVer) return s;
  return defaultSettings();
}

bool saveSettings(const Settings &s) {
  Preferences p;
  p.begin(kPrefs, false);
  size_t n = p.putBytes("cfg", &s, sizeof(s));
  p.end();
  return n == sizeof(s);
}

const char *stateName(DState s) {
  switch (s) {
    case DState::IDLE:     return "IDLE";
    case DState::RUNNING:  return "DRYING";
    case DState::COOLDOWN: return "PURGING";
    case DState::DONE:     return "DONE";
    case DState::FAULT:    return "FAULT";
  }
  return "?";
}

// ---------------------------------------------------------------------
//  Hardware outputs
// ---------------------------------------------------------------------
static inline int dutyOf(uint8_t pct) {           // % -> 0..1023
  return (int)pct * ((1 << PWM_RES_BITS) - 1) / 100;
}

void Dryer::setRelay(bool on) {
  _relayOn = on;                      // state the website shows
#if RELAYS_ENABLED
#if RELAY_ACTIVE_LOW
  digitalWrite(PIN_LOAD_RELAY, on ? LOW : HIGH);
#else
  digitalWrite(PIN_LOAD_RELAY, on ? HIGH : LOW);
#endif
#else
  (void)0;                            // no relay fitted: state only
#endif
}

void Dryer::outputsAllOff() {
  _heatDuty = 0; _fanDuty = 0; _fanInD = 0; _fanOutD = 0;
  _manUntil = 0;                        // knob window dies with the outputs
  pwmWritePin(PIN_BTS_RPWM, 0);
#if PIN_BTS_LPWM >= 0
  pwmWritePin(PIN_BTS_LPWM, 0);
#endif
  digitalWrite(PIN_BTS_EN, LOW);
  pwmWritePin(PIN_L298_ENA, 0);
  pwmWritePin(PIN_L298_ENB, 0);
}

// ---------------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------------
void Dryer::begin(const Settings &s) {
  _cfg = s;

  // heater pins
  pinMode(PIN_BTS_EN, OUTPUT);
#if PIN_BTS_LPWM >= 0
  pinMode(PIN_BTS_LPWM, OUTPUT);
  digitalWrite(PIN_BTS_LPWM, LOW);
#endif
  pwmInitPin(PIN_BTS_RPWM, HEATER_PWM_FREQ, PWM_RES_BITS);

  // fan pin - speed on ENB only. Direction is HARD-WIRED on the module
  // (v2.0.10: IN1/IN3 -> 5V, IN2/IN4 -> GND); the IN pins are -1 and
  // ENB LOW is a real off (both bridge switches open, fan coasts).
  pwmInitPin(PIN_L298_ENA, FAN_PWM_FREQ, PWM_RES_BITS);   // no-op at -1
  pwmInitPin(PIN_L298_ENB, FAN_PWM_FREQ, PWM_RES_BITS);

#if RELAYS_ENABLED
  pinMode(PIN_LOAD_RELAY, OUTPUT);
#endif
  setRelay(false);                 // (no-op without the relay fitted)

  battery.setType(_cfg.battType);
  _st = DState::IDLE;
}

void Dryer::applySettings(const Settings &s) {
  _cfg = s;                              // LIVE: takes effect on the next
  battery.setType(_cfg.battType);        // 1 s tick - even mid-cycle
  // -- v2.0.16: stick/paste calculator overrides the raw target -------
  // moisture belongs to the paste: N x wet x (1 - (water% - final%)/100)
  if (_cfg.stickCount > 0 && _cfg.stickWetG >= 0.5f) {
    float loss = (_cfg.pasteWaterPct - _cfg.targetMoistPct) / 100.0f;
    if (loss < 0) loss = 0;
    if (loss > 0.8f) loss = 0.8f;
    float t = (float)_cfg.stickCount * _cfg.stickWetG * (1.0f - loss);
    if (t >= 50.0f && t <= 9000.0f) {
      _cfg.targetG = roundf(t);
      Serial.printf("[cfg] target %.0f g COMPUTED: %u sticks x %.2f g wet,"
                    " paste %.0f%% water -> %.0f%% final\n",
                    (double)t, (unsigned)_cfg.stickCount,
                    (double)_cfg.stickWetG,
                    (double)_cfg.pasteWaterPct, (double)_cfg.targetMoistPct);
    }
  }
  Serial.printf("[cfg] LIVE settings applied: setTemp=%.1fC time=%umin fans=%u/%u%%\n",
                _cfg.setTemp, (unsigned)_cfg.dryMinutes, _cfg.fanIn, _cfg.fanOut);
}

void Dryer::start() {
  if (_st == DState::RUNNING) return;
  if (!door::calibrated() && scale.ok()) {   // gate only when the scale WORKS
    Serial.println(F("[start] REFUSED: calibrate the weigh scale first "
                     "(known weight on the trays) - start stays locked"));
    return;
  }
  if (!scale.ok()) {                   // v2.0.17: a dead scale != a dead dryer
    _scaleLost = true;
    warnE(15, "scale absent/dead - DEGRADED: time + RH termination only");
  }
  // v2.0: remember the batch weight at start (target-weight tracking)
  _wtStart = (scale.ok() && !isnan(scale.grams())) ? scale.grams()
             : (door::batchG() > 50.0f ? door::batchG() : NAN);
  // -- #7 door ajar at START (owner spec) --------------------------------
  if (door::fitted() && !door::closed()) {
    bz::startRepeat(BP::DOOR_AJAR, 2000);        // nag until it shuts
    Serial.println(F("[start] REFUSED: door is OPEN - close it first"));
    return;
  }
  // -- #8 nothing on the trays -------------------------------------------
  if (scale.ok() && !isnan(scale.grams()) && scale.grams() < 50.0f) {
    bz::startRepeat(BP::NO_TRAYS, 3000);
    Serial.println(F("[start] REFUSED: trays look EMPTY - load the batch"));
    return;
  }
  // -- E-code start gates (owner spec v2.0.14) --------------------------
  if (!isnan(_wtStart)) {
    if (_wtStart > 9500.0f) {                       // E16 overload
      warnE(16, "batch exceeds the 2x5 kg cells", _wtStart, 9500.0f);
      bz::play(BP::INVALID);
      Serial.println(F("[start] REFUSED: E16 - remove the excess load"));
      return;
    }
    if (_wtStart < -50.0f) {                        // E05 scale implausible
      warnE(5, "scale reads negative - check the load cells", _wtStart, 0.0f);
      bz::play(BP::E05_LOAD);                      // #26: 2 s held tone
      Serial.println(F("[start] REFUSED: E05 - load-cell error"));
      return;
    }
    if (_cfg.targetG > 0.0f) {                      // E17 invalid target
      if (_cfg.targetG >= _wtStart) {
        bz::play(BP::INVALID);
        warnE(17, "target must be BELOW the batch weight",
              _cfg.targetG, _wtStart);
        Serial.println(F("[start] REFUSED: E17 - target weight >= batch"));
        return;
      }
      if (_cfg.targetG < 50.0f) {
        bz::play(BP::INVALID);
        warnE(17, "target below the 50 g minimum", _cfg.targetG, 50.0f);
        Serial.println(F("[start] REFUSED: E17 - target below minimum"));
        return;
      }
    }
  }
  buzzer.stopAll();                                // setup nags end here
  if (battery.valid() && battery.percent() < 20)
    bz::play(BP::BATT_LOW);                         // #12: low battery at start
  _spReached = false; _midway = false; _anomWarned = false; _lastG = NAN;
  _fr.active = false;  _wr.active = false;          // fresh cycle, fresh E-records
  // -- E-code start gates (owner spec v2.0.14) --------------------------
  if (!isnan(_wtStart)) {
    if (_wtStart > 9500.0f) {                       // E16 overload
      warnE(16, "batch exceeds the 2x5 kg cells", _wtStart, 9500.0f);
      Serial.println(F("[start] REFUSED: E16 - remove the excess load"));
      return;
    }
    if (_wtStart < -50.0f) {                        // E05 scale implausible
      warnE(5, "scale reads negative - check the load cells", _wtStart, 0.0f);
      Serial.println(F("[start] REFUSED: E05 - load-cell error"));
      return;
    }
    if (_cfg.targetG > 0.0f) {                      // E17 invalid target
      if (_cfg.targetG >= _wtStart) {
        warnE(17, "target must be BELOW the batch weight",
              _cfg.targetG, _wtStart);
        Serial.println(F("[start] REFUSED: E17 - target weight >= batch"));
        return;
      }
      if (_cfg.targetG < 50.0f) {
        warnE(17, "target below the 50 g minimum", _cfg.targetG, 50.0f);
        Serial.println(F("[start] REFUSED: E17 - target below minimum"));
        return;
      }
    }
  }
  _fr.active = false;  _wr.active = false;          // fresh cycle, fresh E-records
  _tWarned = false; _suggestMin = -1;
  _trigSince = 0; _fanBurstUntil = 0; _rhErrSince = 0; _flatSince = 0;
  _scaleLostSince = 0; _voltLowSince = 0; _voltHighSince = 0;
  _tHighWarnSince = 0;  _scaleWasOk = scale.ok();
  _scaleLostSince = 0; _voltLowSince = 0; _voltHighSince = 0;
  _tHighWarnSince = 0;  _scaleWasOk = scale.ok();
  _integ = 0; _lastE = 0;
  clearLog();
  _lastLog = 0;
  _tStart = millis();
  _elapsed = 0;
  _why[0] = 0;
  setRelay(true);
  battery.setBypass(false);
  cyclelog::start();                  // history entry opens
  bz::cycleStart();                   // start: 3 s continuous (spec)
  _st = DState::RUNNING;
  Serial.printf("[dryer] START  %.1f C / RH %0.f-%0.f%% / %u min\n",
                _cfg.setTemp, _cfg.humLow, _cfg.humHigh,
                (unsigned)_cfg.dryMinutes);
}

void Dryer::stop() {               // user pressed STOP -> purge, then cut
  if (_st == DState::RUNNING) {
    strncpy(_endReason, "stopped", sizeof(_endReason));
    buzzer.beep(1, 500);              // stop: one long beep
    _manUntil = 0;                    // knob override ends with the cycle
    _wtGoodSince = 0;                 // weight-settle timer resets too
    _st = DState::COOLDOWN;
    _cdStart = millis();
    Serial.println("[dryer] user STOP -> purge");
  }
}

void Dryer::setManualHeat(uint8_t pct) {  // web knob turned
  if (pct > 100) pct = 100;
  _manPct  = pct;
  _manUntil = millis() + MANUAL_HEAT_MS;  // every turn re-arms the window
  Serial.printf("[man] manual heat %u%% - automatic resumes in %u s\n",
                (unsigned)pct, (unsigned)(MANUAL_HEAT_MS / 1000UL));
}

bool Dryer::manualOn() const {
  return _manUntil != 0 && millis() < _manUntil;
}

uint16_t Dryer::manualLeftS() const {
  if (_manUntil == 0 || millis() >= _manUntil) return 0;
  return (uint16_t)((_manUntil - millis() + 999UL) / 1000UL);
}

void Dryer::powerOn() {            // re-energise after DONE / FAULT
  _why[0] = 0;
  _fr.active = false;  _wr.active = false;   // E-records -> CLEARED view
  _fr.active = false;  _wr.active = false;   // E-records -> CLEARED view
  _wr.active = false;
  buzzer.stopAll();                   // silence every nagging pattern
  bz::play(BP::RECOVERED);            // #25: relief tone (owner spec)
  _st = DState::IDLE;
  setRelay(true);
  Serial.println("[dryer] power ON -> IDLE");
}

void Dryer::addMinutes(int m) {
  if (_st != DState::RUNNING) return;
  // +m minutes => pretend the cycle started m minutes later in the past:
  //   newElapsed = elapsed - m*60  =>  tStart = now - newElapsed*1000
  int32_t newElapsed = (int32_t)_elapsed - (int32_t)m * 60;
  if (newElapsed < 0) newElapsed = 0;
  _tStart = millis() - (uint32_t)newElapsed * 1000UL;   // shift the epoch
  _elapsed = (uint32_t)newElapsed;
}

void Dryer::faultNow(const char *why) { raiseFault(why); }   // door & co.

// ---- E-code fault system (owner spec, v2.0.14) --------------------------
const char *eName(uint8_t c) {
  switch (c) {
    case 1:  return "HEATER OVERHEATING";
    case 2:  return "POWER SOURCE ERROR";
    case 3:  return "HEATER FAILURE";
    case 4:  return "TEMP SENSOR FAILURE";
    case 5:  return "LOAD CELL ERROR";
    case 6:  return "FAN / RH ERROR";
    case 7:  return "DOOR OPEN";
    case 8:  return "DOOR SENSOR FAULT";
    case 9:  return "BLOWER FAILURE";
    case 10: return "HEATER CURRENT FAULT";
    case 11: return "RELAY / FEED FAULT";
    case 12: return "LOW BATTERY";
    case 13: return "LOW SUPPLY VOLTAGE";
    case 14: return "HIGH SUPPLY VOLTAGE";
    case 15: return "LOAD CELL COMM ERROR";
    case 16: return "LOAD OVERLOAD";
    case 17: return "INVALID TARGET";
    case 18: return "INVALID PARAMETERS";
    case 19: return "CYCLE TIMEOUT";
    case 20: return "TEMP OUT OF RANGE";
  }
  return "";
}

void Dryer::warnE(uint8_t code, const char *why, float val, float limit) {
  if (_wr.active && _wr.code == code) return;         // dedupe repeats
  _wr = FaultRec();
  _wr.code = code;  _wr.sev = 0;  _wr.active = true;
  _wr.sinceMs = millis();  _wr.val = val;  _wr.limit = limit;
  Serial.printf("[warn] E%02u %s - %s\n", code, eName(code), why);
  bz::startRepeat(BP::WARN, 15000);      // #23: 2x150 every 15 s while active
}

void Dryer::clearWarn(uint8_t code) {
  if (_wr.active && _wr.code == code) {
    _wr.active = false;                  // banner -> CLEARED
    bz::stopRepeat(BP::WARN);
    bz::play(BP::RECOVERED);             // #25: self-recovered
    Serial.printf("[dryer] E%02u cleared - conditions back to normal\n", code);
  }
}

void Dryer::faultNowE(uint8_t code, const char *why, float val, float limit) {
  _fr = FaultRec();
  _fr.code = code;  _fr.sev = 1;  _fr.active = true;
  _fr.sinceMs = millis();  _fr.val = val;  _fr.limit = limit;
  char buf[40];
  snprintf(buf, sizeof(buf), "E%02u %s", code, why);
  raiseFault(buf);                    // halt + power cut + 5 s beep
}

void Dryer::applyMode(uint8_t m) {       // 0 agarbatti / 1 user / 2 silica
  bz::play(BP::MODE);                    // #4: mode rotated (owner spec)
  if (m > 2) m = 0;
  if (m == 0) { _cfg.setTemp = 60.0f; _cfg.dryMinutes = 120; }   // AGARBATTI
  else if (m == 2) { _cfg.setTemp = 80.0f; _cfg.dryMinutes = 120; } // SILICAGEL
  // m == 1 (USER DEFINED): keep the current values untouched
  _cfg.mode = m;
  saveSettings(_cfg);
  applySettings(_cfg);
  Serial.printf("[mode] %s (target %.0fC, %u min)%s\n",
                m == 0 ? "AGARBATTI DEFAULT" :
                m == 2 ? "SILICAGEL DEFAULT" : "USER DEFINED",
                (double)_cfg.setTemp, (unsigned)_cfg.dryMinutes,
                (_st == DState::RUNNING || _st == DState::COOLDOWN)
                  ? " - applies to the next cycle" : "");
}

void Dryer::raiseFault(const char *why) {
  bool cycleWasOn = (_st == DState::RUNNING || _st == DState::COOLDOWN);
  strncpy(_why, why, sizeof(_why) - 1);
  _why[sizeof(_why) - 1] = 0;
  if (cycleWasOn) cyclelog::finish("fault", why);
  _st = DState::FAULT;
  bz::play(BP::CRITICAL);               // #24: 5 s, then nag every 10 s
  bz::startRepeat(BP::CRITICAL_R, 10000);
  outputsAllOff();
  setRelay(false);                 // hard power cut
  Serial.printf("[dryer] FAULT: %s -> power cut\n", why);
}

uint32_t Dryer::remainingS() const {
  if (_st != DState::RUNNING) return 0;
  uint32_t total = _cfg.dryMinutes * 60UL;
  return (total > _elapsed) ? (total - _elapsed) : 0;
}

const LogRec &Dryer::logAt(uint16_t i) const {
  uint16_t idx = (_logHead + i) % LOG_MAX;
  return _log[idx];
}

// ---------------------------------------------------------------------
//  Control laws
// ---------------------------------------------------------------------
void Dryer::pidStep() {            // heater: hold setTemp via BTS7960 duty
  float t = sensors.tAvg();
  if (isnan(t)) { _heatDuty = 0; _boosting = false; pwmWritePin(PIN_BTS_RPWM, 0); digitalWrite(PIN_BTS_EN, LOW); return; }

  // MANUAL KNOB from the website: the user's exact duty for up to
  // MANUAL_HEAT_MS; the MCU returns to automatic (boost / PID) by itself.
  // Over-temperature and sensor-fault safety still apply (checked in tick
  // before we get here), so the knob cannot override a safety cut.
  if (_manUntil != 0 && millis() < _manUntil) {
    _boosting = false;
    _integ = 0;                          // bumpless hand-back to automatic
    _lastE = _cfg.setTemp - t;
    uint8_t cap = _cfg.heaterMax;
    _heatDuty = (_manPct < cap) ? _manPct : cap;
    digitalWrite(PIN_BTS_EN, _heatDuty > 0 ? HIGH : LOW);
    pwmWritePin(PIN_BTS_RPWM, dutyOf(_heatDuty));
    return;
  }
  _manUntil = 0;                         // window over -> automatic again

  // FULL-POWER HEAT-UP: below (setTemp - band) the BTS runs at max output
  // (heaterMax cap); once the chamber is close to target the PID takes
  // over and holds the temperature there.
  if (_cfg.boostHeat && t < _cfg.setTemp - _cfg.tempHyst) {
    _boosting = true;
    _integ = 0;                          // no wind-up from the boost phase
    _lastE = _cfg.setTemp - t;
    _heatDuty = _cfg.heaterMax;
    digitalWrite(PIN_BTS_EN, HIGH);
    pwmWritePin(PIN_BTS_RPWM, dutyOf(_heatDuty));
    return;
  }
  _boosting = false;

  float e = _cfg.setTemp - t;
  _integ += e * _cfg.ki;                          // 1 s loop
  if (_integ > 100.0f) _integ = 100.0f;
  if (_integ < -20.0f) _integ = -20.0f;           // anti-windup
  float d = e - _lastE;
  _lastE = e;

  float out = _cfg.kp * e + _integ + _cfg.kd * d;
  float cap = (float)_cfg.heaterMax;
  if (out < 0)   out = 0;
  if (out > cap) out = cap;
  _heatDuty = (uint8_t)(out + 0.5f);

  digitalWrite(PIN_BTS_EN, _heatDuty > 0 ? HIGH : LOW);
  pwmWritePin(PIN_BTS_RPWM, dutyOf(_heatDuty));
}

void Dryer::fanStep() {            // v2.0: burst venting, ONE outlet fan
  // Spec: RH above fanTrigRH (60 %) for fanTrigMin (1) minute -> the fan
  // runs at 100 % for fanBurstS (60) seconds, then re-arms. Purge = 100 %
  // continuous. fanMin is an optional continuous floor (default 0 = off).
  float h = sensors.hMax();       // control on the wettest sensor
  uint8_t d = 0;
  uint32_t now = millis();

  if (_st == DState::COOLDOWN) {
    d = 100;                      // purge heat + moist air before power cut
  } else if (_st == DState::RUNNING && !isnan(h)) {
    if (h >= _cfg.fanTrigRH) {
      if (_trigSince == 0) _trigSince = now ? now : 1;
      bz::play(BP::FAN_ON);                       // #15: fan kicks in
      if (_fanBurstUntil == 0 &&
          now - _trigSince >= (uint32_t)_cfg.fanTrigMin * 60000UL) {
        _fanBurstUntil = (now ? now : 1) + (uint32_t)_cfg.fanBurstS * 1000UL;
        _trigSince = 0;           // next burst needs a fresh trigger window
        Serial.printf("[fan] RH %.0f%% -> %u s burst at 100%%\n",
                      (double)h, (unsigned)_cfg.fanBurstS);
      }
    } else _trigSince = 0;
    if (_fanBurstUntil != 0) {
      if (now >= _fanBurstUntil) _fanBurstUntil = 0;   // burst finished
      else d = 100;
    }
    if (d == 0) d = _cfg.fanMin;  // optional continuous floor
  } else {
    _trigSince = 0; _fanBurstUntil = 0;
  }

  uint8_t dOut = (uint8_t)((uint16_t)d * _cfg.fanOut / 100);
  // kick-start: a resting fan rotor can stall at very low PWM - when the
  // channel wakes from 0, one 1 s tick at 60 % gets it spinning
  if (_fanOutD == 0 && dOut > 0 && dOut < 60) dOut = 60;
#if FAN_FIXED_DIR
  // enable-PWM: below ~40 % duty a 12 V fan just hums - clamp the floor
  if (dOut > 0 && dOut < FAN_PWM_FLOOR) dOut = FAN_PWM_FLOOR;
#endif

  _fanDuty = d;                   // demand (shown as fan %)
  _fanInD  = 0;                   // intake channel removed in v2.0
  _fanOutD = dOut;
  if (PIN_L298_ENA >= 0) pwmWritePin(PIN_L298_ENA, 0);
  pwmWritePin(PIN_L298_ENB, dutyOf(dOut));
}

// ---------------------------------------------------------------------
//  1-second tick
// ---------------------------------------------------------------------
void Dryer::tick() {
  uint32_t now = millis();
  if (_lastTick != 0 && now - _lastTick < CONTROL_PERIOD_MS) return;
  _lastTick = now ? now : 1;

  // -- battery housekeeping (safe shutdown; bypass only with relays) ---
  uint8_t pct = battery.percent();
  bool lowCut = battery.valid() && pct <= _cfg.cutoffPct;
#if RELAYS_ENABLED
  bool bypOn  = battery.valid() && pct <= _cfg.bypassPct && !lowCut;
  static bool prevByp = false;
  if (bypOn != prevByp) {
    Serial.printf("[batt] %s @ %.2fV (%u%%) - bypass threshold %u%%\n",
                  bypOn ? "BYPASS ON (battery low)" : "BYPASS OFF (battery recovered)",
                  battery.volts(), pct, _cfg.bypassPct);
    prevByp = bypOn;
  }
  battery.setBypass(bypOn);
#else
  battery.setBypass(false);           // no bypass relay fitted
#endif

  // -- safety: over-temperature ----------------------------------------
  float tMax = sensors.tMax();
  if (!isnan(tMax) && tMax >= _cfg.maxTemp && _st != DState::DONE) {
    faultNowE(1, "over-temperature - check airflow / coil size",
               sensors.tMax(), _cfg.maxTemp);
    fanStep();
    return;
  }

  // -- safety: both sensors dead ---------------------------------------
  if (sensors.anyOk()) {
    _sensFailSince = 0;
  } else {
    if (_sensFailSince == 0) _sensFailSince = now;
    if (_st == DState::RUNNING && now - _sensFailSince > SENSOR_FAIL_GRACE) {
      faultNowE(4, "all chamber sensors stopped responding");
      return;
    }
  }

  // -- v2.0.17 DEGRADED: one chamber sensor left -> warn + keep drying --
  {
    bool s1 = sensors.s1ok(), s2 = sensors.s2ok();
    if ((s1 != s2) && !_singleWarned) {
      _singleWarned = true;
      warnE(4, s1 ? "DHT22 return sensor lost - running on the AHT10 only"
                  : "AHT10 top sensor lost - running on the DHT22 only");
    } else if (s1 && s2 && _singleWarned) {
      _singleWarned = false;
      clearWarn(4);
    }
  }

  // -- safety: battery empty -------------------------------------------
  if (lowCut && _st != DState::DONE && _st != DState::FAULT) {
    faultNowE(12, "battery empty - safe shutdown",
               battery.volts(), 0.0f);
    return;
  }

  // -- E13/E14: supply-voltage guards (12.8 V LiFePO4 band) -------------
  if (battery.valid()) {
    float vb = battery.volts();
    if (vb < 11.5f) {
      if (_voltLowSince == 0) _voltLowSince = now;
      else if (now - _voltLowSince > 60000UL)
        warnE(13, "supply below 11.5 V - check panel / charge", vb, 11.5f);
    } else _voltLowSince = 0;
    if (vb > 15.0f) {
      if (_voltHighSince == 0) _voltHighSince = now;
      else if (now - _voltHighSince > 30000UL) {
        faultNowE(14, "supply above 15 V - check the MPPT setting", vb, 15.0f);
        return;
      }
    } else _voltHighSince = 0;
  }

  switch (_st) {
    case DState::IDLE:
    case DState::FAULT:
      outputsAllOff();
      break;

    case DState::DONE:
      outputsAllOff();
      if (_doneAt && now - _doneAt > 60000UL) {   // #35: safe to open
        bz::play(BP::COOL_DONE);
        _doneAt = 0;
      }
      break;

    case DState::RUNNING: {
      _elapsed = (now - _tStart) / 1000UL;

      // -- E15 DEGRADED (v2.0.17): scale lost -> CONTINUE on time + RH ----
      if (scale.ok()) {
        if (_scaleLost) {
          _scaleLost = false;
          clearWarn(15);
          Serial.println(F("[dryer] scale RECOVERED - weight tracking resumes"));
        }
        _scaleWasOk = true;  _scaleLostSince = 0;
      } else if (_scaleWasOk) {
        if (_scaleLostSince == 0) _scaleLostSince = now;
        else if (now - _scaleLostSince > 10000UL && !_scaleLost) {
          _scaleLost = true;
          warnE(15, "HX711 lost - DEGRADED: continuing on time + RH stop");
        }
      }

      // -- E20: far above setpoint but under the hard cut (PID runaway) --
      float tA20 = sensors.tAvg();
      if (!isnan(tA20) && tA20 > _cfg.setTemp + 15.0f) {
        if (_tHighWarnSince == 0) _tHighWarnSince = now;
        else if (now - _tHighWarnSince > 60000UL)
          warnE(20, "chamber far above setpoint", tA20, _cfg.setTemp + 15.0f);
      } else _tHighWarnSince = 0;

      // -- #13/#19 battery critical + 5-min-to-timeout nags ---------------
      if (battery.valid() && battery.percent() < 10)
        bz::startRepeat(BP::BATT_CRIT, 30000);
      else bz::stopRepeat(BP::BATT_CRIT);
      if (remainingS() <= 300) bz::startRepeat(BP::TIMEOUT5, 30000);
      else                     bz::stopRepeat(BP::TIMEOUT5);

      // -- #14/#17/#18/#22 progress cues (owner spec) ---------------------
      float gNow = (scale.ok() && !isnan(scale.grams())) ? scale.grams() : NAN;
      if (!isnan(gNow) && !isnan(_wtStart)) {
        if (!_spReached && !isnan(sensors.tAvg()) &&
            sensors.tAvg() >= _cfg.setTemp - 1.0f) {
          _spReached = true;
          bz::play(BP::SETPOINT);                  // warm-up over
        }
        if (_cfg.targetG > 0) {
          float span = _wtStart - _cfg.targetG;
          if (span > 1.0f) {
            if (!_midway && (_wtStart - gNow) >= span / 2) {
              _midway = true;
              bz::play(BP::MIDWAY);                // 50 % moisture removed
            }
            if (gNow > _cfg.targetG && gNow <= _cfg.targetG * 1.05f)
              bz::startRepeat(BP::APPROACH, 60000);// target approaching
            else bz::stopRepeat(BP::APPROACH);
          }
        }
        if (!_anomWarned && !isnan(_lastG) && _elapsed > 60 &&
            fabsf(gNow - _lastG) > 150.0f) {       // tray shifted / fell
          _anomWarned = true;
          bz::play(BP::ANOMALY);
          Serial.printf("[warn] weight jumped %.0f -> %.0f g mid-cycle\n",
                        (double)_lastG, (double)gNow);
        }
        if (!isnan(gNow)) _lastG = gNow;
      }

      pidStep();
      fanStep();

      // ---- v2.0 watchdog: HEATER FAILURE (constant temp 3 min) ---------
      // While heating up (still 2 deg below target, duty >= 80 %), the
      // chamber temperature must creep up. Flat for 3 minutes = the coil
      // / fuse / PSU / EN path is dead -> fault (spec).
      {
        float tA = sensors.tAvg();
        if (!isnan(tA) && tA < _cfg.setTemp - 2.0f && _heatDuty >= 80) {
          if (_flatSince == 0) { _flatSince = now; _flatT0 = tA; }
          else if (now - _flatSince >= 180000UL && tA - _flatT0 < 0.5f) {
            faultNowE(3, "no temperature rise for 3 min while heating",
                     tA, _cfg.setTemp - 2.0f);
            return;
          }
        } else _flatSince = 0;
      }

      // ---- v2.0 watchdog: FAN ERROR (RH above humHigh for 5 min) -------
      // Bursts are firing but the humidity will not come down: fan dead,
      // blocked duct or the sensor is wet -> fault (spec).
      {
        float hM = sensors.hMax();
        if (!isnan(hM) && hM >= _cfg.humHigh) {
          if (_rhErrSince == 0) _rhErrSince = now ? now : 1;
          else if (now - _rhErrSince >= 300000UL) {
            faultNowE(6, "RH above 60% for 5 min despite the fan",
                     sensors.hMax(), 60.0f);
            return;
          }
        } else _rhErrSince = 0;
      }

      // ---- v2.0 target weight: warn 5 min before time-up + suggest ----
      if (_cfg.targetG > 0 && scale.ok() && !isnan(_wtStart) &&
          !isnan(scale.grams()) && !_tWarned) {
        float cur = scale.grams();
        uint32_t remainS = (uint32_t)_cfg.dryMinutes * 60UL - _elapsed;
        if (cur > _cfg.targetG * 1.05f && remainS <= 300UL) {
          _tWarned = true;
          float rate = scale.rate();          // g/min, positive = losing
          _suggestMin = (rate > 0.1f) ? (cur - _cfg.targetG) / rate : -1.0f;
          if (_suggestMin > 0.0f)
            Serial.printf("[warn] target weight not reachable in the last "
                          "%u min - at this rate it needs about +%.0f min\n",
                          (unsigned)(remainS / 60UL), (double)_suggestMin);
          else
            Serial.println(F("[warn] target weight not reachable and the "
                             "rate is too low to estimate"));
          buzzer.beep(2, 500, 300);           // distinct warning pattern
        }
      }

      bool timeUp   = _elapsed >= _cfg.dryMinutes * 60UL;
      bool humOk    = !_cfg.requireHum ||
                      (!isnan(sensors.hMax()) && sensors.hMax() <= _cfg.humTarget);

      // dry-to-weight: the batch itself says when it is dry. The rate
      // must stay under weightRateG (in g/min) for weightMinY minutes.
      // Scale absent + gate on -> warn once and fall back to time+RH.
      bool wtOk = true;
      static bool warnedNoScale = false;
      if (_cfg.requireWeight) {
        if (scale.ok() && !isnan(scale.rate())) {
          warnedNoScale = false;
          if (fabsf(scale.rate()) < _cfg.weightRateG) {
            if (_wtGoodSince == 0) _wtGoodSince = now;
          } else _wtGoodSince = 0;
          wtOk = (_wtGoodSince != 0 &&
                  now - _wtGoodSince >= (uint32_t)_cfg.weightMinY * 60UL);
        } else {
          if (!warnedNoScale) {
            Serial.println("[warn] weight gate requested but scale absent - falling back to time+RH");
            warnedNoScale = true;
          }
          _wtGoodSince = 0;
        }
      } else _wtGoodSince = 0;

      if (timeUp && humOk && wtOk) {
        // v2.0: within 5 % of the target weight (when set) = complete,
        // otherwise DONE-WITH-WARNING (the cycle still ends on time)
        bool tgtOk = true;
        if (_cfg.targetG > 0 && scale.ok() && !isnan(scale.grams()))
          tgtOk = scale.grams() <= _cfg.targetG * 1.05f;
        if (tgtOk) {
          strncpy(_endReason, "completed", sizeof(_endReason));
        } else {
          strncpy(_endReason, "E19 done - target weight NOT reached",
                  sizeof(_endReason));
          Serial.println(F("[warn] cycle ended: target weight not reached "
                           "within 5 % - re-run if needed"));
          buzzer.beep(2, 500, 300);
        }
        _st = DState::COOLDOWN;
        _cdStart = now;
        Serial.println("[dryer] cycle complete -> purge, then power cut");
      }

      // data log every 10 s
      if (_lastLog == 0 || now - _lastLog >= LOG_PERIOD_MS) {
        _lastLog = now;
        LogRec r{};
        r.t = _elapsed;
        r.tAvg10 = isnan(sensors.tAvg()) ? 0 : (int16_t)(sensors.tAvg() * 10);
        r.hAvg10 = isnan(sensors.hAvg()) ? 0 : (int16_t)(sensors.hAvg() * 10);
        r.hMax10 = isnan(sensors.hMax()) ? 0 : (int16_t)(sensors.hMax() * 10);
        r.heat = (int16_t)_heatDuty;
        r.fan  = (int16_t)_fanDuty;
        r.vb10 = (int16_t)(battery.volts() * 10);
        r.bat  = (int16_t)pct;
        r.wt10 = scale.ok() && !isnan(scale.grams())
                   ? (int16_t)constrain(scale.grams() / 10.0f, -3200.0f, 3200.0f)
                   : INT16_MIN;      // marker: no scale this cycle
        r.ot10 = (dht::outdoor.ok && !isnan(dht::outdoor.t))
                   ? (int16_t)constrain(dht::outdoor.t * 10.0f, -300.0f, 300.0f)
                   : INT16_MIN;      // marker: no outdoor sensor
        r.oh10 = (dht::outdoor.ok && !isnan(dht::outdoor.h))
                   ? (int16_t)constrain(dht::outdoor.h * 10.0f, 0.0f, 100.0f)
                   : INT16_MIN;
        if (_logN < LOG_MAX) { _log[_logN] = r; _logN++; _logHead = 0; }
        else { _log[_logHead] = r; _logHead = (_logHead + 1) % LOG_MAX; }
      }
      break;
    }

    case DState::COOLDOWN: {
      _heatDuty = 0;
      pwmWritePin(PIN_BTS_RPWM, 0);
      digitalWrite(PIN_BTS_EN, LOW);
      fanStep();                    // 100 % purge
      if (now - _cdStart >= (uint32_t)_cfg.cooldownSec * 1000UL) {
        outputsAllOff();
        setRelay(false);            // relay (if fitted) cuts the power
        cyclelog::finish(_endReason,
                    _scaleLost ? "E15 scale lost - degraded time/RH stop" : "");
        _finalG    = (scale.ok() && !isnan(scale.grams())) ? scale.grams() : NAN;
        _endElapsed = _elapsed;                 // completion summary (v2.0.15)
        _doneAt    = now;
        bz::cycleDone();             // 5 s continuous (spec)
        _st = DState::DONE;
#if RELAYS_ENABLED
        Serial.println("[dryer] DONE - power cut by relay");
#else
        Serial.println("[dryer] DONE - heater and fans off");
#endif
      }
      break;
    }
  }
}
