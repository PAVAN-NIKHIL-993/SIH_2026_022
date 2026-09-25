#include "web.h"
#include "config.h"
#include "control.h"
#include "cyclelog.h"
#include "eelog.h"
#include "keypad.h"
#include "menu.h"       // v2.0.19: menu state for the web virtual keypad
#include "scale.h"
#include "door.h"
#include "supply.h"
#include "dht.h"
#include "webui.h"
#include "rtc.h"       // mirror every real time set into the DS1307 RTC
#include <WiFi.h>
#include <LittleFS.h>
#include <sys/time.h>
#include <Preferences.h>
#include <Update.h>

// v2.0.19: the web virtual keypad feeds the SAME path as the physical one
void dryerKey(char k);

static WebServer server(80);
static DNSServer  dns;

// ---- wall clock: phone-synced, persisted to NVS, restored at boot -------
// The S3/classic RTC dies on a full battery disconnect; we save the epoch
// every TIME_SAVE_MS and on every phone sync, so a power-cycled unit
// comes back with the LAST-SAVED time (stale, flagged) instead of 1970.
// "clockSet" reported to the site stays false until a real phone sync,
// so the first dashboard visit always corrects the drift.
static bool sClockSynced = false;              // phone synced THIS boot

static void clockSave(uint32_t ep) {
  if (ep <= 1700000000UL) return;              // refuse junk epochs
  Preferences p;
  p.begin("dryer", false);                     // same namespace, own key
  p.putUInt("tsep", ep);
  p.end();
}

// mirror a real time set into the DS1307 (no-op when absent). The chip
// then holds the clock across a full power-down.
static void clockToRtc() {
#if RTC_ENABLED
  rtc::writeNow();
#endif
}

// keypad menu rows 6/7 (and any manual set) land here: set the clock,
// flag it as really set, persist immediately.
// v2.0.19-fix: must be web::clockSetManual - web.h declares it inside
// namespace web and menu.cpp calls it qualified (link error otherwise).
void web::clockSetManual(time_t ep) {
  if (ep <= (time_t)1700000000) return;
  struct timeval tv;
  tv.tv_sec = ep;  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  sClockSynced = true;
  clockSave((uint32_t)ep);
  clockToRtc();
}

// ---------------------------------------------------------------------
//  tiny JSON helpers - keeps the firmware 100 % library-free
// ---------------------------------------------------------------------
static String jesc(const String &s) {          // make a string JSON-safe
  String o;
  o.reserve(s.length() + 8);
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n' || c == '\r') o += ' ';
    else o += c;
  }
  return o;
}

static float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// append "key":value pairs (no trailing comma)
static void jn(String &o, const char *k, float v, int dec, bool ok = true) {
  o += "\""; o += k; o += "\":";
  o += ok ? String(v, dec) : String("null");
}
static void ji(String &o, const char *k, long v) {
  o += "\""; o += k; o += "\":"; o += v;
}
static void ji(String &o, long v) {   // bare value (key already emitted)
  o += String(v);
}
static void jb(String &o, const char *k, bool v) {
  o += "\""; o += k; o += "\":"; o += (v ? "true" : "false");
}
static void js(String &o, const char *k, const String &v) {
  o += "\""; o += k; o += "\":\""; o += jesc(v); o += "\"";
}

// ---- minimal flat-object parser (for POST bodies from our own pages) --
static int jfind(const String &b, const char *k) {
  return b.indexOf(String("\"") + k + "\"");
}
static bool jhas(const String &b, const char *k) { return jfind(b, k) >= 0; }

static String jval(const String &b, const char *k) {   // raw value or ""
  int i = jfind(b, k);
  if (i < 0) return "";
  int c = b.indexOf(':', i + strlen(k) + 2);
  if (c < 0) return "";
  int e1 = b.indexOf(',', c), e2 = b.indexOf('}', c);
  int e;
  if (e1 < 0) e = e2; else if (e2 < 0) e = e1; else e = (e1 < e2 ? e1 : e2);
  if (e < 0) e = b.length();
  String v = b.substring(c + 1, e);
  v.trim();
  return v;
}
static float jgetnum(const String &b, const char *k, float def) {
  String v = jval(b, k);
  return v.length() ? v.toFloat() : def;
}
static bool jgetbool(const String &b, const char *k, bool def) {
  String v = jval(b, k);
  if (!v.length()) return def;
  return v.startsWith("true") || v.toInt() == 1;
}
static String jgetstr(const String &b, const char *k) {
  String v = jval(b, k);
  if (!v.startsWith("\"")) return "";
  int e = v.indexOf('"', 1);
  return e > 1 ? v.substring(1, e) : "";
}

