#include "cyclelog.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <sys/time.h>
#include <algorithm>
#include "config.h"
#include "control.h"      // dryer (log records), cfg
#include "buzzer.h"    // bz:: LOG_SAVED / BROWNOUT_RET (owner spec)
#include "eelog.h"     // AT24C256 long-term registry (v2.0.17)
#include "battery.h"

namespace cyclelog {

#include "battery.h"
static bool     s_active  = false;
static time_t   s_startE  = 0;
static char     s_startStr[24] = "";
static float    s_vbStart = NAN;         // battery at cycle start

// --------------------------------------------------------------- time
void applyTz() {
static char     s_startStr[24] = "";
static float    s_vbStart = NAN;         // battery at cycle start

// --------------------------------------------------------------- time
void applyTz() {
  int16_t m = cfg.tzMinutes;
  int16_t a = (m < 0) ? -m : m;
  char tz[16];
  // POSIX TZ: "DRY-5:30" means local = UTC + 5:30
  snprintf(tz, sizeof(tz), "DRY%c%d:%02d", (m >= 0) ? '-' : '+', a / 60, a % 60);
  setenv("TZ", tz, 1);
  tzset();
}

static bool clockSet() { return time(nullptr) > 1700000000; }  // ~Nov 2023

static void fmtLocal(time_t t, char *buf, size_t n) {
  if (t <= 1700000000) { snprintf(buf, n, "clock-not-set"); return; }
  struct tm tmv;
  localtime_r(&t, &tmv);
  strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tmv);
}

// --------------------------------------------------------------- boot
static std::vector<String> listFiles() {
  std::vector<String> out;
  File root = LittleFS.open(CYCLE_DIR);
  if (!root) return out;
  File f = root.openNextFile();
  while (f) {
    String n = f.name();
    int slash = n.lastIndexOf('/');
    if (slash >= 0) n = n.substring(slash + 1);
    if (n.length() && !n.startsWith("index.") && !f.isDirectory()) out.push_back(n);
    f = root.openNextFile();
  }
  return out;
}

static void prune() {
  auto files = listFiles();
  if (files.size() <= CYCLE_MAX_FILES) return;
  std::sort(files.begin(), files.end());          // oldest first (timestamped names)
  size_t excess = files.size() - CYCLE_MAX_FILES;
  for (size_t i = 0; i < excess; i++)
    LittleFS.remove(String(CYCLE_DIR) + "/" + files[i]);
  Preferences p;
  p.begin("dryer", true);
  s_counter = p.getUInt("cycn", 1);
  // a cycle that started but never finished = power loss mid-run.
  // Leave a trace in the history so the batch is not silently forgotten.
  uint32_t st = p.getUInt("cycStart", 0);
  p.end();
  if (st > 1700000000UL) {
    char fn[48]; struct tm tmv; char stamp[24];
    localtime_r((time_t *)&st, &tmv);
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tmv);
    snprintf(fn, sizeof(fn), "%s/cycle%s-INT.csv", CYCLE_DIR, stamp);
    File f = LittleFS.open(fn, FILE_WRITE);
    if (f) {
      f.print(F("sec,temp_avg_C,hum_avg_RH,hum_peak_RH,heat_pct,fan_pct,"
                "batt_V,batt_pct,wt_g,out_t_C,out_rh_RH\r\n"));
      f.print(F("# INTERRUPTED by power loss (no data rows were kept)\r\n"));
      f.close();
      char ws[24]; fmtLocal((time_t)st, ws, sizeof(ws));
      Serial.printf("[warn] the cycle started at %s was INTERRUPTED by a "
                    "power loss - marked in the history\n", ws);
      bz::play(BP::BROWNOUT_RET);      // #21: "the cycle was interrupted"
      if (eelog::ok()) {               // registry row: endR = 4 interrupted
        eelog::EeRec r = {};
        r.startEpoch = st;  r.mode = (uint8_t)cfg.mode;
        r.endR = 4;  r.flags = 0x02;
        eelog::append(r);
      }
    }
    Preferences q; q.begin("dryer", false); q.remove("cycStart"); q.end();
  }
  applyTz();
  eelog::begin();                     // AT24C256 cycle registry (v2.0.17)
  prune();
}

  // a cycle that started but never finished = power loss mid-run.
