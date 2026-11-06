// Host test for arduino-ide/esp32_T1/esp32_T1.ino - the minimal ESP32 Dev +
// 2x DHT22 + BTS7960 (via 555) dryer. The REAL sketch runs against mock/
// (bit-exact DHT22 emulators, LEDC capture, web-server hooks, NVS) coupled
// to a lumped thermal model of the 500 W chamber. Build + run: run.sh.
//   harness all    every scenario       harness save / load   NVS reboot test
// Prints "JSON {...}" lines (validate.py parses them), "FAIL ..." on errors.
#include "Arduino.h"
#include "WiFi.h"
#include "WebServer.h"
#include "Preferences.h"
#include <deque>
#include <map>
#include <vector>

uint64_t gUs = 0;
int gCritical = 0, gCritViolations = 0;
uint64_t gCritStart = 0, gCritMaxUs = 0;
DhtEmu gDht[64];
int gMode[64], gOut[64];
bool gDhtJump = true;
uint32_t gDuty[64], gLedcFreq = 0;
int gLedcBits = 0, gLedcPin = -1, gChPin[16];
bool gQuiet = true;
SerialMock Serial;
WiFiMock WiFi;

#include SKETCH

// ---------------------------------------------------------------- checks
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL  "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NOTE(...) do { printf("OK    "); printf(__VA_ARGS__); printf("\n"); } while (0)

// ------------------------------------------------- 500 W chamber model
struct Plant {
  double C = 12000;            // J/K  racks + sticks + walls
  double k = 5;                // W/K  losses (500 W alone would hold Ta+100 C)
  double Ta = 30;              // ambient, C
  double tauS = 30;            // s    DHT22 lag in still chamber air
  double dead = 10;            // s    transport delay coil -> sensors
  double off1 = 1.0, off2 = -1.0;   // the two sensor spots
  double extraW = 0;           // outside heat (fault injection)
  bool heaterWorks = true;     // false = dead coil / fuse / EN wire
  double T = 30, s1 = 30, s2 = 30, now = 0;
  std::deque<std::pair<double, double>> hist;
  void reset(double T0) { T = s1 = s2 = T0; now = 0; hist.clear(); }
  void step(double dt, double heatW) {
    now += dt;
    T += dt * ((heaterWorks ? heatW : 0) + extraW - k * (T - Ta)) / C;
    hist.push_back({now, T});
    while (hist.size() > 1 && hist[1].first <= now - dead) hist.pop_front();
    double Td = hist.front().second, a = dt / (tauS + dt);
    s1 += a * (Td + off1 - s1);
    s2 += a * (Td + off2 - s2);
  }
  double rh(double s) const { double v = 85 - 1.1 * (s - Ta); return v < 8 ? 8 : v > 95 ? 95 : v; }
};
static Plant P;
static uint32_t lcg = 12345;
static double noise() {                          // +-0.1 C, reproducible
  lcg = lcg * 1103515245u + 12345u;
  return ((int)((lcg >> 16) % 201) - 100) / 1000.0;
}

static void syncSensors() {
  gDht[PIN_DHT1].t = P.s1 + noise();  gDht[PIN_DHT1].h = P.rh(P.s1);
  gDht[PIN_DHT2].t = P.s2 + noise();  gDht[PIN_DHT2].h = P.rh(P.s2) + 1.5;
}
static double heaterW() {
  bool en = gOut[PIN_BTS_EN] == HIGH;
  return en ? 500.0 * gDuty[PIN_BTS_RPWM] / 1023.0 : 0.0;
}

static int invBad = 0;
static void invariants() {
  bool en = gOut[PIN_BTS_EN] == HIGH;
  uint32_t d = gDuty[PIN_BTS_RPWM];
  if ((!en && d) || (en && !d)) invBad++;                 // EN and PWM agree
  if (state != RUNNING && (en || d)) invBad++;             // heat only when RUNNING
  if (d > 1023 || (en && (uint32_t)heatPct * 1023 / 100 != d)) invBad++;
}

static void tick() {
  uint64_t t0 = gUs;
  loop();                       // loop() itself spends 1 ms (delay)
  gUs += 49000;                 // -> one iteration = 50 ms of real time
  P.step((gUs - t0) / 1e6, heaterW());
  syncSensors();
  invariants();
}
static void run(double sec, const std::function<bool()> &each = {}) {
  uint64_t end = gUs + (uint64_t)(sec * 1e6);
  while (gUs < end) { tick(); if (each && !each()) break; }
}

