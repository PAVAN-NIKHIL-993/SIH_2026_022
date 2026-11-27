/**
 * @file rtc.h
 * @brief DS1307 real-time clock (date & time) on the shared I2C bus -
 *        auto-detected, library-free (v2.0.23; replaced the v2.0.21 DS1302).
 *
 * The DS1307 keeps the wall clock on its own coin cell, so the date & time
 * survive a FULL power-down (even a battery disconnect). Without the chip
 * the firmware falls back to the old phone-sync + NVS clock - nothing else
 * changes.
 *
 * Wiring (DS1307 module -> ESP32), on the I2C0 bus the AHT10 #1, the PCF8574
 * keypad and the EEPROM already share - no extra GPIO:
 *   VCC -> 5V      the DS1307 needs 4.5-5.5 V; at 3.3 V it ignores the bus
 *   GND -> GND
 *   SDA -> PIN_I2C0_SDA   (S3: GPIO 8 · classic: GPIO 21)
 *   SCL -> PIN_I2C0_SCL   (S3: GPIO 9 · classic: GPIO 22)
 *   SQW, DS, BAT pads: unused.
 *   !! Most DS1307 boards (the "Tiny RTC" among them) pull SDA/SCL up to
 *      5 V through R2/R3. ESP32 pins are NOT 5 V tolerant: remove R2 + R3
 *      (the bus already has 3.3 V pull-ups on the AHT10/PCF boards) or put
 *      a 3.3 V <-> 5 V I2C level shifter in between. The DS1307 reads 3.3 V
 *      logic fine (V_IH = 2.2 V); its pull-ups may go to any voltage <= 5.5 V.
 *   Address 0x68 (fixed). The bus runs at the Wire default of 100 kHz -
 *   exactly the DS1307's maximum (it has no 400 kHz mode).
 *
 * Behaviour:
 *  - begin(): detects the chip (ACK at RTC_I2C_ADDR). A factory-fresh module
 *    - or one whose coin cell died - has the CH (clock-halt) flag set and/or
 *    a junk date, so its time is untrusted until the first real set.
 *  - readTime(): 24 h LOCAL wall clock from the chip, converted to an
 *    absolute epoch with the configured timezone (no TZ env needed).
 *    false while the oscillator is halted or the registers are not a time.
 *  - writeNow(): system clock -> chip (local time, tzMinutes applied,
 *    24 h mode, CH cleared = oscillator running).
 *  - All calls are no-ops (present() == false) when no chip answers, and
 *    compile to stubs with RTC_ENABLED 0.
 *  - DS3231 boards (3.3 V-native, far more accurate) use the same time
 *    registers and work with this driver unchanged.
 */
#pragma once
#include <Arduino.h>

namespace rtc {
bool  begin();                  // detect (Wire must be started); true = present
bool  present();                // chip found (does not mean time is good)
bool  readTime(time_t *outEp);  // true = chip running with a sane time
void  writeNow();               // system clock -> chip (no-op if absent)
const char *statusText();       // short one-liner for serial/console
}
