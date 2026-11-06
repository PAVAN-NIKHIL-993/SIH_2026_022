/*
 * ============================================================================
 *  esp32_T1 - MINIMAL AGARBATTI DRYER
 *  ESP32 DevKit + 2x DHT22 + BTS7960 heater driver (through the 555)
 * ============================================================================
 *  One file, no extra libraries (only the esp32 board package).
 *
 *  ARDUINO IDE 2 - HOW TO FLASH
 *    1. Boards Manager -> search "esp32" -> install "esp32 by Espressif Systems".
 *    2. Tools -> Board -> esp32 -> "ESP32 Dev Module".
 *    3. USB DATA cable -> Tools -> Port -> select it -> Upload.
 *       Stuck on "Connecting..."? Hold BOOT until "Writing..." starts.
 *    4. Phone -> WiFi "AgarbattiDryer" / "dryer1234" -> open http://192.168.4.1
 *    Serial Monitor 115200: boot log + one status line every 10 s.
 *
 *  WIRING (ESP32 DevKit V1)
 *    GPIO 25 -> 555 pin 4 (RESET);  555 pin 3 (OUT) -> BTS7960 RPWM
 *               (555 on 5 V lifts the 3.3 V PWM to 5 V - GPIO HIGH = heat)
 *    GPIO 26 -> BTS7960 R_EN + L_EN (tied together)
 *               + 10 k resistor GPIO 26 -> GND (recommended): the heater
 *               stays OFF while the ESP32 boots or is being flashed
 *    BTS7960: LPWM -> GND, VCC -> 5 V, GND -> common GND,
 *             B+ / B- -> 12 V supply, M+ / M- -> heater coil
 *    GPIO 32 -> DHT22 #1 DATA    GPIO 33 -> DHT22 #2 DATA
 *               each: VCC -> 3V3, GND -> GND, 10 k DATA -> 3V3
 *               (3-pin DHT22 modules already have the 10 k)
 *    Both DHT22 go INSIDE the chamber, away from the coil (rated to 80 C).
 *
 *  WHAT IT DOES
 *    - holds the chamber at the target (40-80 C): full power until 1.5 C
 *      below the target, then PID (integral gain tuned for the DHT22)
 *    - chamber temperature = average of the working DHT22s; one may fail
 *      (warning, keeps drying), both offline 15 s = fault
 *    - stops by itself after the drying time (1-1440 min)
 *    - SAFETY: heater off at boot / when idle / on any fault; hard cut when
 *      the hottest sensor reaches 90 C; heater watchdog: no temperature
 *      rise for 3 min at high power = fault (dead coil, fuse, 12 V or EN
 *      wire - or a sensor that is not in the chamber)
 *    - web page: live readings, target + time, Start / Update / Stop;
 *      target and time survive power cuts (a cut always ends the cycle)
 *
 *  REMOVED vs the full SMART-DEHUMIDIFIER firmware: AHT10s, weigh scale,
 *  keypad + menus, displays, RTC, EEPROM + cycle logs, door, supply relays,
 *  battery monitor, buzzer, buttons, power latch, status LED, fan, OTA,
 *  online UI, weather, GitHub backup.
 * ============================================================================
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ================================================================ SETTINGS
#define FW_NAME         "esp32_T1 v1.0"
#define AP_SSID         "AgarbattiDryer"
#define AP_PASS         "dryer1234"      // at least 8 characters

#define PIN_BTS_RPWM    25    // -> 555 pin 4; 555 pin 3 -> BTS7960 RPWM
#define PIN_BTS_EN      26    // -> BTS7960 R_EN + L_EN
#define PIN_DHT1        32    // DHT22 #1 DATA
#define PIN_DHT2        33    // DHT22 #2 DATA

#define HEATER_PWM_HZ   1000  // 1 kHz through the 555 (as the full firmware)
#define PWM_BITS        10    // duty 0..1023
#define HEATER_MAX_PCT  100   // 100 = 500 W coil; 70 = ~350 W (PSU only)

#define SET_MIN_C       40
#define SET_MAX_C       80
#define DEF_SET_C       60    // agarbatti
#define DEF_MINUTES     120
#define MAX_MINUTES     1440  // 24 h
#define HARD_CUT_C      90.0f // hottest sensor at/above -> heater cut + fault
#define START_BELOW_C   85.0f // Start refused until the chamber is below this

#define BAND_C          1.5f  // full power until (target - BAND_C), then PID
#define REBOOST_C       5.0f  // fell this far below (door opened) -> full power
#define KP              10.0f // heater % per C          (as the full firmware)
#define KI              0.05f // heater % per (C * s)    (full firmware: 0.2 -
                              //   with DHT22-only sensing that oscillated by
                              //   4-9 C in simulation; 0.05 holds +-0.2 C)
#define KD              5.0f  // heater % per (C / s)    (as the full firmware)

#define READ_MS         2500  // DHT22 cadence (needs >= 2 s between reads)
#define SENSOR_FAIL_MS  15000 // both sensors offline this long -> fault
#define RISE_WINDOW_MS  180000UL  // heater watchdog: every 3 min of high power
#define RISE_MIN_C      0.5f      // must raise the chamber at least this much
#define LOG_MS          10000 // serial status line

// =================================================================== STATE
enum State : uint8_t { IDLE, RUNNING, DONE, FAULT };
static const char *const STATE_NAME[] = {"IDLE", "RUNNING", "DONE", "FAULT"};

struct Dht { uint8_t pin; float t, h; bool ok; uint8_t miss; };
static Dht dht1 = {PIN_DHT1, NAN, NAN, false, 0};
static Dht dht2 = {PIN_DHT2, NAN, NAN, false, 0};

static WebServer server(80);
static Preferences prefs;

static State    state    = IDLE;
static int      setC     = DEF_SET_C;     // target, whole degrees C
static uint16_t minutes  = DEF_MINUTES;   // drying time
static uint8_t  heatPct  = 0;             // heater output now, %
static bool     holding  = false;         // false = full-power heat-up
static float    integ    = 0, lastE = 0;  // PID memory
static uint32_t startMs  = 0, elapsedS = 0;
static uint32_t lastRead = 0, lastLog = 0, failSince = 0, riseSince = 0;
static float    riseT0   = 0;
static char     msg[112] = "";            // warning / fault text for the page

// ================================================================== HEATER
static void pwmBegin() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_BTS_RPWM, HEATER_PWM_HZ, PWM_BITS);   // core 3.x
#else
  ledcSetup(0, HEATER_PWM_HZ, PWM_BITS);                // core 2.x
  ledcAttachPin(PIN_BTS_RPWM, 0);
#endif
}

static void pwmDuty(uint32_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_BTS_RPWM, duty);
#else
  ledcWrite(0, duty);
#endif
}

// 0..HEATER_MAX_PCT. EN follows the output: the BTS7960 is disabled at 0 %.
static void heater(int pct) {
  if (pct < 0) pct = 0;
  if (pct > HEATER_MAX_PCT) pct = HEATER_MAX_PCT;
  heatPct = (uint8_t)pct;
  if (pct == 0) {
    pwmDuty(0);
    digitalWrite(PIN_BTS_EN, LOW);
  } else {
    digitalWrite(PIN_BTS_EN, HIGH);
    pwmDuty((uint32_t)pct * ((1UL << PWM_BITS) - 1) / 100);
  }
}

// ================================================================== DHT22
// Library-free. The line is open-drain (the 10 k pull-up makes the HIGH), so
// no pinMode() switching is needed inside the timing-critical part.
static portMUX_TYPE dhtMux = portMUX_INITIALIZER_UNLOCKED;

// microseconds the line stayed at `level`, or -1 after timeoutUs
static int32_t held(uint8_t pin, int level, uint32_t timeoutUs) {
  uint32_t t0 = micros();
  while (digitalRead(pin) == level)
    if (micros() - t0 > timeoutUs) return -1;
  return (int32_t)(micros() - t0);
}

static bool dhtFrame(uint8_t pin, float &t, float &h) {
  uint8_t d[5] = {0, 0, 0, 0, 0};
  digitalWrite(pin, LOW);                   // start signal: >= 1 ms low
  delay(2);
  bool ok = true;
  portENTER_CRITICAL(&dhtMux);              // ~5 ms of us timing
  digitalWrite(pin, HIGH);                  // release the line
  // The sensor answers 20-200 us later (datasheet): 80 us LOW + 80 us HIGH,
  // then 40 bits. Sync on those edges instead of guessing a fixed delay.
  if (held(pin, HIGH, 250) < 0 || held(pin, LOW, 120) < 0 ||
      held(pin, HIGH, 120) < 0) ok = false;
  for (uint8_t i = 0; ok && i < 40; i++) {
    int32_t hi = -1;                        // bit: 50 us LOW, then HIGH
    if (held(pin, LOW, 90) < 0 || (hi = held(pin, HIGH, 110)) < 0) ok = false;
    else d[i >> 3] = (uint8_t)((d[i >> 3] << 1) | (hi > 48 ? 1 : 0));  // 27 / 70 us
  }
  portEXIT_CRITICAL(&dhtMux);
  if (!ok) return false;
  if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return false;   // checksum
  if ((d[0] | d[1] | d[2] | d[3]) == 0) return false;                 // all zero
  h = (float)(((uint16_t)d[0] << 8) | d[1]) * 0.1f;
  float a = (float)(((uint16_t)(d[2] & 0x7F) << 8) | d[3]) * 0.1f;
  t = (d[2] & 0x80) ? -a : a;
  return h <= 100.0f && t > -40.5f && t < 125.0f;
}

static void readDht(Dht &s) {
  float t, h;
  if (dhtFrame(s.pin, t, h)) {
    if (!s.ok) Serial.printf("[dht] GPIO%d OK: %.1f C %.1f %%RH\n", s.pin, t, h);
    s.t = t;  s.h = h;  s.ok = true;  s.miss = 0;
  } else if (s.miss < 255 && ++s.miss >= 3) {     // 3 misses in a row = offline
    if (s.ok) Serial.printf("[dht] GPIO%d stopped responding\n", s.pin);
    s.ok = false;
  }
}

static float avgT() {
  if (dht1.ok && dht2.ok) return (dht1.t + dht2.t) / 2;
  return dht1.ok ? dht1.t : dht2.ok ? dht2.t : NAN;
}
static float avgH() {
  if (dht1.ok && dht2.ok) return (dht1.h + dht2.h) / 2;
  return dht1.ok ? dht1.h : dht2.ok ? dht2.h : NAN;
}
static float maxT() {
  if (dht1.ok && dht2.ok) return dht1.t > dht2.t ? dht1.t : dht2.t;
  return dht1.ok ? dht1.t : dht2.ok ? dht2.t : NAN;
}

// ================================================================ SETTINGS
static void loadSettings() {
  prefs.begin("dryer-t1", false);
  setC    = constrain(prefs.getInt("set", DEF_SET_C), SET_MIN_C, SET_MAX_C);
  minutes = constrain(prefs.getUShort("min", DEF_MINUTES), 1, MAX_MINUTES);
  prefs.end();
}

static void saveSettings() {
  prefs.begin("dryer-t1", false);
  prefs.putInt("set", setC);
  prefs.putUShort("min", minutes);
  prefs.end();
}

// ?set=60&min=120 (both optional). Clamped; empty / junk values are ignored.
static void takeArgs() {
  bool changed = false;
  long v = server.hasArg("set") ? server.arg("set").toInt() : 0;
  if (v > 0) {
    v = constrain(v, SET_MIN_C, SET_MAX_C);
    if (v != setC) { setC = (int)v; changed = true; }
  }
  v = server.hasArg("min") ? server.arg("min").toInt() : 0;
  if (v > 0) {
    v = constrain(v, 1, MAX_MINUTES);
    if (v != minutes) { minutes = (uint16_t)v; changed = true; }
  }
  if (changed) {
    saveSettings();
    Serial.printf("[set] target %d C, time %u min\n", setC, (unsigned)minutes);
  }
}

// ================================================================= CONTROL
static void fault(const char *why) {
  state = FAULT;
  heater(0);
  snprintf(msg, sizeof msg, "%s", why);
  Serial.printf("[FAULT] %s\n", why);
}

// heater % for chamber temperature t
static int pid(float t) {
  const float dt = READ_MS / 1000.0f;
  float e = (float)setC - t;
  if (holding && e > REBOOST_C) holding = false;    // big drop: full power again
  if (!holding) {
    if (e > BAND_C) { lastE = e; return HEATER_MAX_PCT; }   // heat-up
    holding = true;                                  // near the target: PID
  }
  // The integral is frozen (not reset) during full-power phases: no wind-up,
  // and it keeps the holding power learned so far.
  integ += KI * e * dt;
  if (integ > HEATER_MAX_PCT) integ = HEATER_MAX_PCT;
  if (integ < -20.0f) integ = -20.0f;
  float out = KP * e + integ + KD * (e - lastE) / dt;
  lastE = e;
  if (out < 0) out = 0;
  if (out > HEATER_MAX_PCT) out = HEATER_MAX_PCT;
  return (int)(out + 0.5f);
}

// runs after every sensor read (READ_MS)
static void control(uint32_t now) {
  if (state != RUNNING) {
    heater(0);
    if (state == IDLE)
      snprintf(msg, sizeof msg, "%s", (dht1.ok || dht2.ok) ? ""
               : "No DHT22 responding - check the DATA wires and 10 k pull-ups");
    return;
  }

  float t = avgT();
  if (isnan(t)) {                                    // never heat blind
    heater(0);
    if (!failSince) failSince = now ? now : 1;
    if (now - failSince >= SENSOR_FAIL_MS) {
      fault("Both DHT22 offline - heater cut. Check the sensor wiring, then Start again.");
      return;
    }
    snprintf(msg, sizeof msg, "Both DHT22 offline - heater paused");
    return;
  }
  failSince = 0;

  float hot = maxT();
  if (hot >= HARD_CUT_C) {
    char why[112];
    snprintf(why, sizeof why, "Over-temperature %.1f C - heater cut. Check the coil "
             "and the sensors, then Start again.", hot);
    fault(why);
    return;
  }

  elapsedS = (now - startMs) / 1000UL;
  if (elapsedS >= minutes * 60UL) {
    state = DONE;
    heater(0);
    snprintf(msg, sizeof msg, "Done - %u min completed", (unsigned)minutes);
    Serial.printf("[done] %u min completed\n", (unsigned)minutes);
    return;
  }

  heater(pid(t));

  // heater watchdog (sliding 3-min windows while heating hard and still
  // more than 2 C below the target)
  if (heatPct >= HEATER_MAX_PCT * 8 / 10 && t < setC - 2.0f) {
    if (!riseSince) { riseSince = now ? now : 1; riseT0 = t; }
    else if (now - riseSince >= RISE_WINDOW_MS) {
      if (t - riseT0 < RISE_MIN_C) {
        fault("No temperature rise in 3 min at high heater power - check coil, "
              "fuse, 12 V, EN wire, sensor placement.");
        return;
      }
      riseSince = now ? now : 1;  riseT0 = t;        // fine - next window
    }
  } else {
    riseSince = 0;
  }

  if (!dht1.ok || !dht2.ok)
    snprintf(msg, sizeof msg, "DHT22 #%d offline - running on #%d only",
             dht1.ok ? 2 : 1, dht1.ok ? 1 : 2);
  else
    msg[0] = 0;
}

// NULL = started, else the reason it was refused
static const char *startCycle() {
  if (state == RUNNING) return NULL;                 // already running
  if (isnan(avgT())) return "No DHT22 responding - check the sensor wiring.";
  if (maxT() >= START_BELOW_C) return "Too hot to start - wait until the chamber is below 85 C.";
  state = RUNNING;
  startMs = millis();
  elapsedS = 0;
  holding = false;  integ = 0;  lastE = 0;
  failSince = 0;  riseSince = 0;  msg[0] = 0;
  Serial.printf("[start] target %d C for %u min\n", setC, (unsigned)minutes);
  control(millis());                                 // heater on right away
  return NULL;
}

static void stopCycle() {
  if (state == RUNNING) Serial.println("[stop] stopped from the web page");
  state = IDLE;
  elapsedS = 0;
  heater(0);
  msg[0] = 0;
}

// ===================================================================== WEB
static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Agarbatti Dryer</title>
<style>
:root{--gold:#f2c46d;--brown:#8a5a14;--card:rgba(20,44,128,.62);--edge:rgba(242,196,109,.6)}
*{box-sizing:border-box}
html{background:#1d48b8}
body{margin:0;min-height:100vh;color:#fff;font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
body::before{content:"";position:fixed;inset:0;z-index:-1;
 background:radial-gradient(110% 40% at 50% 74%,rgba(247,201,111,.34),transparent 70%),
 linear-gradient(180deg,#2352c9 0%,#1d48b8 36%,#3560c8 55%,#c4914a 68%,#a8741d 80%,#8a5a14 100%)}
main{max-width:480px;margin:0 auto;padding:14px}
header{display:flex;align-items:center;justify-content:space-between;margin:4px 2px 10px}
h1{margin:0;font-size:21px;color:var(--gold)}
.pill{padding:4px 12px;border-radius:999px;font-size:13px;font-weight:700;background:#3560c8;border:1px solid var(--edge)}
.pill.RUNNING{background:var(--gold);color:#3b2606}
.pill.DONE{background:#2f8f5b}
.pill.FAULT{background:#a8321c}
.card{background:var(--card);border:1px solid var(--edge);border-radius:16px;padding:14px 16px;margin:10px 0;box-shadow:0 6px 18px rgba(10,20,60,.25)}
.lbl{font-size:12px;letter-spacing:.8px;text-transform:uppercase;color:var(--gold)}
.big{font-size:52px;font-weight:700;line-height:1.1}
.big small{font-size:22px;font-weight:600}
.bar{height:10px;border-radius:6px;background:rgba(255,255,255,.2);overflow:hidden;margin:10px 0 4px}
.bar i{display:block;height:100%;width:0;background:linear-gradient(90deg,#f7c96f,#c4914a);transition:width .6s}
.two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.two .card{margin:0}
.v{font-size:22px;font-weight:700}
.off{opacity:.5}
#msg{background:rgba(138,90,20,.92);border-color:var(--gold)}
label{display:block;font-size:14px;margin:4px 0 10px}
input{width:100%;margin-top:4px;padding:10px 12px;font:inherit;font-size:18px;color:#fff;background:rgba(255,255,255,.12);border:1px solid var(--edge);border-radius:10px}
.btns{display:grid;grid-template-columns:1fr 1fr;gap:10px}
button{padding:13px;font:inherit;font-weight:700;border-radius:12px;border:1px solid var(--edge);cursor:pointer}
#go{background:var(--gold);color:#3b2606}
#stop{background:var(--brown);color:#fff}
footer{text-align:center;font-size:12px;opacity:.85;margin:12px 0}
</style></head><body><main>
<header><h1>Agarbatti Dryer</h1><span id="st" class="pill">...</span></header>
<section class="card">
 <div class="lbl">Chamber</div>
 <div class="big"><span id="t">--</span><small> &deg;C</small></div>
 <div>Humidity <b id="h">--</b> % &middot; Target <b id="ts">--</b> &deg;C</div>
 <div class="bar"><i id="hb"></i></div>
 <div>Heater <b id="hp">0</b> % <span id="ph"></span></div>
 <div id="tm"></div>
</section>
<div class="two">
 <section class="card" id="c1"><div class="lbl">DHT22 #1</div><div class="v"><span id="t1">--</span> &deg;C</div><div><span id="h1">--</span> %</div></section>
 <section class="card" id="c2"><div class="lbl">DHT22 #2</div><div class="v"><span id="t2">--</span> &deg;C</div><div><span id="h2">--</span> %</div></section>
</div>
<section class="card" id="msg" hidden></section>
<section class="card">
 <label>Target temperature (40&ndash;80 &deg;C)<input id="fs" type="number" min="40" max="80" step="1" inputmode="numeric"></label>
 <label>Drying time (minutes)<input id="fm" type="number" min="1" max="1440" step="1" inputmode="numeric"></label>
 <div class="btns"><button id="go">Start</button><button id="stop">Stop</button></div>
</section>
<footer id="fw"></footer>
</main>
<script>
'use strict';
const $=i=>document.getElementById(i);
let run=false,edit=0;
const f1=v=>v==null?'--':v.toFixed(1);
const hm=s=>{s=Math.max(0,s|0);return (s/3600|0)+'h '+String((s%3600)/60|0).padStart(2,'0')+'m'};
for(const i of['fs','fm'])$(i).oninput=()=>{edit=Date.now()};
function show(d){
 $('st').textContent=d.state;$('st').className='pill '+d.state;
 $('t').textContent=f1(d.t);$('h').textContent=f1(d.h);$('ts').textContent=d.set;
 for(const n of[1,2]){$('t'+n).textContent=f1(d['t'+n]);$('h'+n).textContent=f1(d['h'+n]);
  $('c'+n).classList.toggle('off',d['t'+n]==null)}
 $('hp').textContent=d.heat;$('hb').style.width=d.heat+'%';
 $('ph').textContent=d.phase=='HEAT-UP'?'\u00b7 heating up':d.phase=='HOLD'?'\u00b7 holding':'';
 $('tm').textContent=d.state=='RUNNING'?hm(d.left)+' left of '+hm(d.min*60)
  :d.state=='IDLE'?'Set the target and time, then Start':'';
 $('msg').hidden=!d.msg;$('msg').textContent=d.msg;
 run=d.state=='RUNNING';$('go').textContent=run?'Update':'Start';
 if(Date.now()-edit>8000){$('fs').value=d.set;$('fm').value=d.min}
 $('fw').textContent=d.fw;
}
async function poll(){
 try{const r=await fetch('api',{cache:'no-store'});show(await r.json())}
 catch(e){$('st').textContent='OFFLINE';$('st').className='pill FAULT'}
}
async function post(p){
 try{const r=await fetch(p,{method:'POST'});const t=await r.text();if(!r.ok)alert(t)}
 catch(e){alert('Dryer not reachable')}
 edit=0;poll();
}
$('go').onclick=()=>post((run?'set':'start')+'?set='+encodeURIComponent($('fs').value)
 +'&min='+encodeURIComponent($('fm').value));
$('stop').onclick=()=>post('stop');
poll();setInterval(poll,2000);
</script></body></html>)HTML";

static void addNum(String &o, const char *key, float v) {   // "key":12.3 / null
  o += '"';  o += key;  o += "\":";
  if (isnan(v)) o += "null"; else o += String(v, 1);
  o += ',';
}

static void handleApi() {
  uint32_t total = minutes * 60UL;
  uint32_t el = (state == RUNNING || state == FAULT) ? elapsedS
              : (state == DONE) ? total : 0;
  String o;
  o.reserve(300);
  o += "{\"state\":\"";  o += STATE_NAME[state];
  o += "\",\"phase\":\"";
  o += (state == RUNNING) ? (holding ? "HOLD" : "HEAT-UP") : "";
  o += "\",";
  addNum(o, "t", avgT());
  addNum(o, "h", avgH());
  addNum(o, "t1", dht1.ok ? dht1.t : NAN);
  addNum(o, "h1", dht1.ok ? dht1.h : NAN);
  addNum(o, "t2", dht2.ok ? dht2.t : NAN);
  addNum(o, "h2", dht2.ok ? dht2.h : NAN);
  o += "\"set\":";   o += setC;
  o += ",\"min\":";  o += (unsigned long)minutes;
  o += ",\"el\":";   o += (unsigned long)el;
  o += ",\"left\":"; o += (unsigned long)(el < total ? total - el : 0);
  o += ",\"heat\":"; o += (unsigned long)heatPct;
  o += ",\"msg\":\""; o += msg;
  o += "\",\"fw\":\"" FW_NAME "\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", o);
}

static void setupWeb() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", PAGE); });
  server.on("/api", HTTP_GET, handleApi);
  server.on("/start", HTTP_POST, [] {
    bool wasRunning = (state == RUNNING);
    takeArgs();
    const char *why = startCycle();
    server.send(why ? 409 : 200, "text/plain",
                why ? why : wasRunning ? "Already running - settings saved" : "Started");
  });
  server.on("/set", HTTP_POST, [] {
    takeArgs();
    server.send(200, "text/plain", "Saved");
  });
  server.on("/stop", HTTP_POST, [] {
    stopCycle();
    server.send(200, "text/plain", "Stopped");
  });
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

// =================================================================== MAIN
void setup() {
  pinMode(PIN_BTS_EN, OUTPUT);              // heater OFF before anything else
  digitalWrite(PIN_BTS_EN, LOW);
  pinMode(PIN_BTS_RPWM, OUTPUT);
  digitalWrite(PIN_BTS_RPWM, LOW);
  pwmBegin();
  heater(0);

  Serial.begin(115200);
  delay(50);
  Serial.printf("\n%s - ESP32 Dev + 2x DHT22 + BTS7960 (via 555)\n", FW_NAME);

  pinMode(PIN_DHT1, OUTPUT_OPEN_DRAIN | PULLUP);   // idle HIGH (released)
  digitalWrite(PIN_DHT1, HIGH);
  pinMode(PIN_DHT2, OUTPUT_OPEN_DRAIN | PULLUP);
  digitalWrite(PIN_DHT2, HIGH);

  loadSettings();

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASS, 6, 0, 4)) Serial.println("[wifi] hotspot FAILED");
  setupWeb();

  Serial.printf("heater: RPWM GPIO%d (%d Hz, max %d %%) EN GPIO%d | DHT22: GPIO%d + GPIO%d\n",
                PIN_BTS_RPWM, HEATER_PWM_HZ, HEATER_MAX_PCT, PIN_BTS_EN, PIN_DHT1, PIN_DHT2);
  Serial.printf("target %d C, %u min | WiFi \"%s\" / \"%s\" -> http://%s\n", setC,
                (unsigned)minutes, AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
  lastRead = millis();                      // first read after READ_MS (sensor warm-up)
  lastLog = millis();
}

void loop() {
  server.handleClient();
  uint32_t now = millis();
  if (now - lastRead >= READ_MS) {
    lastRead = now;
    readDht(dht1);
    readDht(dht2);
    control(millis());
  }
  if (now - lastLog >= LOG_MS) {
    lastLog = now;
    Serial.printf("[%s] t=%.1f C (%.1f / %.1f) h=%.1f %% target=%d heat=%u %% %s\n",
                  STATE_NAME[state], avgT(), dht1.ok ? dht1.t : NAN,
                  dht2.ok ? dht2.t : NAN, avgH(), setC, (unsigned)heatPct, msg);
  }
  delay(1);
}