void start() {
  s_active   = true;
  s_startE   = time(nullptr);
  s_vbStart  = battery.valid() ? battery.volts() : NAN;
  fmtLocal(s_startE, s_startStr, sizeof(s_startStr));
  Preferences p; p.begin("dryer", false);         // power-loss marker
  p.putUInt("cycStart", (uint32_t)s_startE); p.end();
}

static String sanitize(const char *s) {           // keep CSV header lines clean
    snprintf(fn, sizeof(fn), "%s/cycle%s-INT.csv", CYCLE_DIR, stamp);
    File f = LittleFS.open(fn, FILE_WRITE);
    if (f) {
      f.print(F("sec,temp_avg_C,hum_avg_RH,hum_peak_RH,heat_pct,fan_pct,"
                "batt_V,batt_pct,wt_g,out_t_C,out_rh_RH\r\n"));
      f.print(F("# INTERRUPTED by power loss (no data rows were kept)\r\n"));
      f.close();
      char ws[24]; fmtLocal((time_t)st, ws, sizeof(ws));
      Serial.printf("[warn] the cycle started at %s was INTERRUPTED by a "
                    "power loss - marked in the history\n", ws);
      bz::play(BP::BROWNOUT_RET);      // #21: "the cycle was interrupted"
      if (eelog::ok()) {               // registry row: endR = 4 interrupted
        eelog::EeRec r = {};
        r.startEpoch = st;  r.mode = (uint8_t)cfg.mode;
        r.endR = 4;  r.flags = 0x02;
        eelog::append(r);
      }
    }
    Preferences q; q.begin("dryer", false); q.remove("cycStart"); q.end();
  }
  applyTz();
  eelog::begin();                     // AT24C256 cycle registry (v2.0.17)
  prune();
    localtime_r(&s_startE, &tmv);
    char stamp[24];
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tmv);
    snprintf(fname, sizeof(fname), "cycle%s-%03u.csv", stamp, (unsigned)(s_counter % 1000U));
  } else {
    snprintf(fname, sizeof(fname), "cycle-%010u.csv", (unsigned)s_counter);
  }

  File f = LittleFS.open(String(CYCLE_DIR) + "/" + fname, "w");
  p.putUInt("cycStart", (uint32_t)s_startE); p.end();
}