// ---------------------------------------------------------------------
//  settings <-> JSON
// ---------------------------------------------------------------------
static void buildSettings(String &o, const Settings &s) {
  jn(o, "setTemp", s.setTemp, 1);     o += ",";
  jn(o, "tempHyst", s.tempHyst, 1);   o += ",";
  jn(o, "maxTemp", s.maxTemp, 1);     o += ",";
  jn(o, "humHigh", s.humHigh, 1);     o += ",";
  jn(o, "humLow", s.humLow, 1);       o += ",";
  jn(o, "humTarget", s.humTarget, 1); o += ",";
  jb(o, "requireHum", s.requireHum);  o += ",";
  ji(o, "dryMinutes", s.dryMinutes);  o += ",";
  ji(o, "fanMin", s.fanMin);          o += ",";
  ji(o, "fanIn", s.fanIn);            o += ",";
  jn(o, "targetG",      s.targetG, 0);   o += ",";
  ji(o, "stickCount",   s.stickCount);   o += ",";
  jn(o, "stickWetG",    s.stickWetG, 2);  o += ",";
  jn(o, "pasteWaterPct", s.pasteWaterPct, 0);  o += ",";
  jn(o, "targetMoistPct", s.targetMoistPct, 0);  o += ",";
  ji(o, "fanTrigRH",    s.fanTrigRH);    o += ",";
  ji(o, "fanTrigMin",   s.fanTrigMin);   o += ",";
  ji(o, "fanBurstS",    s.fanBurstS);    o += ",";
  ji(o, "mode",         s.mode);         o += ",";
  ji(o, "fanOut", s.fanOut);          o += ",";
  ji(o, "fanSlope", s.fanSlope);      o += ",";
  ji(o, "heaterMax", s.heaterMax);    o += ",";
  ji(o, "cooldownSec", s.cooldownSec);o += ",";
  ji(o, "bypassPct", s.bypassPct);    o += ",";
  ji(o, "cutoffPct", s.cutoffPct);    o += ",";
  ji(o, "battType", s.battType);      o += ",";
  ji(o, "tzMinutes", s.tzMinutes);    o += ",";
  jb(o, "smartVent", s.smartVent);    o += ",";
  jb(o, "boostHeat", s.boostHeat);    o += ",";
  jn(o, "kp", s.kp, 1); o += ","; jn(o, "ki", s.ki, 2); o += ","; jn(o, "kd", s.kd, 1); o += ",";
  jb(o, "requireWeight", s.requireWeight);  o += ",";
  jn(o, "weightRateG", s.weightRateG, 1);   o += ",";
  ji(o, "weightMinY", s.weightMinY);
}

// ---------------------------------------------------------------------
//  GET /  and  GET /online
// ---------------------------------------------------------------------
static void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

// v2.0.7: the kiosk display page - a mounted phone/tablet BECOMES the
// hardware display (read-only, huge type, 1 Hz). The TFT is gone.
static void handleDisplay() {
  server.send_P(200, "text/html", DISPLAY_HTML);
}

// OPTIONAL online UI: tiny bridge that frames the GitHub Pages dashboard
// and relays its /api calls (see webui.h for why the bridge exists).
static void handleOnline() {
  String page = FPSTR(ONLINE_LOADER_HTML);
  page.replace("__ONLINE_URL__", ONLINE_UI_URL);
  server.send(200, "text/html", page);
}

