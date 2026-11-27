#include "door.h"
#include "control.h"
#include "scale.h"
#include "buzzer.h"
#include <Preferences.h>

// Phases: CALIBRATE (locked) -> LOAD (unlocked) -> READY (batch weighed)
//         -> RUNNING (locked) -> back to LOAD after DONE/FAULT.
enum class DPhase { CALIBRATE, LOAD, READY, RUNNING };
static DPhase sPhase = DPhase::CALIBRATE;
static bool  sCalOK  = false;          // scale calibrated at least once
static bool  sLocked = false;          // lock output state
static float sBatch  = 0.0f;           // measured batch grams
static bool  sWasClosed = true;
static uint32_t sStableSince = 0;      // scale-stable window for weighing
static float   sStableG = 0.0f;
static bool  sUnlockLogged = false;

namespace door {

bool fitted()     { return DOOR_ENABLED != 0; }
bool locked()     { return sLocked; }
bool calibrated() { return sCalOK; }
float batchG()    { return sBatch; }

bool closed() {
#if DOOR_ENABLED
  return digitalRead(PIN_DOOR_REED) == DOOR_CLOSED_LEVEL;
#else
  return true;                         // no sensor: assume closed
#endif
}

const char *phase() {
  switch (sPhase) {
    case DPhase::CALIBRATE: return "CALIBRATE";
    case DPhase::LOAD:      return "LOAD";
    case DPhase::READY:     return "READY";
    case DPhase::RUNNING:   return "RUNNING";
  }
  return "?";
}

static void driveLock(bool on) {
  if (on == sLocked) return;
  sLocked = on;
#if DOOR_ENABLED && DOOR_LOCK_ENABLED
  digitalWrite(PIN_DOOR_LOCK, on ? DOOR_LOCK_ACTIVE : !DOOR_LOCK_ACTIVE);
#endif
  // without a physical lock this is the SOFTWARE gate: start refused
  Serial.printf("[door] %s\n", on ? "START LOCKED" : "start unlocked");
}

void engage() { driveLock(!sCalOK); }   // called after the init window

void markCalibrated() {
  if (sCalOK) return;
  sCalOK = true;
  Preferences p;
  p.begin("dryer", false);
  p.putBool("scalOK", true);
  p.end();
  Serial.println(F("[door] scale calibrated - workflow unlocked"));
}

void serviceUnlock() {                 // serial service override
  sCalOK = true;
  driveLock(false);
  Serial.println(F("[door] SERVICE unlock"));
}

void begin(bool engageLock) {
#if DOOR_ENABLED
#if DOOR_LOCK_ENABLED
  pinMode(PIN_DOOR_LOCK, OUTPUT);
#endif
  pinMode(PIN_DOOR_REED, INPUT_PULLUP);
#endif
  Preferences p;
  p.begin("dryer", true);
  sCalOK = p.getBool("scalOK", false);
  p.end();
  sWasClosed = closed();
  if (sCalOK) sPhase = DPhase::LOAD;   // calibrated on an earlier boot
  // spec v2.0: during the 10 s power-on window every pin stays LOW -
  // the lock engages only when the window ends (engage())
  driveLock(engageLock && !sCalOK);
  Serial.printf("[door] %s%s - %s\n",
                fitted() ? "lock+reed fitted, " : "software workflow (no lock pins), ",
                sCalOK ? "scale calibrated" : "scale NOT calibrated",
                sCalOK ? "load the trays" : "door LOCKED until calibration");
}

void update() {
  DState st = dryer.state();

  // ---- phase machine --------------------------------------------------
  if (st == DState::RUNNING) {
    if (sPhase != DPhase::RUNNING) {
      sPhase = DPhase::RUNNING;
      driveLock(true);                 // keep it shut while hot
    }
  } else if (st == DState::COOLDOWN) {
    driveLock(true);                   // still hot - stay locked
  } else {                             // IDLE / DONE / FAULT
    if (sPhase == DPhase::RUNNING) {   // cycle just ended
      sPhase = DPhase::LOAD;
      sBatch = 0.0f;
      driveLock(sCalOK ? false : true);
      if (sCalOK) Serial.println(F("[door] unlocked - unload / load the next batch"));
    }
    if (!sCalOK) {                     // the whole point: no cal, no door
      sPhase = DPhase::CALIBRATE;
      driveLock(true);
      return;
    }
    if (sPhase == DPhase::CALIBRATE) {
      sPhase = DPhase::LOAD;
      driveLock(false);
      if (!sUnlockLogged) {
        Serial.println(F("[door] unlocked - load the trays, then close the door"));
        sUnlockLogged = true;
      }
    }
  }

  // ---- door closed + stable scale = batch weight (the "diff") ---------
  bool nowClosed = closed();
  if (nowClosed && !sWasClosed) {         // a close ends these nags
    bz::stopRepeat(BP::DOOR_AJAR);
    bz::stopRepeat(BP::DOOR_OPEN_RUN);
    bz::stopRepeat(BP::UNSTABLE);
  }
  if (nowClosed && !sWasClosed && sPhase == DPhase::LOAD &&
      scale.ok() && !isnan(scale.grams())) {
    if (sStableSince == 0) { sStableSince = millis(); sStableG = scale.grams(); }
    // weigh when the reading sits within 20 g for ~4 s
    if (millis() - sStableSince >= 4000) {
      if (fabsf(scale.grams() - sStableG) <= 20.0f) {
        sBatch = scale.grams();        // tare was captured at calibration
        sPhase = DPhase::READY;
        sStableSince = 0;
        bz::play(BP::READY);            // #11: weighed - ready to start
        if (sBatch < 50.0f)
          Serial.printf("[load] door closed - trays look EMPTY (%.0f g)\n", (double)sBatch);
        else if (sBatch > 9000.0f)
          Serial.printf("[load] door closed - OVERLOAD %.0f g (cells are 10 kg) - remove some\n", (double)sBatch);
        else
          Serial.printf("[load] door closed - batch %.0f g loaded - press Start (or knob/serial)\n", (double)sBatch);
      } else {
        sStableSince = millis(); sStableG = scale.grams();  // still settling
      }
    }
  } else if (nowClosed && sPhase == DPhase::LOAD && sStableSince != 0 &&
             millis() - sStableSince > 6000UL) {
    // #9: weight still moving ~6 s after the close - keep nudging
    bz::startRepeat(BP::UNSTABLE, 3000);
    sStableSince = millis();            // re-arm the window
  } else if (!nowClosed && sPhase == DPhase::READY) {
    sPhase = DPhase::LOAD;             // reopened: allow the next weighing
    sStableSince = 0;
  } else if (nowClosed != sWasClosed) {
    sStableSince = 0;
  }
  if (nowClosed != sWasClosed) bz::door();     // 1 s beep: door moved (spec)
  sWasClosed = nowClosed;

  // ---- door opened mid-cycle = FAULT ----------------------------------
#if DOOR_ENABLED
  if ((st == DState::RUNNING || st == DState::COOLDOWN) && !nowClosed) {
    dryer.faultNowE(7, "door opened during the cycle");   // E07 (spec)
    bz::startRepeat(BP::DOOR_OPEN_RUN, 150);  // #16: rapid until closed
  }
#endif
}

}  // namespace door