static String sanitize(const char *s) {           // keep CSV header lines clean
  f.printf("# ended,%s\n",     endStr);
  f.printf("# reason,%s\n",    sanitize(reason).c_str());
  f.printf("# setTemp,%.1f\n", cfg.setTemp);
  f.printf("# mode,%s\n", cfg.mode == 0 ? "AGARBATTI" :
                          cfg.mode == 2 ? "SILICAGEL" : "USER");
  if (cfg.targetG > 0) f.printf("# targetG,%.0f\n", cfg.targetG);
  if (!isnan(dryer.wtStartG())) f.printf("# wtStartG,%.0f\n", dryer.wtStartG());
  f.printf("# dryMinutes,%u\n",(unsigned)cfg.dryMinutes);
  f.printf("# elapsedMin,%.1f\n", elapsedMin);
  f.printf("# remainMin,%.1f\n",  remainMin);
  if (noteS.length()) f.printf("# note,%s\n", noteS.c_str());
  f.print(F("sec,temp_avg_C,hum_avg_RH,hum_peak_RH,heat_pct,fan_pct,batt_V,batt_pct,wt_g,out_t_C,out_rh_RH\r\n"));

  uint16_t n = dryer.logCount();
  for (uint16_t i = 0; i < n; i++) {
    const LogRec &r = dryer.logAt(i);
    if (r.wt10 == INT16_MIN)
      f.printf("%u,%.1f,%.1f,%.1f,%d,%d,%.1f,%d,\r\n",
               (unsigned)r.t, r.tAvg10 / 10.0f, r.hAvg10 / 10.0f, r.hMax10 / 10.0f,
               (int)r.heat, (int)r.fan, r.vb10 / 10.0f, (int)r.bat);
    else
      f.printf("%u,%.1f,%.1f,%.1f,%d,%d,%.1f,%d,%.0f",
               (unsigned)r.t, r.tAvg10 / 10.0f, r.hAvg10 / 10.0f, r.hMax10 / 10.0f,
               (int)r.heat, (int)r.fan, r.vb10 / 10.0f, (int)r.bat, r.wt10 / 10.0f);
    if (r.ot10 == INT16_MIN) f.print(",");
    else                     f.printf(",%.1f", r.ot10 / 10.0f);
    if (r.oh10 == INT16_MIN) f.print(",");
    else                     f.printf(",%.1f", r.oh10 / 10.0f);
    f.print("\r\n");
    if ((i & 0x3F) == 0) yield();
  }
  f.close();
  float remainMin   = (elapsedS >= totalS) ? 0.0f : (totalS - elapsedS) / 60.0f;
  Preferences p;
  p.begin("dryer", false);
  p.putUInt("cycn", ++s_counter);
  p.remove("cycStart");                           // cycle finished cleanly
  p.end();
  bz::play(BP::LOG_SAVED);                        // #29: log write OK

  // ---- AT24C256 long-term registry: one 40-byte summary per cycle -----
  if (eelog::ok()) {
    eelog::EeRec r = {};
    r.startEpoch = (uint32_t)s_startE;
    r.durS = elapsedS;
    r.mode = (uint8_t)cfg.mode;
    const char *rsn = reason ? reason : "";
    r.endR = (strncmp(rsn, "stopped", 7) == 0) ? 1 :
             (strncmp(rsn, "fault", 5) == 0)  ? 2 :
             (strncmp(rsn, "E19", 3) == 0)    ? 3 : 0;
    r.ecode = dryer.faultRec().code;
    r.setT10 = (int16_t)(cfg.setTemp * 10);
    if (strstr(rsn, "completed")) r.flags |= 0x04;
    if (dryer.scaleLost())        r.flags |= 0x01;
    float tSum = 0, tMax = -300, hMax = -300, oSum = 0; int tn = 0, on = 0;
    uint16_t nL = dryer.logCount();
    for (uint16_t i = 0; i < nL; i++) {
      const LogRec &L2 = dryer.logAt(i);
      if (L2.tAvg10 != INT16_MIN) { float t = L2.tAvg10 / 10.0f;
                                    tSum += t; if (t > tMax) tMax = t; tn++; }
      if (L2.hMax10 != INT16_MIN) { float h = L2.hMax10 / 10.0f;
                                    if (h > hMax) hMax = h; }
      if (L2.ot10  != INT16_MIN)  { oSum += L2.ot10 / 10.0f; on++; }
    }
    if (tn)          r.tAvg10 = (int16_t)constrain(tSum / tn * 10.0f, -300.0f, 300.0f);
    if (tMax > -300) r.tMax10 = (int16_t)(tMax * 10);
    if (hMax > -300) r.hMax10 = (int16_t)constrain(hMax * 10, 0.0f, 1000.0f);
    if (on)          r.outT10 = (int16_t)constrain(oSum / on * 10.0f, -300.0f, 300.0f);
    if (!isnan(dryer.wtStartG())) r.wtS10 = (int16_t)constrain(dryer.wtStartG() / 10.0f, -3200.0f, 3200.0f);
    if (!isnan(dryer.finalG()))   r.wtE10 = (int16_t)constrain(dryer.finalG() / 10.0f, -3200.0f, 3200.0f);
    if (cfg.targetG > 0)          r.wtT10 = (int16_t)constrain(cfg.targetG / 10.0f, 0.0f, 3200.0f);
    if (!isnan(s_vbStart))        r.vbS10 = (int16_t)(s_vbStart * 10);
    if (battery.valid())          r.vbE10 = (int16_t)(battery.volts() * 10);
    eelog::append(r);
  }

  prune();
}

uint32_t cycleNo()  { return s_counter; }

uint16_t fileCount() { return (uint16_t)listFiles().size(); }

String lastFile() {              // names are timestamps -> sort = newest last
  auto files = listFiles();
  if (files.empty()) return String();
  std::sort(files.begin(), files.end());
  return files.back();
}