// ---------------------------------------------------------------------
//  GET /api/data  - everything the dashboard shows, polled every 2 s
// ---------------------------------------------------------------------
static void handleData() {
  String o;
  o.reserve(2800);
  o += "{";
  js(o, "state", stateName(dryer.state()));            o += ",";
  js(o, "fw", FW_VERSION);                             o += ",";
  js(o, "fault", dryer.faultWhy());                    o += ",";
  { // E-code fault records (owner spec v2.0.14)
    const FaultRec &fr = dryer.faultRec();
    o += "\"faultRec\":{\"code\":";  ji(o, fr.code);          o += ",";
    js(o, "name", eName(fr.code));                     o += ",";
    jb(o, "critical", fr.sev == 1);                    o += ",";
    jb(o, "active", fr.active);                        o += ",";
    ji(o, "sinceS", fr.sinceMs ? (uint32_t)((millis() - fr.sinceMs) / 1000UL) : 0);
    o += ",";  jn(o, "val",   isnan(fr.val)   ? 0.0f : fr.val,   1);
    o += ",";  jn(o, "limit", isnan(fr.limit) ? 0.0f : fr.limit, 1);
    o += "},";
    const FaultRec &wr = dryer.warnRec();
    o += "\"warnRec\":{\"code\":";  ji(o, wr.code);          o += ",";
    js(o, "name", eName(wr.code));                     o += ",";
    jb(o, "active", wr.active);
    o += "},";
  }

  o += "\"s1\":{\"ok\":";
  o += sensors.s1ok() ? "true" : "false";              o += ",";
  jn(o, "t", sensors.t1(), 1, sensors.s1ok());         o += ",";
  jn(o, "h", sensors.h1(), 1, sensors.s1ok());         o += "},";
  o += "\"s2\":{\"ok\":";
  o += sensors.s2ok() ? "true" : "false";              o += ",";
  jn(o, "t", sensors.t2(), 1, sensors.s2ok());         o += ",";
  jn(o, "h", sensors.h2(), 1, sensors.s2ok());         o += "},";

  jn(o, "tAvg", sensors.tAvg(), 1, sensors.anyOk());   o += ",";
  jn(o, "hAvg", sensors.hAvg(), 1, sensors.anyOk());   o += ",";
  jn(o, "hMax", sensors.hMax(), 1, sensors.anyOk());   o += ",";
  ji(o, "heat", dryer.heatDuty());                     o += ",";
  jb(o, "boost", dryer.boosting());                    o += ",";
  o += "\"man\":{";                                        // knob override state
  ji(o, "pct", dryer.manualPct());                      o += ",";
  ji(o, "left", dryer.manualLeftS());                   o += "},";

  o += "\"kp\":{";                                   // hex keypad state
  o += keypad.ok() ? "true" : "false";                o += ",";
  js(o, "last", keypad.last() ? String(keypad.last()) : String(""));
  o += "},";

  o += "\"scale\":{\"ok\":";                       // weigh scale
  o += scale.ok() ? "true" : "false";              o += ",";
  jn(o, "g", scale.ok() ? scale.grams() : 0.0f, 0); o += ",";
  jn(o, "rate", scale.ok() ? scale.rate()  : 0.0f, 1);
  o += ",";  jb(o, "cal", door::calibrated());
  o += "},";

  o += "\"dht\":{\"ok\":";  o += dht::outdoor.ok ? "true" : "false";  o += ",";
  jn(o, "t", isnan(dht::outdoor.t) ? 0.0f : dht::outdoor.t, 1);   o += ",";
  jn(o, "h", isnan(dht::outdoor.h) ? 0.0f : dht::outdoor.h, 0);
  o += "},";

  o += "\"door\":{\"fitted\":";                     // lock workflow state
  o += door::fitted() ? "true" : "false";          o += ",";
  o += door::locked() ? "true" : "false";          o += ",";
  o += door::closed() ? "true" : "false";          o += ",";
  js(o, "phase", door::phase());                   o += ",";
  jn(o, "batch", door::batchG(), 0);
  o += "},";
  ji(o, "fan",  dryer.fanDuty());                      o += ",";
  ji(o, "fanIn",  dryer.fanInDuty());                   o += ",";
  o += "\"supply\":{\"name\":\"";  o += supply::modeName();
  o += "\",\"solar\":";            o += supply::solarRequested() ? "true" : "false";
  o += ",\"live\":";              o += supply::optoLive() ? "true" : "false";
  o += ",\"mode\":";              o += String(cfg.mode);
  o += "},";
  ji(o, "fanOut", dryer.fanOutDuty());                  o += ",";
  jb(o, "relay", dryer.relayOn());                     o += ",";
  ji(o, "elapsed", dryer.elapsedS());                  o += ",";
  ji(o, "remaining", dryer.remainingS());              o += ",";
  ji(o, "logCount", dryer.logCount());                 o += ",";
  o += "\"eelog\":{";                                  // AT24C256 registry
  o += eelog::ok() ? "true" : "false";  o += ",";
  ji(o, "used", eelog::count());       o += ",";
  ji(o, "slots", eelog::slots());
  o += "},";

  // v2.0.19: on-device menu state (web virtual keypad); null = menu closed
  {
    String m = "null";
    if (menu::active()) {
      m = "{\"cur\":" + String(menu::cursor()) + ",\"edit\":" +
          String(menu::editing() ? "true" : "false") + ",\"items\":[";
      for (uint8_t i = 0; i < menu::itemCount() && i < 20; i++)
        m += "{\"n\":\"" + String(menu::itemName(i)) + "\",\"v\":\"" +
             String(menu::itemValue(i)) + "\"},";
      if (m.endsWith(",")) m.remove(m.length() - 1);
      m += "],\"buf\":\"" + String(menu::editBuffer()) +
           "\",\"hint\":\"" + String(menu::editHint()) + "\"}";
    }
    o += "\"menu\":" + m + ",";
  }
  jn(o, "wtStart",   isnan(dryer.wtStartG())  ? 0.0f : dryer.wtStartG(),  0); o += ",";
  jn(o, "finalG",    isnan(dryer.finalG())    ? 0.0f : dryer.finalG(),    0); o += ",";
  jn(o, "moistureG", isnan(dryer.moistureG()) ? 0.0f : dryer.moistureG(), 0); o += ",";
  ji(o, "cycleSecs", dryer.endElapsedS());              o += ",";
  ji(o, "heap", ESP.getFreeHeap());                    o += ",";

  time_t nowE = time(nullptr);                         // live date & time
  ji(o, "now", (long)nowE);                            o += ",";
  jb(o, "clockSet", sClockSynced && nowE > 1700000000); o += ",";
  ji(o, "tz", cfg.tzMinutes);                          o += ",";

  // outdoor weather + merged outdoor source
  o += "\"wx\":{\"ok\":";
  o += (weather.rxMs != 0) ? "true" : "false";
  if (weather.rxMs) {
    o += ",";
    jn(o, "t", weather.tempC, 1);            o += ",";
    jn(o, "h", weather.humRH, 1);            o += ",";
    jn(o, "r", weather.rainPct, 1);          o += ",";
    jn(o, "w", weather.windKmh, 1);          o += ",";
    ji(o, "code", weather.code);             o += ",";
    js(o, "loc", weather.loc);               o += ",";
    ji(o, "age", (long)((millis() - weather.rxMs) / 60000UL)); o += ",";
    jb(o, "manual", weather.manual);         o += ",";
    jb(o, "fresh", weatherFresh());
  }
  o += ",\"src\":\"";
  o += outdoorSrc();
  o += "\",";
  float oT, oH;
  bool hasOut = getOutdoor(oT, oH);
  jn(o, "outT", oT, 1, hasOut);             o += ",";
  jn(o, "outH", oH, 1, hasOut);             o += "},";

  o += "\"bat\":{";
  jn(o, "v", battery.valid() ? battery.volts() : 0.0f, 2); o += ",";
  ji(o, "pct", battery.percent());          o += ",";
  jb(o, "bypass", battery.bypass());        o += ",";
  jb(o, "valid", battery.valid());          o += ",";
  js(o, "name", battery.profile(cfg.battType).name);
  o += "},\"set\":{";
  buildSettings(o, cfg);
  o += "},\"defs\":{";
  buildSettings(o, defaultSettings());
  o += "}}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", o);
}