// ------------------------------------------------------------- web
static int post(const char *path, std::map<std::string, std::string> a = {}) {
  return server.request(HTTP_POST, path, a);
}
static std::string api() {
  server.request(HTTP_GET, "/api");
  printf("JSON %s\n", server.lastBody.c_str());
  return server.lastBody;
}
static double jnum(const std::string &j, const char *k) {
  std::string key = std::string("\"") + k + "\":";
  size_t p = j.find(key);
  if (p == std::string::npos) return -9999;
  p += key.size();
  if (j.compare(p, 4, "null") == 0) return NAN;
  return strtod(j.c_str() + p, nullptr);
}
static std::string jstr(const std::string &j, const char *k) {
  std::string key = std::string("\"") + k + "\":\"";
  size_t p = j.find(key);
  if (p == std::string::npos) return "<missing>";
  p += key.size();
  return j.substr(p, j.find('"', p) - p);
}
static bool has(const std::string &s, const char *x) { return s.find(x) != std::string::npos; }

static void fresh(double T0 = 30) {        // idle, plant at T0, sensors healthy
  post("/stop");
  P = Plant();
  P.reset(T0);
  for (int pin : {PIN_DHT1, PIN_DHT2}) {
    DhtEmu &e = gDht[pin];
    e.present = true; e.corrupt = false; e.tgo = 30; e.zeroUs = 27; e.oneUs = 70;
  }
  syncSensors();
  run(8);
}
static double simMin(uint64_t since) { return (gUs - since) / 60e6; }

// ======================================================== scenarios
static void tBoot() {
  CHECK(gMode[PIN_BTS_EN] == OUTPUT && gOut[PIN_BTS_EN] == LOW, "boot: EN must be an output held LOW");
  CHECK(gLedcPin == PIN_BTS_RPWM && gLedcFreq == 1000 && gLedcBits == 10,
        "boot: RPWM = GPIO25, 1 kHz, 10 bit (got GPIO%d %u Hz %d bit)", gLedcPin, gLedcFreq, gLedcBits);
  CHECK(gDuty[PIN_BTS_RPWM] == 0 && heatPct == 0 && state == IDLE, "boot: heater off, IDLE");
  CHECK(gMode[PIN_DHT1] == (OUTPUT_OPEN_DRAIN | PULLUP) && gOut[PIN_DHT1] == HIGH &&
        gMode[PIN_DHT2] == (OUTPUT_OPEN_DRAIN | PULLUP) && gOut[PIN_DHT2] == HIGH,
        "boot: DHT lines open-drain, released (HIGH)");
  CHECK(WiFi.ssid == "AgarbattiDryer" && WiFi.pass == "dryer1234" && server.begun,
        "boot: hotspot AgarbattiDryer/dryer1234 + web server");
  std::string j = api();
  CHECK(jstr(j, "state") == "IDLE" && isnan(jnum(j, "t1")) && jnum(j, "heat") == 0,
        "boot: no reading before the first DHT22 read (sensor warm-up)");
  run(10);
  j = api();
  CHECK(fabs(jnum(j, "t1") - P.s1) < 0.25 && fabs(jnum(j, "t2") - P.s2) < 0.25 &&
        fabs(jnum(j, "t") - (jnum(j, "t1") + jnum(j, "t2")) / 2) < 0.06,
        "idle: both DHT22 read, t = their average (%s)", j.c_str());
  CHECK(jnum(j, "set") == 60 && jnum(j, "min") == 120 && jstr(j, "msg").empty(),
        "idle: defaults 60 C / 120 min, no message");

  server.request(HTTP_GET, "/");
  const std::string &pg = server.lastBody;
  CHECK(server.lastCode == 200 && server.lastType == "text/html" &&
        has(pg, "<title>Agarbatti Dryer</title>") && has(pg, "fetch('api'"),
        "GET / serves the page (relative api URL)");
  CHECK(has(pg, "#2352c9") && has(pg, "#c4914a") && !has(pg, "#000") && !has(pg, "black"),
        "page: royal blue + golden brown backdrop, no black");
  CHECK(server.request(HTTP_GET, "/start") == 404 && state == IDLE, "GET /start must not start (POST only)");
  CHECK(server.request(HTTP_GET, "/nope") == 404, "unknown path -> 404");
  NOTE("boot: heater off, pins safe, hotspot + page up, sensors read");
}

