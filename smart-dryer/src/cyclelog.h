/**
 * @file cyclelog.h
 * @brief Per-cycle history stored in LittleFS (survives power loss).
 *
 * Every drying cycle is written to its own CSV file in /cycles when the
 * cycle ends (completed / stopped by user / fault / battery empty), and
 * the website can list + download them. Oldest files are auto-pruned
 * beyond CYCLE_MAX_FILES.
 */
#pragma once
#include <Arduino.h>

namespace cyclelog {
void begin();                            // mount FS, restore counter, prune
void start();                            // called by Dryer::start()
void finish(const char *reason, const char *note);   // write + prune
void clear();
String lastFile();               // newest cycle CSV name ("" = none yet)                            // delete all history

String listingJson();                    // /api/cycles payload
String safePath(const String &name);     // "" if invalid, else "/cycles/<name>"
String listingJson();                    // /api/cycles payload
/** Apply cfg.tzMinutes to libc (file names / header stamps use it). */
void applyTz();

uint32_t cycleNo();          // NVS cycle counter (maintenance nag #33)
uint16_t fileCount();        // stored cycles (storage-full warning #30)

} // namespace cyclelog

uint32_t cycleNo();          // NVS cycle counter (maintenance nag #33)
uint16_t fileCount();        // stored cycles (storage-full warning #30)

} // namespace cyclelog