// ---------------------------------------------------------------------
//  GET /api/history - seed the chart after a page refresh
// ---------------------------------------------------------------------
static void handleHistory() {
  uint16_t n = dryer.logCount();
  uint16_t skip = n > 600 ? n - 600 : 0;
  String o;
  o.reserve(2000 + (n - skip) * 30);
  o += "{\"t\":[";
  for (uint16_t i = skip; i < n; i++)
    o += String(dryer.logAt(i).t) + (i + 1 < n ? "," : "");
  o += "],\"temp\":[";
  for (uint16_t i = skip; i < n; i++)
    o += String(dryer.logAt(i).tAvg10 / 10.0, 1) + (i + 1 < n ? "," : "");
  o += "],\"hum\":[";
  for (uint16_t i = skip; i < n; i++)
    o += String(dryer.logAt(i).hAvg10 / 10.0, 1) + (i + 1 < n ? "," : "");
  o += "]}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", o);
}

// ---------------------------------------------------------------------
//  GET /api/log.csv - the full data dump
// ---------------------------------------------------------------------
static void handleCsv() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  server.sendContent(F("sec,temp_avg_C,hum_avg_RH,hum_peak_RH,heat_pct,fan_pct,batt_V,batt_pct\r\n"));
  String chunk;
  chunk.reserve(512);
  uint16_t n = dryer.logCount();
  for (uint16_t i = 0; i < n; i++) {
    const LogRec &r = dryer.logAt(i);
    chunk = String(r.t) + ',' + String(r.tAvg10 / 10.0, 1) + ',' +
            String(r.hAvg10 / 10.0, 1) + ',' + String(r.hMax10 / 10.0, 1) + ',' +
            r.heat + ',' + r.fan + ',' + String(r.vb10 / 10.0, 1) + ',' + r.bat + "\r\n";
    server.sendContent(chunk);
    yield();
  }
  server.sendContent("");   // terminate chunked mode
}