static void tCycle() {
  fresh(30);
  int c = post("/start", {{"set", "60"}, {"min", "120"}});
  CHECK(c == 200 && server.lastBody == "Started", "start -> 200 Started (got %d %s)", c, server.lastBody.c_str());
  CHECK(state == RUNNING && heatPct == 100 && gDuty[PIN_BTS_RPWM] == 1023 && gOut[PIN_BTS_EN] == HIGH,
        "start: full power at once");
  std::string j0 = api();
  CHECK(jstr(j0, "state") == "RUNNING" && jstr(j0, "phase") == "HEAT-UP" && jnum(j0, "heat") == 100 &&
        jnum(j0, "left") == 7200, "start: JSON shows HEAT-UP at 100 %% (%s)", j0.c_str());
  uint64_t t0 = gUs;
  double reach = -1, peak = 0, lo = 1e9, hi = -1e9, sum = 0;
  long n = 0;
  run(120 * 60 - 3, [&] {
    double t = avgT(), m = simMin(t0);
    if (reach < 0 && t >= 58.5) reach = m;
    if (reach >= 0 && t > peak) peak = t;
    if (m >= 90) { lo = std::min(lo, t); hi = std::max(hi, t); sum += t; n++; }
    return true;
  });
  double mean = n ? sum / n : 0;
  std::string j = api();
  CHECK(jstr(j, "state") == "RUNNING" && jstr(j, "phase") == "HOLD", "cycle: holding near the end");
  CHECK(reach > 0 && reach < 30, "cycle: 58.5 C reached in %.1f min (< 30)", reach);
  CHECK(peak - 60 <= 3.5, "cycle: overshoot %.2f C (<= 3.5)", peak - 60);
  CHECK(fabs(mean - 60) <= 1.0 && hi - lo <= 2.5, "cycle: last 30 min mean %.2f C, swing %.2f C", mean, hi - lo);
  run(8);
  j = api();
  CHECK(state == DONE && heatPct == 0 && jnum(j, "left") == 0 && jnum(j, "el") == 7200 &&
        has(jstr(j, "msg"), "Done - 120 min"), "cycle: DONE after 120 min, heater off (%s)", j.c_str());
  run(300);
  CHECK(state == DONE && heatPct == 0, "cycle: stays off after DONE");
  NOTE("cycle 60 C / 120 min: 58.5 C at %.1f min, peak %.1f C, last 30 min %.2f C (swing %.2f), DONE",
       reach, peak, mean, hi - lo);
}

static void tGrid() {
  int cases = 0, bad = 0;
  double worstOver = 0, worstErr = 0, worstSwing = 0, worstHot = 0, worstEarly = 0;
  for (double C : {5000.0, 12000.0, 30000.0})
  for (double k : {3.0, 6.0})
  for (double tauS : {10.0, 45.0})
  for (double dead : {0.0, 20.0})
  for (int set : {50, 65, 80}) {
    fresh(30);
    P.C = C; P.k = k; P.tauS = tauS; P.dead = dead;
    post("/start", {{"set", std::to_string(set)}, {"min", "300"}});
    uint64_t t0 = gUs;
    double peak = 0, hot = 0, lo = 1e9, hi = -1e9, sum = 0, early = 0;
    long n = 0, ne = 0;
    bool reached = false;
    uint64_t reachUs = 0;
    run(180 * 60, [&] {
      double t = avgT();
      if (!reached && t >= set - BAND_C) { reached = true; reachUs = gUs; }
      if (reached && simMin(reachUs) >= 10 && simMin(reachUs) < 40) { early += fabs(t - set); ne++; }
      if (reached) peak = std::max(peak, t);
      hot = std::max(hot, (double)maxT());
      if (simMin(t0) >= 140) { lo = std::min(lo, t); hi = std::max(hi, t); sum += t; n++; }
      return state == RUNNING;
    });
    double mean = n ? sum / n : 0;
    early = ne ? early / ne : 99;
    bool ok = state == RUNNING && hot < HARD_CUT_C && fabs(mean - set) <= 1.5 && hi - lo <= 4.0 && early <= 2.0;
    cases++;
    if (!ok) {
      bad++;
      CHECK(ok, "grid C=%.0f k=%.0f tau=%.0f dead=%.0f set=%d: state=%s msg='%s' peak=%.1f hot=%.1f mean=%.2f swing=%.2f early=%.2f",
            C, k, tauS, dead, set, STATE_NAME[state], msg, peak, hot, mean, hi - lo, early);
    }
    worstOver = std::max(worstOver, peak - set);
    worstErr = std::max(worstErr, fabs(mean - set));
    worstSwing = std::max(worstSwing, hi - lo);
    worstHot = std::max(worstHot, hot);
    worstEarly = std::max(worstEarly, early);
  }
  post("/stop");
  NOTE("grid %d plants x targets: %d bad | worst: overshoot %.1f C, error 10-40 min after reaching %.2f C, "
       "final error %.2f C, swing %.2f C, hottest %.1f C", cases, bad, worstOver, worstEarly, worstErr, worstSwing, worstHot);
}

