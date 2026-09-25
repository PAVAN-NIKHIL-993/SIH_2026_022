/**
 * @file supply.h
 * @brief Power latch + supply selector (v2.0): solar <-> bypass with an
 *        optocoupler live-check, the 2-channel feed relay, the master
 *        power button (hard off / reboot) and the default-automation
 *        button.
 *
 * HARD POWER-OFF CIRCUIT (manual 02 sect.4.11): BUTTON-1 connects the
 * P-MOSFET latch gate to GND while pressed -> the MCU gets power ->
 * this module asserts PIN_POWER_HOLD within milliseconds of boot.
 *   BUTTON-1 held 3 s  -> firmware stops the loads, drops the hold ->
 *                         the WHOLE pillar loses power.
 *   BUTTON-1 held 10 s -> reboot instead (ESP.restart()).
 *
 * SUPPLY MODE: a physical toggle requests SOLAR (or BYPASS). The MCU
 * drives the 2-channel relay (CH1 = solar feed, CH2 = bypass feed) but
 * ONLY when the loads are quiet - a change requested while a supply is
 * under load is refused with a 5 s error beep. The optocoupler input
 * verifies the selected feed is actually live; a mismatch is an error.
 * At battery cutoff, if the bypass feed is live, the MCU switches to
 * bypass automatically (and beeps).
 */
#pragma once
#include <Arduino.h>
#include "config.h"

namespace supply {
  void begin();              // latch hold UP FIRST, pins, relay LOW (boot)
  void engage();             // after the init window: relay follows the toggle
  void update();             // buttons, toggle, opto check, relay switching
  bool solarRequested();     // toggle position: true = solar mode
  const char *modeName();    // "SOLAR MODE" / "BYPASS MODE"
  bool optoLive();           // optocoupler: selected feed is live
  bool relayAuto();          // the MCU can switch the feed relay (S3)
  bool latched();            // soft-latch hardware present
  void powerOff();           // safe-stop then drop the latch (hard off)
  void requestSwitch();      // follow the toggle now if safe (serial too)
}