// ---------------------------------------------------------------------
//  POST /api/settings - validate, persist, apply (no JSON library)
// ---------------------------------------------------------------------
static void applyFromBody(const String &b, Settings &s) {
  if (jhas(b, "setTemp"))   s.setTemp   = clampf(jgetnum(b, "setTemp",   s.setTemp),   40, 80);  // v2.0 ceiling
  if (jhas(b, "tempHyst"))  s.tempHyst  = clampf(jgetnum(b, "tempHyst",  s.tempHyst), 0.2,  5);
  if (jhas(b, "humHigh"))   s.humHigh   = clampf(jgetnum(b, "humHigh",   s.humHigh),   20, 95);
  if (jhas(b, "humLow"))    s.humLow    = clampf(jgetnum(b, "humLow",    s.humLow),    10, 80);
  if (jhas(b, "humTarget")) s.humTarget = clampf(jgetnum(b, "humTarget", s.humTarget),  5, 70);
  if (jhas(b, "kp")) s.kp = clampf(jgetnum(b, "kp", s.kp), 0, 100);
  if (jhas(b, "ki")) s.ki = clampf(jgetnum(b, "ki", s.ki), 0,  10);
  if (jhas(b, "kd")) s.kd = clampf(jgetnum(b, "kd", s.kd), 0, 100);
  if (jhas(b, "maxTemp"))
    s.maxTemp = clampf(jgetnum(b, "maxTemp", s.maxTemp), s.setTemp + 5, 110);
  if (jhas(b, "dryMinutes"))
    s.dryMinutes = constrain((uint32_t)jgetnum(b, "dryMinutes", s.dryMinutes), 1U, 1440U);
  if (jhas(b, "fanMin"))      s.fanMin      = constrain((int)jgetnum(b, "fanMin", s.fanMin), 0, 60);
  if (jhas(b, "fanIn"))       s.fanIn       = constrain((int)jgetnum(b, "fanIn", s.fanIn), 10, 100);
  if (jhas(b, "targetG"))     s.targetG     = constrain((float)jgetnum(b, "targetG", s.targetG), 0.0f, 9000.0f);
  if (jhas(b, "stickCount"))    s.stickCount    = (uint16_t)constrain((int)jgetnum(b, "stickCount", s.stickCount), 0, 3000);
  if (jhas(b, "stickWetG"))     s.stickWetG     = constrain((float)jgetnum(b, "stickWetG", s.stickWetG), 0.5f, 20.0f);
  if (jhas(b, "pasteWaterPct")) s.pasteWaterPct = constrain((float)jgetnum(b, "pasteWaterPct", s.pasteWaterPct), 5.0f, 60.0f);
  if (jhas(b, "targetMoistPct"))s.targetMoistPct= constrain((float)jgetnum(b, "targetMoistPct", s.targetMoistPct), 3.0f, 20.0f);
  if (jhas(b, "fanTrigRH"))   s.fanTrigRH   = constrain((int)jgetnum(b, "fanTrigRH", s.fanTrigRH), 30, 90);
  if (jhas(b, "fanTrigMin"))  s.fanTrigMin  = constrain((int)jgetnum(b, "fanTrigMin", s.fanTrigMin), 1, 10);
  if (jhas(b, "fanBurstS"))   s.fanBurstS   = constrain((int)jgetnum(b, "fanBurstS", s.fanBurstS), 10, 300);
  if (jhas(b, "fanOut"))      s.fanOut      = constrain((int)jgetnum(b, "fanOut", s.fanOut), 10, 100);
  if (jhas(b, "fanSlope"))    s.fanSlope    = constrain((int)jgetnum(b, "fanSlope", s.fanSlope), 1, 12);
  if (jhas(b, "heaterMax"))   s.heaterMax   = constrain((int)jgetnum(b, "heaterMax", s.heaterMax), 10, 100);
  if (jhas(b, "cooldownSec")) s.cooldownSec = constrain((int)jgetnum(b, "cooldownSec", s.cooldownSec), 10, 600);
  if (jhas(b, "bypassPct"))   s.bypassPct   = constrain((int)jgetnum(b, "bypassPct", s.bypassPct), 5, 50);
  if (jhas(b, "cutoffPct"))   s.cutoffPct   = constrain((int)jgetnum(b, "cutoffPct", s.cutoffPct), 0, 40);
  if (jhas(b, "battType"))    s.battType    = constrain((int)jgetnum(b, "battType", s.battType), 0, 3);
  if (jhas(b, "tzMinutes"))   s.tzMinutes   = constrain((int)jgetnum(b, "tzMinutes", s.tzMinutes), -720, 840);
  if (jhas(b, "requireHum"))  s.requireHum  = jgetbool(b, "requireHum", s.requireHum);
  if (jhas(b, "smartVent"))   s.smartVent   = jgetbool(b, "smartVent", s.smartVent);
  if (jhas(b, "boostHeat"))   s.boostHeat   = jgetbool(b, "boostHeat", s.boostHeat);
  if (jhas(b, "requireWeight")) s.requireWeight = jgetbool(b, "requireWeight", s.requireWeight);
  if (jhas(b, "weightRateG"))   s.weightRateG   = clampf(jgetnum(b, "weightRateG", s.weightRateG), 0.5, 50);
  if (jhas(b, "weightMinY"))    s.weightMinY    = constrain((int)jgetnum(b, "weightMinY", s.weightMinY), 2, 120);
  if (jhas(b, "scaleCal"))      s.scaleCal      = clampf(jgetnum(b, "scaleCal", s.scaleCal), 0.05, 200000);
  if (s.cutoffPct >= s.bypassPct) s.cutoffPct = s.bypassPct - 1;   // keep order sane
}

