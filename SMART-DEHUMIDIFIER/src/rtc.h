/**
 * @file rtc.h
 * @brief DS1302 real-time clock (date & time) - 3-wire bit-banged,
 *        auto-detected, library-free (v2.0.21).
 *
 * The DS1302 keeps the wall clock on its own CR2032 coin cell, so the
 * date & time survive a FULL power-down (even a battery disconnect).
 * Without the chip the firmware falls back to the old phone-sync + NVS
 * clock - nothing else changes.
 *
 * Wiring (DS1302 module -> S3):  VCC -> 3V3, GND -> GND,
 *   SCLK -> PIN_RTC_SCLK,  I/O -> PIN_RTC_IO,  RST -> PIN_RTC_RST
 *   (the module's BZ buzzer pin is unused; its battery stays ON the
 *    module and holds the time when the pillar is off).
 *
 * Behaviour:
 *  - begin(): detects the chip (two identical reads). Write-protect is
 *    always cleared (its power-on state is undefined). A factory-fresh
 *    module ships with the CH (halt) flag set + garbage registers, so
 *    such a time is untrusted until the first real set.
 *  - readTime(): 24 h LOCAL wall clock from the chip (converted to an
 *    absolute epoch using the configured timezone - no TZ env needed).
 *  - writeNow(): system clock -> chip (local time, tzMinutes applied).
 *  - All calls are no-ops (present() == false) when no chip is found
 *    or the RTC pins are -1 (classic variant).
 */
#pragma once
#include <Arduino.h>

namespace rtc {
bool  begin();                  // detect + clear write-protect; true = present
bool  present();                // chip found (does not mean time is good)
bool  readTime(time_t *outEp);  // true = chip running with a sane time
void  writeNow();               // system clock -> chip (no-op if absent)
const char *statusText();       // short one-liner for serial/console
}
