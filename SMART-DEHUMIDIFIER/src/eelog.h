/**
 * @file eelog.h
 * @brief AT24C256 (32 kB I2C EEPROM) long-term cycle registry - v2.0.17.
 *
 * LittleFS keeps the detailed per-cycle CSVs (last 40); this chip keeps a
 * SUMMARY of EVERY cycle (~817 slots, years of batches) that outlives any
 * filesystem reformat. Wiring: VCC->3V3, GND->GND, SDA/SCL on I2C0
 * (GPIO 8/9), A0/A1/A2->GND = address 0x50. Auto-detected at boot - no
 * chip, no problem (everything else works without it).
 *
 * Layout: page 0 = header (magic, version, count, head, seq); records of
 * 40 bytes start at 64. Ring buffer: when full, the oldest summary is
 * overwritten. One append = one record page-write + header rewrite
 * (~10 ms, once per cycle - wear is a non-issue at 1 M cycles/cell).
 * Each record carries a CRC8; a torn write (power loss mid-append) is
 * rolled back at the next boot.
 */
#pragma once
#include <Arduino.h>
#include "config.h"

namespace eelog {

// one cycle summary, 40 bytes on the chip (packed by hand in eelog.cpp)
struct EeRec {
  uint32_t seq;          // 1, 2, 3, ... (0 = never written)
  uint32_t startEpoch;   // unix time the cycle started
  uint32_t durS;         // duration in seconds
  uint8_t  mode;         // 0 agarbatti / 1 user / 2 silica
  uint8_t  endR;         // 0 done / 1 stopped / 2 fault / 3 timeout / 4 interrupted
  uint8_t  ecode;        // E-code (0 = none)
  uint8_t  flags;        // bit0 scaleLost · bit1 interrupted · bit2 targetReached
  int16_t  setT10;       // setpoint  x10 C
  int16_t  tAvg10;       // avg temp  x10 C
  int16_t  tMax10;       // max temp  x10 C
  int16_t  hMax10;       // max RH    x10 %
  int16_t  outT10;       // avg outdoor temp x10 C
  int16_t  wtS10;        // start weight  x10 g
  int16_t  wtE10;        // end weight    x10 g
  int16_t  wtT10;        // target weight x10 g
  int16_t  vbS10;        // battery at start x10 V
  int16_t  vbE10;        // battery at end   x10 V
  uint8_t  crc;          // CRC8 over bytes 0..37
  uint8_t  spare;
};

#if ELOG_ENABLED

void begin();                 // detect the chip, validate/repair the header
bool ok();                    // chip present and healthy
void append(EeRec &r);        // add one summary (r.seq assigned here)
uint16_t count();             // summaries stored (<= slots())
uint16_t slots();             // ring capacity (817 on a 24C256)
bool get(uint16_t i, EeRec &r);   // i = 0 (oldest) .. count()-1
void clear();                 // logical wipe (header count = 0)

#else   // no EEPROM fitted: everything compiles away to nothing

inline void begin() {}
inline bool ok()                   { return false; }
inline void append(EeRec &)        {}
inline uint16_t count()            { return 0; }
inline uint16_t slots()            { return 0; }
inline bool get(uint16_t, EeRec &) { return false; }
inline void clear()                {}

#endif

}  // namespace eelog
