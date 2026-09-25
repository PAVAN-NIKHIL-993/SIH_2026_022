/**
 * @file door.h
 * @brief Door limit switch + the CALIBRATE -> LOAD -> READY workflow.
 *
 * This build: LIMIT SWITCH ONLY (open/closed status) - no solenoid lock.
 * Software gates: START refused until the scale is calibrated, batch
 * weighed on door close (READY), opening mid-cycle = instant FAULT.
 * A 12 V lock can be fitted later (DOOR_LOCK_ENABLED 1).
 *
 * The weigh scale is the master of the workflow:
 *   1. NOT calibrated  -> START LOCKED. Start is refused everywhere
 *      (website, keypad, BOOT button, serial).
 *   2. Calibrated      -> gate opens. "Load the trays."
 *   3. Door closed with the scale stable -> batch weight = the diff
 *      vs the tare captured at calibration. "READY: 1234 g - Start".
 *   4. RUNNING         -> opening the door mid-cycle is a FAULT
 *      (heat + fans stop, purge, power off).
 *   5. DONE/FAULT      -> ready for unloading.
 *
 * Classic ESP32 has no free pins for the switch -> DOOR_ENABLED 0: the same
 * workflow runs in software (start still refused until calibration).
 */
#pragma once
#include <Arduino.h>
#include "config.h"

namespace door {
  void begin(bool engageLock = true);  // pins + calibrated flag (NVS);
                                        // engageLock=false: outputs LOW (boot)
  void engage();                // after the 10 s init window: lock per state
  void update();                // call from loop: reed edges, lock, workflow
  bool fitted();                // lock/reed hardware present
  bool locked();                // lock output engaged
  bool closed();                // reed says closed (always true if unfitted)
  bool calibrated();            // scale calibrated at least once
  void markCalibrated();        // called after a successful known-weight cal
  void serviceUnlock();         // service override: unlock (serial only)
  float batchG();               // measured batch weight, 0 until measured
  const char *phase();          // CALIBRATE / LOAD / READY / RUNNING
}