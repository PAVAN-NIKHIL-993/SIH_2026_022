#include "supply.h"
#include "control.h"
#include "buzzer.h"

namespace supply {

// ---- state -------------------------------------------------------------
static bool  sHoldUp    = false;      // our latch keep-alive asserted
static bool  sSolar     = true;       // requested mode (toggle)
static bool  sRelayAuto = SUPPLY_RELAYS_ENABLED != 0;
static bool  sBtn1Was   = false;      // BUTTON-1 edge tracking
static uint32_t sBtn1T  = 0;          // press start
static bool  sBtn2Was   = false;
static bool  sErrLatched = false;     // one error beep per event

bool latched()    { return PIN_POWER_HOLD >= 0; }
bool relayAuto()  { return sRelayAuto; }
bool solarRequested() { return sSolar; }

bool optoLive() {
  if (PIN_SUPPLY_OPTO < 0) return true;         // no detector: assume ok
  return digitalRead(PIN_SUPPLY_OPTO) == (OPTO_ACTIVE_HIGH ? HIGH : LOW);
}

const char *modeName() { return sSolar ? "SOLAR MODE" : "BYPASS MODE"; }

// ---- 2-channel feed relay ----------------------------------------------
static void driveRelay(bool solar) {
#if SUPPLY_RELAYS_ENABLED
  // one channel ON at a time, the other always OFF (never both feeds)
  digitalWrite(PIN_SUPPLY_CH1, solar ? HIGH : LOW);
  digitalWrite(PIN_SUPPLY_CH2, solar ? LOW  : HIGH);
#else
  (void)solar;
#endif
}

static void driveAllOff() {                 // both feed channels LOW
#if SUPPLY_RELAYS_ENABLED
  digitalWrite(PIN_SUPPLY_CH1, LOW);
  digitalWrite(PIN_SUPPLY_CH2, LOW);
#endif
}

void engage() { driveRelay(sSolar); }        // after the 10 s init window

bool loadsQuiet() {
  DState st = dryer.state();
  return st != DState::RUNNING && st != DState::COOLDOWN &&
         dryer.heatDuty() == 0 && dryer.fanOutDuty() == 0;
}

// ---- hard power-off ------------------------------------------------------
void powerOff() {
  Serial.println(F("[power] BUTTON-1 3 s -> HARD POWER OFF"));
  dryer.stop();                                  // loads off, purge skipped:
  bz::error();                                   // the pillar is dying anyway
  delay(300);                                    // let the beep + serial flush
  if (PIN_POWER_HOLD >= 0) digitalWrite(PIN_POWER_HOLD, LOW);
  sHoldUp = false;
  delay(2000);                                   // latch falls, MCU starves
  // still alive? the latch failed - say so and carry on
  Serial.println(F("[power] latch did not release - check the P-MOSFET circuit"));
  if (PIN_POWER_HOLD >= 0) digitalWrite(PIN_POWER_HOLD, HIGH);
}

// ---- mode switch ----------------------------------------------------------
void requestSwitch() {                          // follow the toggle if safe
  bool wantSolar = solarRequested();
  if (wantSolar == sSolar) return;
  if (!loadsQuiet()) {
    if (!sErrLatched) {
      Serial.println(F("[supply] REFUSED: mode change while a supply is "
                       "under load - stop the cycle first"));
      dryer.warnE(2, "change-over refused under load");      // E02 (spec)
      bz::error();                              // 5 s error beep (spec)
      sErrLatched = true;
    }
    return;                                     // toggle stays pending
  }
  sSolar = wantSolar;
  sErrLatched = false;
  driveRelay(sSolar);
  Serial.printf("[supply] switched to %s\n", modeName());
  bz::modeChange();                             // 3 s continuous (spec)
}

// ---- begin -----------------------------------------------------------------
void begin() {
  // 1. keep ourselves alive - the soft-latch button may already be released
  if (PIN_POWER_HOLD >= 0) {
    pinMode(PIN_POWER_HOLD, OUTPUT);
    digitalWrite(PIN_POWER_HOLD, HIGH);         // assert within ms of boot
    sHoldUp = true;
  }
  // 2. inputs
  if (PIN_BTN1 >= 0) pinMode(PIN_BTN1, INPUT_PULLUP);
  if (PIN_BTN2 >= 0) pinMode(PIN_BTN2, INPUT_PULLUP);
  if (PIN_SOLAR_TOGGLE >= 0) pinMode(PIN_SOLAR_TOGGLE, INPUT_PULLUP);
  if (PIN_SUPPLY_OPTO >= 0) pinMode(PIN_SUPPLY_OPTO, INPUT);
  // 3. relay follows the toggle immediately (loads are off at boot)
  if (PIN_SOLAR_TOGGLE >= 0) sSolar = digitalRead(PIN_SOLAR_TOGGLE) == HIGH;
#if SUPPLY_RELAYS_ENABLED
  pinMode(PIN_SUPPLY_CH1, OUTPUT);
  pinMode(PIN_SUPPLY_CH2, OUTPUT);
#endif
  driveAllOff();                 // spec: every pin LOW for the init window
  Serial.printf("[supply] %s (toggle), opto %s, relay %s\n",
                modeName(),
                PIN_SUPPLY_OPTO >= 0 ? "fitted" : "absent",
                sRelayAuto ? "auto" : "manual");
}

// ---- update ------------------------------------------------------------------
void update() {
  uint32_t now = millis();

  // BUTTON-1: 3 s = hard power off, 10 s = reboot
  if (PIN_BTN1 >= 0) {
    static uint32_t sLastTick = 0;
    bool p = digitalRead(PIN_BTN1) == LOW;      // active low (to GND)
    if (p && !sBtn1Was) { sBtn1T = now; sLastTick = 0; }
    if (p && sBtn1Was) {
      // #3: live tick every 500 ms so the operator holds long enough
      if (now - sBtn1T > 500 && now - sLastTick >= 500) {
        bz::play(BP::TICK);
        sLastTick = now;
      }
      if (now - sBtn1T >= BTN1_RESET_MS) {      // 10 s: reboot
        Serial.println(F("[power] BUTTON-1 10 s -> REBOOT"));
        bz::play(BP::FACT_RESET);               // #37: sweep + 3 beeps
        delay(2100);                            // let it sound before dying
        ESP.restart();
      } else if (now - sBtn1T >= BTN1_OFF_MS) { // 3 s: hard off
        bz::play(BP::SHUTDOWN);                 // #36: descending confirm
        delay(1200);
        powerOff();                             // may not return
        sBtn1T = now;                           // re-arm if latch failed
      }
    }
    sBtn1Was = p;
  }

  // BUTTON-2: default automation - agarbatti preset + start (door-gated)
  if (PIN_BTN2 >= 0) {
    bool p = digitalRead(PIN_BTN2) == LOW;
    if (p && !sBtn2Was) {
      Serial.println(F("[btn2] default automation: AGARBATTI preset + start"));
      dryer.applyMode(0);
      dryer.start();
    }
    sBtn2Was = p;
  }

  // toggle / opto / relay
  if (PIN_SOLAR_TOGGLE >= 0) {
    bool wantSolar = digitalRead(PIN_SOLAR_TOGGLE) == HIGH;
    if (wantSolar != sSolar) requestSwitch();
  }

  // opto verification: selected feed should be live
  static uint32_t optoWarnT = 0;
  if (PIN_SUPPLY_OPTO >= 0 && !optoLive() && optoWarnT != 0 &&
      now - optoWarnT > 60000UL) {
    Serial.printf("[warn] %s but the feed reads DEAD - check the supply\n",
                  modeName());
    dryer.warnE(11, "selected feed reads DEAD");            // E11 (spec)
    bz::error();
    optoWarnT = now ? now : 1;
  } else if (PIN_SUPPLY_OPTO >= 0 && optoLive()) {
    optoWarnT = now ? now : 1;                  // feed healthy: re-arm
  } else if (PIN_SUPPLY_OPTO >= 0 && optoWarnT == 0) {
    optoWarnT = now ? now : 1;
  }
}

}  // namespace supply