void clear() {
  eelog::clear();                     // AT24C256 registry too
  auto files = listFiles();
  for (auto &n : files) LittleFS.remove(String(CYCLE_DIR) + "/" + n);
}
  File f = LittleFS.open(String(CYCLE_DIR) + "/" + fname, "w");
  if (!f) return;

  String noteS = sanitize(note);
  f.printf("# started,%s\n",   s_startStr);
  f.printf("# ended,%s\n",     endStr);
  f.printf("# reason,%s\n",    sanitize(reason).c_str());
  f.printf("# setTemp,%.1f\n", cfg.setTemp);
  f.printf("# mode,%s\n", cfg.mode == 0 ? "AGARBATTI" :
                          cfg.mode == 2 ? "SILICAGEL" : "USER");
  if (cfg.targetG > 0) f.printf("# targetG,%.0f\n", cfg.targetG);
  if (!isnan(dryer.wtStartG())) f.printf("# wtStartG,%.0f\n", dryer.wtStartG());
  f.printf("# dryMinutes,%u\n",(unsigned)cfg.dryMinutes);
  f.printf("# elapsedMin,%.1f\n", elapsedMin);
  f.printf("# remainMin,%.1f\n",  remainMin);
  if (noteS.length()) f.printf("# note,%s\n", noteS.c_str());
  f.print(F("sec,temp_avg_C,hum_avg_RH,hum_peak_RH,heat_pct,fan_pct,batt_V,batt_pct,wt_g,out_t_C,out_rh_RH\r\n"));

  uint16_t n = dryer.logCount();
  for (uint16_t i = 0; i < n; i++) {
    const LogRec &r = dryer.logAt(i);
    if (r.wt10 == INT16_MIN)
      f.printf("%u,%.1f,%.1f,%.1f,%d,%d,%.1f,%d,\r\n",
               (unsigned)r.t, r.tAvg10 / 10.0f, r.hAvg10 / 10.0f, r.hMax10 / 10.0f,
               (int)r.heat, (int)r.fan, r.vb10 / 10.0f, (int)r.bat);
    else
      f.printf("%u,%.1f,%.1f,%.1f,%d,%d,%.1f,%d,%.0f",
               (unsigned)r.t, r.tAvg10 / 10.0f, r.hAvg10 / 10.0f, r.hMax10 / 10.0f,
               (int)r.heat, (int)r.fan, r.vb10 / 10.0f, (int)r.bat, r.wt10 / 10.0f);
    if (r.ot10 == INT16_MIN) f.print(",");
    else                     f.printf(",%.1f", r.ot10 / 10.0f);
    if (r.oh10 == INT16_MIN) f.print(",");
    else                     f.printf(",%.1f", r.oh10 / 10.0f);
    f.print("\r\n");
    if ((i & 0x3F) == 0) yield();
  }
  f.close();

  Preferences p;
  p.begin("dryer", false);
  p.putUInt("cycn", ++s_counter);
  p.remove("cycStart");                           // cycle finished cleanly
  p.end();
  bz::play(BP::LOG_SAVED);                        // #29: log write OK

  // ---- AT24C256 long-term registry: one 40-byte summary per cycle -----
  if (eelog::ok()) {
    eelog::EeRec r = {};
    r.startEpoch = (uint32_t)s_startE;
    r.durS = elapsedS;
    r.mode = (uint8_t)cfg.mode;
    const char *rsn = reason ? reason : "";
    r.endR = (strncmp(rsn, "stopped", 7) == 0) ? 1 :
             (strncmp(rsn, "fault", 5) == 0)  ? 2 :
             (strncmp(rsn, "E19", 3) == 0)    ? 3 : 0;
    r.ecode = dryer.faultRec().code;
    r.setT10 = (int16_t)(cfg.setTemp * 10);
    if (strstr(rsn, "completed")) r.flags |= 0x04;
    if (dryer.scaleLost())        r.flags |= 0x01;
    float tSum = 0, tMax = -300, hMax = -300, oSum = 0; int tn = 0, on = 0;
    uint16_t nL = dryer.logCount();
    for (uint16_t i = 0; i < nL; i++) {
      const LogRec &L2 = dryer.logAt(i);
      if (L2.tAvg10 != INT16_MIN) { float t = L2.tAvg10 / 10.0f;
                                    tSum += t; if (t > tMax) tMax = t; tn++; }
      if (L2.hMax10 != INT16_MIN) { float h = L2.hMax10 / 10.0f;
                                    if (h > hMax) hMax = h; }
      if (L2.ot10  != INT16_MIN)  { oSum += L2.ot10 / 10.0f; on++; }
    }
    if (tn)          r.tAvg10 = (int16_t)constrain(tSum / tn * 10.0f, -300.0f, 300.0f);
    if (tMax > -300) r.tMax10 = (int16_t)(tMax * 10);
    if (hMax > -300) r.hMax10 = (int16_t)constrain(hMax * 10, 0.0f, 1000.0f);
    if (on)          r.outT10 = (int16_t)constrain(oSum / on * 10.0f, -300.0f, 300.0f);
    if (!isnan(dryer.wtStartG())) r.wtS10 = (int16_t)constrain(dryer.wtStartG() / 10.0f, -3200.0f, 3200.0f);
    if (!isnan(dryer.finalG()))   r.wtE10 = (int16_t)constrain(dryer.finalG() / 10.0f, -3200.0f, 3200.0f);
    if (cfg.targetG > 0)          r.wtT10 = (int16_t)constrain(cfg.targetG / 10.0f, 0.0f, 3200.0f);
    if (!isnan(s_vbStart))        r.vbS10 = (int16_t)(s_vbStart * 10);
    if (battery.valid())          r.vbE10 = (int16_t)(battery.volts() * 10);
    eelog::append(r);
  }

  prune();
}

