#include "menu.h"
#include "control.h"
#include "buzzer.h"   // bz:: patterns (owner spec)
#include "web.h"
#include <time.h>

// rows: name, value formatting, min, max, step
enum { R_TEMP = 0, R_TIME, R_RHTGT, R_WTGT, R_MODE,
       R_CLKD, R_CLKT, R_OTA, R_EXIT, N_ROWS };

namespace menu {

static bool     sUp = false;      // list screen showing
static bool     sEdit = false;    // value-entry screen
static bool     sInfo = false;    // OTA firmware-update info screen
static uint8_t  sCur = 0;         // selected row
static char     sBuf[8] = "";     // digits typed
static char     sVal[16];         // formatted value buffer
static char     sHint[24];

bool active()  { return sUp; }
bool editing() { return sEdit; }
bool info()    { return sInfo; }
uint8_t cursor() { return sCur; }
uint8_t itemCount() { return N_ROWS; }
const char *editBuffer() { return sBuf; }
const char *editHint()  { return sHint; }

const char *itemName(uint8_t i) {
  switch (i) {
    case R_TEMP:  return "1 Temperature";
    case R_TIME:  return "2 Drying time";
    case R_RHTGT: return "3 RH target";
    case R_WTGT:  return "4 Target weight";
    case R_MODE:  return "5 Mode";
    case R_CLKD:  return "6 Clock date";
    case R_CLKT:  return "7 Clock time";
    case R_OTA:   return "8 Firmware update";
    default:      return "0 Exit";
  }
}

const char *itemValue(uint8_t i) {
  switch (i) {
    case R_TEMP:
      snprintf(sVal, sizeof(sVal), "%.0f C", (double)cfg.setTemp);
      snprintf(sHint, sizeof(sHint), "40-80 C");
      return sVal;
    case R_TIME:
      snprintf(sVal, sizeof(sVal), "%u min", (unsigned)cfg.dryMinutes);
      snprintf(sHint, sizeof(sHint), "minutes 1-1440");
      return sVal;
    case R_RHTGT:
      if (cfg.requireHum) snprintf(sVal, sizeof(sVal), "%.0f %%", (double)cfg.humTarget);
      else                snprintf(sVal, sizeof(sVal), "off");
      snprintf(sHint, sizeof(sHint), "0=off, 20-80 %%");
      return sVal;
    case R_WTGT:
      if (cfg.targetG > 0) snprintf(sVal, sizeof(sVal), "%.0f g", (double)cfg.targetG);
      else                 snprintf(sVal, sizeof(sVal), "off");
      snprintf(sHint, sizeof(sHint), "grams 0=off");
      return sVal;
    case R_MODE:
      return cfg.mode == 0 ? "AGARBATTI" :
             cfg.mode == 2 ? "SILICAGEL" : "USER DEFINED";
    case R_CLKD: {
      time_t tN = time(nullptr);
      if (tN > (time_t)1700000000) {
        struct tm m; localtime_r(&tN, &m);
        { // clamp the parts so snprintf's bound proof holds (sVal[16])
          int y  = m.tm_year + 1900;  if (y < 0) y = 0;      if (y > 9999) y = 9999;
          int mo = m.tm_mon + 1;      if (mo < 1) mo = 1;    if (mo > 12) mo = 12;
          int dd = m.tm_mday;         if (dd < 1) dd = 1;    if (dd > 31) dd = 31;
          snprintf(sVal, sizeof(sVal), "%04d-%02d-%02d", y, mo, dd);
        }
      } else snprintf(sVal, sizeof(sVal), "not set");
      snprintf(sHint, sizeof(sHint), "YYMMDD");
      return sVal;
    }
    case R_CLKT: {
      time_t tN = time(nullptr);
      if (tN > (time_t)1700000000) {
        struct tm m; localtime_r(&tN, &m);
        snprintf(sVal, sizeof(sVal), "%02d:%02d", m.tm_hour, m.tm_min);
      } else snprintf(sVal, sizeof(sVal), "--:--");
      snprintf(sHint, sizeof(sHint), "HHMM 24h");
      return sVal;
    }
    case R_OTA:   return "web/IDE";
    default: return "";
  }
}

static void saveApply() {
  saveSettings(cfg);
  dryer.applySettings(cfg);
}

// commit the typed value for row r
static void commit(uint8_t r) {
  float v = atof(sBuf);
  if (sBuf[0] == 0) return;                 // nothing typed: keep old
  switch (r) {
    case R_TEMP:  cfg.setTemp    = constrain(v, 40.0f, 80.0f); break;
    case R_TIME:  cfg.dryMinutes = (uint32_t)constrain(v, 1.0f, 1440.0f); break;
    case R_RHTGT:
      if (v < 20.0f) { cfg.requireHum = false; }             // 0 = off
      else { cfg.requireHum = true; cfg.humTarget = constrain(v, 20.0f, 80.0f); }
      break;
    case R_WTGT:  cfg.targetG    = constrain(v, 0.0f, 9000.0f); break;
    case R_CLKD: {                       // type YYMMDD (e.g. 260923)
      long d = atol(sBuf);
      int yy = (int)(d / 10000), mm = (int)(d / 100 % 100), dd = (int)(d % 100);
      if (yy < 0 || yy > 99 || mm < 1 || mm > 12 || dd < 1 || dd > 31) return;
      time_t tN = time(nullptr); struct tm m; localtime_r(&tN, &m);
      m.tm_year = yy + 100;  m.tm_mon = mm - 1;  m.tm_mday = dd;
      web::clockSetManual(mktime(&m));
      break;
    }
    case R_CLKT: {                       // type HHMM, 24 h (e.g. 1435)
      long d = atol(sBuf);
      int hh = (int)(d / 100), mi = (int)(d % 100);
      if (hh < 0 || hh > 23 || mi < 0 || mi > 59) return;
      time_t tN = time(nullptr); struct tm m; localtime_r(&tN, &m);
      m.tm_hour = hh;  m.tm_min = mi;  m.tm_sec = 0;
      web::clockSetManual(mktime(&m));
      break;
    }
    default: return;
  }
  saveApply();
  bz::play(BP::SAVED);                       // #5: value written
  Serial.printf("[menu] %s -> %s\\n", itemName(r), sBuf);
}

static void openEdit() {
  if (sCur == R_MODE) {                     // mode row: cycle, no typing
    dryer.applyMode((cfg.mode + 1) % 3);
    return;
  }
  if (sCur == R_OTA) {                      // OTA info screen (spec)
    sEdit = false;
    sInfo = true;
    Serial.println(F("[menu] OTA firmware update: WiFi AgarbattiDryer -> "
                     "http://192.168.4.1 (Firmware update) or Arduino IDE "
                     "-> Port -> smart-dehumidifier at 192.168.4.1"));
    return;
  }
  if (sCur == R_EXIT) { sUp = false; sEdit = false; return; }
  sEdit = true;
  sBuf[0] = 0;
  itemValue(sCur);                          // refresh the hint
}

bool key(char k) {
  if (sInfo) {                              // OTA info screen: close keys
    if (k == 'A' || k == '5' || k == '#' || k == '*') sInfo = false;
    return true;
  }
  if (!sUp) {
    if (k == 'B') { sUp = true; sEdit = false; sCur = 0;
      bz::play(BP::KEY);
      Serial.println(F("[menu] open - 2/8 select, A or 5 enter, # close"));
      return true; }
    return false;                           // B/A/#/digits: not ours yet
  }
  if (sEdit) {                              // ---- value entry ----
    if (k >= '0' && k <= '9' && strlen(sBuf) < 5) {
      size_t n = strlen(sBuf);
      sBuf[n] = k; sBuf[n + 1] = 0;          // digits accumulate
      return true;
    }
    switch (k) {
      case 'A':
      case '5': commit(sCur); sEdit = false; bz::play(BP::KEY); return true;  // ENTER
      case '#': sEdit = false; bz::play(BP::BACK); return true;               // BACK #6
      case '*': sEdit = false; sUp = false; bz::play(BP::BACK); return true;  // HOME
      default:  return true;                                // swallow rest
    }
  }
  switch (k) {                              // ---- list navigation ----
    case '2': if (sCur > 0) sCur--; return true;            // UP
    case '8': if (sCur < N_ROWS - 1) sCur++; return true;   // DOWN
    case '4': if (sCur > 0) sCur--; return true;            // LEFT
    case '6': if (sCur < N_ROWS - 1) sCur++; return true;   // RIGHT
    case 'A':                              // ENTER (A)
    case '5': openEdit(); return true;                      // ENTER (5 = OK, owner spec)
    case '#': sUp = false; bz::play(BP::BACK); return true;  // BACK #6
    case '*': sUp = false; bz::play(BP::BACK); return true;  // HOME
    case 'B': sUp = false; return true;                     // toggle menu
    case '0': sUp = false; return true;                     // 0 = exit
    default:  return true;                                  // swallow
  }
}

}  // namespace menu