static void tHardCut() {
  fresh(30);
  post("/start", {{"set", "80"}, {"min", "600"}});
  run(60 * 60, [] { return !holding; });
  run(10 * 60);
  CHECK(state == RUNNING && holding, "hard cut: holding 80 C first");
  P.extraW = 4000;                               // runaway: outside heat
  double atCut = 0;
  run(30 * 60, [&] { if (state != RUNNING) { atCut = maxT(); return false; } return true; });
  std::string j = api();
  CHECK(state == FAULT && heatPct == 0 && has(jstr(j, "msg"), "Over-temperature"),
        "hard cut: FAULT + heater off (%s)", j.c_str());
  CHECK(atCut >= HARD_CUT_C && atCut < HARD_CUT_C + 1.5, "hard cut: tripped at %.1f C", atCut);
  int c = post("/start");
  CHECK(c == 409 && has(server.lastBody, "Too hot"), "hard cut: Start refused while hot (%d %s)",
        c, server.lastBody.c_str());
  P.extraW = 0;
  run(3 * 3600, [] { return maxT() >= START_BELOW_C - 1.0f; });   // clear of the noise at 85.0
  run(5);
  c = post("/start");
  CHECK(c == 200 && state == RUNNING && msg[0] == 0, "hard cut: Start works again once below 85 C "
        "(%d '%s' state=%s msg='%s' hottest=%.1f)", c, server.lastBody.c_str(), STATE_NAME[state], msg, (double)maxT());
  post("/stop");
  NOTE("hard cut: tripped at %.1f C, latched, restart blocked until < 85 C", atCut);
}

static void tSensors() {
  fresh(30);
  gDht[PIN_DHT2].present = false;                // #2 unplugged
  run(10);
  std::string j = api();
  CHECK(isnan(jnum(j, "t2")) && !isnan(jnum(j, "t1")) && jstr(j, "msg").empty(),
        "one sensor: t2 null, idle without a warning (%s)", j.c_str());
  CHECK(post("/start", {{"set", "60"}, {"min", "300"}}) == 200, "one sensor: Start allowed");
  run(60);
  CHECK(has(msg, "DHT22 #2 offline - running on #1 only"), "one sensor: warning shown ('%s')", msg);
  j = api();
  CHECK(isnan(jnum(j, "t2")) && jnum(j, "t") == jnum(j, "t1"), "one sensor: JSON t = t1, t2 null");
  run(60 * 60);
  CHECK(state == RUNNING && fabs(avgT() - 60) < 2.0 && avgT() == dht1.t,
        "one sensor: holds 60 C on #1 alone (%.1f)", (double)avgT());

  gDht[PIN_DHT1].present = false;                // #1 gone too
  uint64_t gone = 0, faultAt = 0;
  bool heatedBlind = false, pausedMsg = false;
  run(60, [&] {
    if (!dht1.ok && !gone) gone = gUs;
    if (gone && heatPct) heatedBlind = true;
    if (gone && has(msg, "heater paused") && !pausedMsg) { pausedMsg = true; api(); }
    if (state == FAULT) { faultAt = gUs; return false; }
    return true;
  });
  double after = gone ? (faultAt - gone) / 1e6 : -1;
  CHECK(gone && !heatedBlind && pausedMsg, "both offline: heater off at once, 'paused' message");
  CHECK(state == FAULT && has(msg, "Both DHT22 offline") && after >= 15.0 && after <= 17.6,
        "both offline: FAULT %.1f s later (15-17.5)", after);
  CHECK(post("/start") == 409 && has(server.lastBody, "No DHT22 responding"), "no sensor: Start refused");
  post("/stop");
  run(5);
  CHECK(has(msg, "No DHT22 responding"), "no sensor: idle hint ('%s')", msg);
  gDht[PIN_DHT1].present = gDht[PIN_DHT2].present = true;
  run(8);
  CHECK(post("/start") == 200 && msg[0] == 0, "sensors back: Start works");
  post("/stop");
  NOTE("sensors: one missing = warning + keeps holding; both missing = heater off, FAULT after %.1f s", after);
}