uint32_t cycleNo()  { return s_counter; }

uint16_t fileCount() { return (uint16_t)listFiles().size(); }

String lastFile() {              // names are timestamps -> sort = newest last
  auto files = listFiles();
  if (files.empty()) return String();
  std::sort(files.begin(), files.end());
  return files.back();
}

void clear() {
  eelog::clear();                     // AT24C256 registry too
  auto files = listFiles();
  for (auto &n : files) LittleFS.remove(String(CYCLE_DIR) + "/" + n);
}

// --------------------------------------------------------------- listing
String safePath(const String &name) {
  if (name.length() < 5 || name.length() > 47) return String();
  if (!name.startsWith("cycle") || !name.endsWith(".csv")) return String();
  if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) return String();
  String p = String(CYCLE_DIR) + "/" + name;
  if (!LittleFS.exists(p)) return String();
  return p;
}

String listingJson() {
  auto files = listFiles();
  std::sort(files.begin(), files.end());
  String out = "[";
  bool first = true;
  for (int i = (int)files.size() - 1; i >= 0; i--) {   // newest first
    File f = LittleFS.open(String(CYCLE_DIR) + "/" + files[i], "r");
    if (!f) continue;
    String started = "-", ended = "-", reason = "-", note = "";
    String setTemp = "-", dryMin = "-", elMin = "-", remMin = "-";
    char line[96];
    while (f.available()) {
      int len = f.readBytesUntil('\n', line, sizeof(line) - 1);
      line[len] = 0;
      if (line[0] != '#') break;
      char *v = strchr(line, ',');
      if (!v) continue;
      *v = 0; v++;
      String val = String(v);
      if      (!strcmp(line, "# started"))    started = val;
      else if (!strcmp(line, "# ended"))      ended = val;
      else if (!strcmp(line, "# reason"))     reason = val;
      else if (!strcmp(line, "# setTemp"))    setTemp = val;
      else if (!strcmp(line, "# dryMinutes")) dryMin = val;
      else if (!strcmp(line, "# elapsedMin")) elMin = val;
      else if (!strcmp(line, "# remainMin"))  remMin = val;
      else if (!strcmp(line, "# note"))       note = val;
    }
    f.close();
    if (!first) out += ",";
    first = false;
    out += "{\"file\":\"" + files[i] + "\",\"started\":\"" + started +
           "\",\"ended\":\"" + ended + "\",\"reason\":\"" + reason +
           "\",\"setTemp\":" + setTemp + ",\"dryMin\":" + dryMin +
           ",\"elapsedMin\":" + elMin + ",\"remainMin\":" + remMin;
    if (note.length()) out += ",\"note\":\"" + note + "\"";
    out += "}";
  }
  out += "]";
  return out;
}

} // namespace cyclelog