static void handleSettings() {
  String body = server.arg("plain");
  if (!body.length()) { server.send(400, "text/plain", "empty body"); return; }
  Settings s = cfg;
  applyFromBody(body, s);
  if (!saveSettings(s)) {
    server.send(500, "text/plain", "nvs write failed");
    return;
  }
  cfg = s;
  dryer.applySettings(cfg);
  scale.setFactor(cfg.scaleCal);               // recalibration round-trips
  scale.setOffset(cfg.scaleOffset);
  server.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  simple actions
// ---------------------------------------------------------------------
static void handleStart() {
  if (!door::calibrated()) {
    server.send(403, "text/plain", "calibrate the scale first (known weight) - start locked");
    return;
  }
  dryer.start();
  server.send(200, "text/plain", "ok");
}
static void handleStop()    { dryer.stop();     server.send(200, "text/plain", "ok"); }
static void handlePower()   { dryer.powerOn();  server.send(200, "text/plain", "ok"); }
static void handleDefaults(){
  cfg = defaultSettings();
  saveSettings(cfg);
  dryer.applySettings(cfg);
  server.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  clock: /api/settime (browser pushes its date & time automatically)
// ---------------------------------------------------------------------
static void handleSetTime() {
  if (!server.hasArg("epoch")) { server.send(400, "text/plain", "epoch?"); return; }
  struct timeval tv;
  tv.tv_sec = (time_t)server.arg("epoch").toInt();
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  if (tv.tv_sec > (time_t)1700000000) {
    sClockSynced = true;                       // a real phone clock arrived
    clockSave((uint32_t)tv.tv_sec);
    clockToRtc();
  }
  bool tzChanged = false;
  if (server.hasArg("tz")) {
    int tz = constrain(server.arg("tz").toInt(), -720, 840);
    if (tz != cfg.tzMinutes) {
      cfg.tzMinutes = (int16_t)tz;
      saveSettings(cfg);
      tzChanged = true;
    }
  }
  cyclelog::applyTz();
  server.send(200, "text/plain", tzChanged ? "ok+tz" : "ok");
}

// ---------------------------------------------------------------------
//  POST /api/weather - the phone's browser relays live outdoor weather
// ---------------------------------------------------------------------
static void handleWeather() {
  String b = server.arg("plain");
  if (!b.length()) { server.send(400, "text/plain", "empty body"); return; }
  if (jhas(b, "t")) weather.tempC   = clampf(jgetnum(b, "t", 0), -60, 70);
  if (jhas(b, "h")) weather.humRH   = clampf(jgetnum(b, "h", 0), 0, 100);
  if (jhas(b, "r")) weather.rainPct = clampf(jgetnum(b, "r", 0), 0, 100);
  if (jhas(b, "w")) weather.windKmh = clampf(jgetnum(b, "w", 0), 0, 200);
  if (jhas(b, "c")) weather.code    = constrain((int)jgetnum(b, "c", 100), 0, 100);
  if (jhas(b, "ep")) weather.epoch  = (uint32_t)jgetnum(b, "ep", 0);
  if (jhas(b, "m"))  weather.manual = jgetbool(b, "m", false);
  if (jhas(b, "loc")) {
    strncpy(weather.loc, jgetstr(b, "loc").c_str(), sizeof(weather.loc) - 1);
    weather.loc[sizeof(weather.loc) - 1] = 0;
  }
  weather.rxMs = millis();
  Serial.printf("[wx] %.1fC %.0f%%RH rain %.0f%% (%s)\n",
                weather.tempC, weather.humRH, weather.rainPct,
                weather.manual ? "manual" : "live");
  server.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  cycle history: list / download / clear
// ---------------------------------------------------------------------
// ---- AT24C256 registry dump: every cycle summary, one CSV ------------
static const char *eeEndName(uint8_t e) {
  switch (e) { case 1: return "stopped"; case 2: return "fault";
               case 3: return "timeout";  case 4: return "interrupted";
               default: return "done"; }
}
static void handleEeLog() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  server.sendContent(F("seq,started,dur_min,mode,end,ecode,setT_C,tavg_C,tmax_C,"
                       "hmax_RH,outT_C,wt_start_g,wt_end_g,wt_target_g,"
                       "vb_start,vb_end,flags\r\n"));
  char line[192];
  for (uint16_t i = 0; i < eelog::count(); i++) {
    eelog::EeRec r;
    if (!eelog::get(i, r)) break;
    char when[24] = "--";
    time_t t = (time_t)r.startEpoch;
    if (r.startEpoch > 1000) {
      struct tm tmv;  localtime_r(&t, &tmv);
      strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &tmv);
    }
    char ec[6] = "-";
    if (r.ecode) snprintf(ec, sizeof(ec), "E%02u", r.ecode);
    snprintf(line, sizeof(line),
      "%u,%s,%.1f,%s,%s,%s,%.1f,%.1f,%.1f,%.0f,%.1f,%.0f,%.0f,%.0f,%.1f,%.1f,%u\r\n",
      (unsigned)r.seq, when, r.durS / 60.0f,
      r.mode == 0 ? "agarbatti" : r.mode == 2 ? "silica" : "user",
      eeEndName(r.endR), ec,
      r.setT10 / 10.0f, r.tAvg10 / 10.0f, r.tMax10 / 10.0f,
      r.hMax10 / 10.0f, r.outT10 / 10.0f,
      (float)r.wtS10 * 10.0f, (float)r.wtE10 * 10.0f, (float)r.wtT10 * 10.0f,
      r.vbS10 / 10.0f, r.vbE10 / 10.0f, r.flags);
    server.sendContent(line);
    if ((i & 0x1F) == 0) yield();
  }
  server.sendContent("");              // terminate the chunked body
}

static void handleCycles() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", cyclelog::listingJson());
}

static void handleCycleDownload() {
  if (!server.hasArg("file")) { server.send(400, "text/plain", "file?"); return; }
  String path = cyclelog::safePath(server.arg("file"));
  if (!path.length()) { server.send(404, "text/plain", "no such cycle"); return; }
  File f = LittleFS.open(path, "r");
  if (!f) { server.send(404, "text/plain", "open failed"); return; }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Disposition",
                    "attachment; filename=" + server.arg("file"));
  server.streamFile(f, "text/csv");
  f.close();
}