static void tWatchdog() {
  fresh(30);
  P.heaterWorks = false;                         // dead coil from the start
  post("/start", {{"set", "60"}, {"min", "300"}});
  uint64_t t0 = gUs;
  run(15 * 60, [] { return state == RUNNING; });
  double at = (gUs - t0) / 1e6;
  CHECK(state == FAULT && has(msg, "No temperature rise") && at >= 180 && at <= 190,
        "watchdog: dead heater -> FAULT at %.0f s (180-190) '%s'", at, msg);

  fresh(30);
  post("/start", {{"set", "60"}, {"min", "300"}});
  run(6 * 60);
  CHECK(state == RUNNING && !holding, "watchdog: still heating up after 6 min");
  P.heaterWorks = false;                         // coil dies mid heat-up
  uint64_t died = gUs;
  run(20 * 60, [] { return state == RUNNING; });
  double after = (gUs - died) / 1e6;
  CHECK(state == FAULT && has(msg, "No temperature rise") && after <= 370,
        "watchdog: coil dies mid heat-up -> FAULT %.0f s later (<= 370)", after);
  post("/stop");
  NOTE("watchdog: dead heater caught at %.0f s; mid-heat-up failure caught %.0f s later", at, after);
}

static void tProtocol() {
  gDhtJump = false;                              // dense polling, 1 us per read
  DhtEmu &e = gDht[PIN_DHT1];
  e.present = true; e.corrupt = false; e.t = 23.4; e.h = 56.7;
  int good = 0, total = 0;
  for (int tgo : {20, 30, 45, 60, 100, 150, 200})
    for (int zero : {22, 27, 30})
      for (int one : {65, 70, 75}) {
        e.tgo = tgo; e.zeroUs = zero; e.oneUs = one;
        float t = 0, h = 0;
        bool ok = dhtFrame(PIN_DHT1, t, h);
        total++;
        if (ok && fabs(t - 23.4) < 0.051 && fabs(h - 56.7) < 0.051) good++;
        else CHECK(false, "protocol: response %d us, bits %d/%d us -> %s %.1f %.1f", tgo, zero, one,
                   ok ? "wrong" : "failed", t, h);
        gUs += 2000000;
      }
  e.tgo = 30; e.zeroUs = 27; e.oneUs = 70;
  float t, h;
  e.t = -5.3; e.h = 99.9;
  CHECK(dhtFrame(PIN_DHT1, t, h) && fabs(t + 5.3) < 0.051 && fabs(h - 99.9) < 0.051, "protocol: -5.3 C / 99.9 %%");
  e.t = 79.9; e.h = 4.2;
  CHECK(dhtFrame(PIN_DHT1, t, h) && fabs(t - 79.9) < 0.051 && fabs(h - 4.2) < 0.051, "protocol: 79.9 C / 4.2 %%");
  e.corrupt = true;
  CHECK(!dhtFrame(PIN_DHT1, t, h), "protocol: bad checksum rejected");
  e.corrupt = false; e.present = false;
  uint64_t t0 = gUs;
  CHECK(!dhtFrame(PIN_DHT1, t, h) && gUs - t0 < 2400, "protocol: unplugged sensor fails fast (%llu us)",
        (unsigned long long)(gUs - t0));
  e.present = true;
  CHECK(gCritViolations == 0 && gCritMaxUs < 6000, "protocol: critical section %llu us, %d violations",
        (unsigned long long)gCritMaxUs, gCritViolations);
  gDhtJump = true;
  NOTE("protocol: %d/%d frames decoded (response 20-200 us, bit widths 22-30 / 65-75 us), "
       "critical section %llu us", good, total, (unsigned long long)gCritMaxUs);
}

static void tMillisWrap() {                // a cycle across the 49.7-day wrap
  fresh(30);
  uint64_t wrapAt = (1ULL << 32) * 1000ULL;  // millis() rolls over here
  gUs = wrapAt - 90 * 1000000ULL;            // 90 s before it
  run(8);
  post("/start", {{"set", "55"}, {"min", "5"}});
  uint64_t t0 = gUs;
  run(6 * 60, [] { return state == RUNNING; });
  double took = (gUs - t0) / 60e6;
  CHECK(gUs > wrapAt && state == DONE && took > 4.95 && took < 5.1,
        "millis wrap: 5-min cycle across the rollover ended after %.2f min (%s)", took, STATE_NAME[state]);
  NOTE("millis() rollover (49.7 days): timer and control unaffected");
}

static void tSettings() {
  fresh(30);
  int w0 = Preferences::writes();
  CHECK(post("/set", {{"set", "72"}, {"min", "45"}}) == 200 && server.lastBody == "Saved", "settings: /set -> Saved");
  std::string j = api();
  CHECK(jnum(j, "set") == 72 && jnum(j, "min") == 45 && Preferences::writes() == w0 + 2,
        "settings: 72 C / 45 min saved (2 writes)");
  post("/set", {{"set", "72"}, {"min", "45"}});
  CHECK(Preferences::writes() == w0 + 2, "settings: unchanged values are not rewritten (flash wear)");
  post("/set", {{"set", "200"}, {"min", "5000"}});
  CHECK(setC == SET_MAX_C && minutes == MAX_MINUTES, "settings: clamped high -> 80 C / 1440 min");
  post("/set", {{"set", "abc"}, {"min", ""}});
  CHECK(setC == SET_MAX_C && minutes == MAX_MINUTES, "settings: junk / empty ignored");
  post("/set", {{"set", "10"}, {"min", "-5"}});
  CHECK(setC == SET_MIN_C && minutes == MAX_MINUTES, "settings: 10 -> 40 C, negative time ignored");
  post("/set", {{"set", "60"}, {"min", "120"}});
  NOTE("settings: saved only on change, clamped 40-80 C / 1-1440 min, junk ignored");
}

static void tLiveUpdate() {
  fresh(30);
  post("/start", {{"set", "60"}, {"min", "300"}});
  run(40 * 60);
  CHECK(holding && fabs(avgT() - 60) < 1.5, "update: holding 60 C");
  uint32_t el0 = elapsedS;
  int c = post("/start", {{"set", "70"}});
  CHECK(c == 200 && has(server.lastBody, "Already running") && setC == 70, "update: Start while running = save");
  run(5);
  CHECK(elapsedS >= el0 && !holding && heatPct == 100, "update: timer kept, +10 C -> full power again");
  run(40 * 60);
  CHECK(state == RUNNING && fabs(avgT() - 70) < 1.5, "update: now holding 70 C (%.1f)", (double)avgT());
  post("/set", {{"set", "50"}});
  run(60 * 60);
  CHECK(state == RUNNING && fabs(avgT() - 50) < 1.5, "update: lowered to 50 C (%.1f)", (double)avgT());
  CHECK(post("/stop") == 200 && server.lastBody == "Stopped" && state == IDLE && heatPct == 0 &&
        gOut[PIN_BTS_EN] == LOW && gDuty[PIN_BTS_RPWM] == 0, "stop: heater off in the same request");
  NOTE("live update 60 -> 70 -> 50 C followed; Stop cuts the heater immediately");
}

int main(int argc, char **argv) {
  std::string mode = argc > 1 ? argv[1] : "all";
  for (int &c : gChPin) c = -1;
  gDht[PIN_DHT1].on = gDht[PIN_DHT2].on = true;
  syncSensors();
  if (mode == "save") {                       // NVS reboot test, part 1
    setup();
    post("/set", {{"set", "67"}, {"min", "33"}});
    printf("saved 67 C / 33 min\nHARNESS DONE fails=0\n");
    return 0;
  }
  if (mode == "load") {                       // part 2: a fresh process = reboot
    setup();
    run(6);
    std::string j = api();
    CHECK(jnum(j, "set") == 67 && jnum(j, "min") == 33 && jstr(j, "state") == "IDLE" && heatPct == 0,
          "reboot: settings restored, IDLE, heater off (%s)", j.c_str());
    if (!fails) NOTE("reboot: target + time survive a power cut, cycle does not auto-resume");
    printf("HARNESS DONE fails=%d\n", fails);
    return fails ? 1 : 0;
  }
  if (mode == "grid") {                       // control tuning only
    setup();
    tGrid();
    printf("HARNESS DONE fails=%d\n", fails);
    return fails ? 1 : 0;
  }
  setup();
  tBoot();
  tProtocol();
  tCycle();
  tSettings();
  tLiveUpdate();
  tSensors();
  tWatchdog();
  tHardCut();
  tMillisWrap();
  tGrid();
  CHECK(invBad == 0, "invariants: %d violations (PWM without EN / heat while not RUNNING / duty mismatch)", invBad);
  if (!invBad) NOTE("invariants held on every 50 ms step: EN and PWM agree, heat only while RUNNING");
  printf("HARNESS DONE fails=%d\n", fails);
  return fails ? 1 : 0;
}