static void handleLastCycle() {
  String fn = cyclelog::lastFile();
  if (!fn.length()) { server.send(404, "text/plain", "no saved cycles yet"); return; }
  File f = LittleFS.open(cyclelog::safePath(fn), "r");
  if (!f) { server.send(404, "text/plain", "open failed"); return; }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Disposition", "attachment; filename=" + fn);
  server.streamFile(f, "text/csv");
  f.close();
}

// ---- v2.0.19: virtual keypad key (touch display / web) ----------------
static void handleVirtualKey() {
  if (!server.hasArg("k") || server.arg("k").length() != 1) {
    server.send(400, "text/plain", "k?"); return;
  }
  char k = server.arg("k")[0];
  if (k >= 'a' && k <= 'd') k = k - 32;               // accept lowercase
  bool ok = (k >= '0' && k <= '9') || (k >= 'A' && k <= 'D') ||
            k == '*' || k == '#';
  if (!ok) { server.send(400, "text/plain", "bad key"); return; }
  dryerKey(k);      // beep + menu + start/stop/mode - exactly the panel path
  server.send(200, "text/plain", "ok");
}

static void handleCyclesClear() {
  cyclelog::clear();
  server.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  OTA firmware update - upload a .bin from the browser (phone/laptop
//  connected to the dryer hotspot) at http://192.168.4.1/update
// ---------------------------------------------------------------------
static const char kOtaPage[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Firmware update</title><style>
body{background:#070b14;color:#eef3fb;font:15px/1.6 system-ui,sans-serif;max-width:520px;margin:40px auto;padding:0 18px}
h1{font-size:19px}p{color:#a9b6c9;font-size:13.5px}
.card{background:#0d1526;border:1px solid #26365a;border-radius:14px;padding:18px;box-shadow:0 10px 30px rgba(0,0,0,.45)}
input[type=file]{width:100%;margin:10px 0;color:#a9b6c9}
button{width:100%;padding:13px;border:none;border-radius:12px;font-weight:750;cursor:pointer;
background:linear-gradient(135deg,#3b82f6,#1d4ed8);color:#fff}
#bar{height:12px;border-radius:7px;background:#101a33;border:1px solid #26365a;margin-top:12px;overflow:hidden}
#fill{height:100%;width:0%;background:linear-gradient(90deg,#d4af37,#f0d078)}
#msg{margin-top:10px;font-size:13px;color:#d4af37;min-height:20px}
</style></head><body>
<h1>&#11014; Firmware update</h1>
<div class="card">
<p>1. Export the compiled <b>.bin</b> (Arduino IDE: Sketch &rarr; Export compiled binary).<br>
2. Pick it below and press Update. The dryer reboots itself when done.<br>
Refused while a cycle is RUNNING &mdash; stop the cycle first.</p>
<input type="file" id="f" accept=".bin">
<button onclick="up()">&#128228; Update firmware</button>
<div id="bar"><div id="fill"></div></div><div id="msg"></div>
</div>
<script>
function up(){var f=document.getElementById('f').files[0];if(!f){alert('pick a .bin first');return}
var x=new XMLHttpRequest(),fd=new FormData();fd.append('update',f,f.name);
x.open('POST','/update');
x.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);
document.getElementById('fill').style.width=p+'%';document.getElementById('msg').textContent=p+' %'}};
x.onload=function(){document.getElementById('msg').textContent='done: '+x.responseText;
setTimeout(function(){location.href='/'},4000)};
x.send(fd)}
</script></body></html>)HTML";

static bool otaRefuse = false;

static void handleOtaGet() {
  server.send_P(200, "text/html", kOtaPage);
}

static void handleOtaUpload() {          // called chunk-by-chunk
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaRefuse = (dryer.state() == DState::RUNNING);
    if (otaRefuse) { Serial.println("[ota] REFUSED: cycle RUNNING - stop it first"); return; }
    Serial.printf("[ota] start: %s\n", up.filename.c_str());
    uint32_t maxSketch = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketch)) { Update.printError(Serial); otaRefuse = true; }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (otaRefuse) return;
    if (Update.write((uint8_t *)up.buf, up.currentSize) != up.currentSize) {
      Update.printError(Serial); otaRefuse = true;
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (otaRefuse) return;
    if (Update.end(true)) {
      Serial.printf("[ota] SUCCESS: %u bytes written - rebooting\n", (unsigned)up.totalSize);
    } else { Update.printError(Serial); otaRefuse = true; }
  }
}

static void handleOtaDone() {            // after the upload finished
  if (otaRefuse) {
    server.send(403, "text/plain",
                Update.hasError() ? "write FAILED - power is fine, try again"
                                  : "refused: cycle RUNNING - stop it first");
    return;
  }
  server.send(200, "text/plain", "OK - rebooting, reconnect in ~15 s");
  delay(800);                             // let the response reach the browser
  ESP.restart();
}

static void handleAddTime() {
  int m = server.hasArg("min") ? server.arg("min").toInt() : 15;
  dryer.addMinutes(m);
  server.send(200, "text/plain", "ok");
}

static void handleScale() {        // /api/scale?tare=1  or  /api/scale?cal=1000
  if (!scale.ok()) { server.send(503, "text/plain", "scale absent"); return; }
  if (server.hasArg("tare")) {
    scale.tare();
  } else if (server.hasArg("cal")) {
    float known = server.arg("cal").toFloat();
    if (known <= 0) { server.send(400, "text/plain", "cal?"); return; }
    scale.calibrate(known);
    door::markCalibrated();                    // unlock the workflow
  } else { server.send(400, "text/plain", "tare or cal"); return; }
  cfg.scaleCal   = scale.calFactor();          // persist for next boots
  cfg.scaleOffset = scale.offset();
  saveSettings(cfg);
  server.send(200, "text/plain", "ok");
}

static void handleManualHeat() {   // web knob: /api/heat?d=0..100
  if (!server.hasArg("d")) { server.send(400, "text/plain", "d?"); return; }
  int d = server.arg("d").toInt();
  if (d < 0) d = 0;
  if (d > 100) d = 100;
  dryer.setManualHeat((uint8_t)d);
  server.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  setup
// ---------------------------------------------------------------------
namespace web {

void clockBoot() {           // restore last-saved time after power-down
  time_t now = time(nullptr);
  if (now > (time_t)1700000000) return;        // already running (soft reset)
  Preferences p;
  p.begin("dryer", true);
  uint32_t ep = p.getUInt("tsep", 0);
  p.end();
  if (ep > 1700000000UL) {
    struct timeval tv;
    tv.tv_sec = (time_t)ep; tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    Serial.printf("[clock] restored last-saved time (%lu) - STALE, "
                  "open the site once to correct it\n", (unsigned long)ep);
  } else {
    Serial.println(F("[clock] no saved time yet - history files use "
                     "sequence numbers until a phone syncs"));
  }
}

void clockTick() {            // periodic persistence (call from loop/handle)
  static uint32_t last = 0;
  uint32_t now = millis();
  if (last != 0 && now - last < TIME_SAVE_MS) return;
  last = now ? now : 1;
  time_t t = time(nullptr);
  if (sClockSynced && t > (time_t)1700000000) clockSave((uint32_t)t);
}

void begin() {
  clockBoot();               // last-saved wall clock (if the battery died)

  // --- hotspot ---------------------------------------------------------
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, 0, AP_MAX_CLIENTS);
  IPAddress ip = WiFi.softAPIP();          // 192.168.4.1
  Serial.printf("[web] AP '%s' up -> http://%s\n", AP_SSID, ip.toString().c_str());

  // --- captive DNS: any name resolves to us, phone pops the portal ------
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", ip);

  server.on("/",            HTTP_GET,  handleRoot);
  server.on("/display",     HTTP_GET,  handleDisplay);   // kiosk screen
  server.on("/online",      HTTP_GET,  handleOnline);
  server.on("/api/data",    HTTP_GET,  handleData);
  server.on("/api/history", HTTP_GET,  handleHistory);
  server.on("/api/log.csv", HTTP_GET,  handleCsv);
  server.on("/api/settings",HTTP_POST, handleSettings);
  server.on("/api/start",   HTTP_POST, handleStart);
  server.on("/api/stop",    HTTP_POST, handleStop);
  server.on("/api/power",   HTTP_POST, handlePower);
  server.on("/api/defaults",HTTP_POST, handleDefaults);
  server.on("/api/addtime",  HTTP_POST, handleAddTime);
  server.on("/api/heat",     HTTP_POST, handleManualHeat);
  server.on("/api/scale",    HTTP_POST, handleScale);
  server.on("/api/settime",     HTTP_POST, handleSetTime);
  server.on("/api/mode",        HTTP_POST, []() {
    if (!server.hasArg("m")) { server.send(400, "text/plain", "m?"); return; }
    dryer.applyMode(constrain(server.arg("m").toInt(), 0, 2));
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/weather",     HTTP_POST, handleWeather);
  server.on("/api/cycles",      HTTP_GET,  handleCycles);
  server.on("/eelog.csv",       HTTP_GET,  handleEeLog);
  server.on("/lastcycle.csv",   HTTP_GET,  handleLastCycle);
  server.on("/api/key",         HTTP_POST, handleVirtualKey);
  server.on("/api/cycle",       HTTP_GET,  handleCycleDownload);
  server.on("/api/clearcycles", HTTP_POST, handleCyclesClear);
  server.on("/update", HTTP_GET,  handleOtaGet);
  server.on("/update", HTTP_POST, handleOtaDone, handleOtaUpload);
  server.onNotFound([]() {                 // captive portal redirect
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();
  bz::play(BP::AP_UP);                     // #27: hotspot is up (owner spec)
}

void handle() {
  clockTick();               // persist wall clock (30 min)
  dns.processNextRequest();
  server.handleClient();
}

} // namespace web
