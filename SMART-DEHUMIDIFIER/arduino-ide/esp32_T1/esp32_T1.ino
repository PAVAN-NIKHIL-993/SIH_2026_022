/*
 * ============================================================================
 *  esp32_T1 - MINIMAL AGARBATTI DRYER            (single file, ESP32 Dev)
 *  ESP32 DevKit + 2x DHT22 + BTS7960 heater (via 555) + fan (one EN pin)
 * ============================================================================
 *  ARDUINO IDE
 *    Board: "ESP32 Dev Module"  (Boards Manager: "esp32 by Espressif Systems")
 *    No libraries to install. Upload, then on the phone join WiFi
 *    "AgarbattiDryer" / "dryer1234" and open http://192.168.4.1
 *    Serial Monitor 115200 shows the boot log and a status line every 10 s.
 *    Stuck on "Connecting..." while uploading? Hold BOOT until it writes.
 *
 *  WIRING
 *    GPIO 25 -> 555 pin 4 (RESET); 555 pin 3 -> BTS7960 RPWM   (heater PWM)
 *    GPIO 26 -> BTS7960 R_EN + L_EN (tied together)
 *               BTS7960: LPWM -> GND, VCC -> 5 V, B+/B- -> 12 V, M+/M- -> coil
 *    GPIO 14 -> fan driver EN (single pin, on/off)
 *               L298N: remove the ENB jumper, ENB <- GPIO 14, IN3 -> 5 V,
 *               IN4 -> GND, fan on OUT3/OUT4  (or a MOSFET / relay input;
 *               active-low relay board -> set FAN_ON_LEVEL to LOW)
 *    GPIO 32 -> DHT22 #1 DATA      GPIO 33 -> DHT22 #2 DATA
 *               each: VCC -> 3V3, GND, 10 k DATA -> 3V3 (modules have it).
 *               Both inside the chamber, away from the coil (rated 80 C).
 *    10 k to GND on GPIO 26 and GPIO 14: heater + fan stay off while the
 *    ESP32 boots or is flashed (GPIO 14 may blip the fan for a moment at
 *    power-up).  All grounds common.
 *
 *  WHAT IT DOES
 *    Heater : full power up to (target - 1.5 C), then PID holds the target.
 *             Stops by itself when the drying time is over.
 *    Fan    : Auto = vents while drying when humidity >= 60 %, off <= 50 %.
 *             On / Off buttons override it (back to Auto after a restart).
 *    Safety : heater off at boot, when stopped and on any fault; cut at 90 C
 *             (hottest sensor); no Start above 85 C; one sensor lost =
 *             warning, both lost = fault after 15 s; no temperature rise in
 *             3 min at high power = fault (coil / fuse / 12 V / EN wire).
 *    Target and time are remembered; after a power cut it stays stopped.
 * ============================================================================
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ================================================================ SETTINGS
#define FW_NAME         "esp32_T1 v1.1"
#define AP_SSID         "AgarbattiDryer"
#define AP_PASS         "dryer1234"      // at least 8 characters

#define PIN_BTS_RPWM    25    // -> 555 pin 4; 555 pin 3 -> BTS7960 RPWM
#define PIN_BTS_EN      26    // -> BTS7960 R_EN + L_EN
#define PIN_FAN_EN      14    // -> fan driver EN
#define PIN_DHT1        32    // DHT22 #1 DATA
#define PIN_DHT2        33    // DHT22 #2 DATA

#define FAN_ON_LEVEL    HIGH  // LOW for an active-low relay board
#define FAN_ON_RH       60    // Auto: fan on at/above this humidity (drying)
#define FAN_OFF_RH      50    // ... and off again at/below this

#define HEATER_PWM_HZ   1000  // 1 kHz through the 555
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
#define KP              10.0f // heater % per C
#define KI              0.05f // heater % per (C * s)  - suits the slow DHT22
#define KD              5.0f  // heater % per (C / s)

#define READ_MS         2500  // DHT22 cadence (needs >= 2 s between reads)
#define SENSOR_FAIL_MS  15000 // both sensors offline this long -> fault
#define RISE_WINDOW_MS  180000UL  // heater watchdog: every 3 min of high power
#define RISE_MIN_C      0.5f      // must raise the chamber at least this much
#define LOG_MS          10000 // serial status line

// =================================================================== STATE
enum State : uint8_t { IDLE, RUNNING, DONE, FAULT };
static const char *const STATE_NAME[] = {"IDLE", "RUNNING", "DONE", "FAULT"};
enum FanMode : uint8_t { FAN_MODE_AUTO, FAN_MODE_ON, FAN_MODE_OFF };
static const char *const FAN_MODE_NAME[] = {"AUTO", "ON", "OFF"};

struct Dht { uint8_t pin; float t, h; bool ok; uint8_t miss; };
static Dht dht1 = {PIN_DHT1, NAN, NAN, false, 0};
static Dht dht2 = {PIN_DHT2, NAN, NAN, false, 0};

static WebServer server(80);
static Preferences prefs;

static State    state    = IDLE;
static int      setC     = DEF_SET_C;     // target, whole degrees C
static uint16_t minutes  = DEF_MINUTES;   // drying time
static uint8_t  heatPct  = 0;             // heater output now, %
static FanMode  fanMode  = FAN_MODE_AUTO;
static bool     fanOn    = false;
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

// ===================================================================== FAN
static void fanSet(bool on) {
  if (on != fanOn) Serial.printf("[fan] %s\n", on ? "ON" : "OFF");
  fanOn = on;
  digitalWrite(PIN_FAN_EN, on ? FAN_ON_LEVEL : !FAN_ON_LEVEL);
}

// Auto: vent moist air while drying (humidity hysteresis); On / Off = manual
static void fanControl() {
  if (fanMode != FAN_MODE_AUTO) { fanSet(fanMode == FAN_MODE_ON); return; }
  float h = avgH();
  if (state != RUNNING || isnan(h)) fanSet(false);
  else if (h >= FAN_ON_RH)          fanSet(true);
  else if (h <= FAN_OFF_RH)         fanSet(false);
  // between FAN_OFF_RH and FAN_ON_RH: keep the current state
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

static void heaterControl(uint32_t now) {
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

// runs after every sensor read (READ_MS) and after every command
static void control(uint32_t now) {
  heaterControl(now);
  fanControl();
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
  fanControl();
}

// ===================================================================== WEB
static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Agarbatti Dryer</title>
<style>
*{box-sizing:border-box;margin:0}
html{background:#1d48b8}
body{min-height:100vh;padding:18px 14px;color:#fff;font:16px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
body::before{content:"";position:fixed;inset:0;z-index:-1;
 background:radial-gradient(110% 40% at 50% 74%,rgba(247,201,111,.34),transparent 70%),
 linear-gradient(180deg,#2352c9 0%,#1d48b8 36%,#3560c8 55%,#c4914a 68%,#a8741d 80%,#8a5a14 100%)}
main{max-width:420px;margin:0 auto}
header{display:flex;justify-content:space-between;align-items:center;margin-bottom:14px}
h1{font-size:20px;font-weight:600;color:#f6cf7e}
#st{font-size:12px;font-weight:700;letter-spacing:.5px;padding:4px 11px;border-radius:99px;background:rgba(255,255,255,.18)}
#st.RUNNING{background:#f6cf7e;color:#3b2606}
#st.DONE{background:#2f8f5b}
#st.FAULT{background:#b3361f}
.card{background:rgba(16,38,110,.55);border:1px solid rgba(246,207,126,.35);border-radius:14px;padding:16px;margin-bottom:12px}
.temp{font-size:58px;font-weight:700;line-height:1}
.temp small{font-size:20px;font-weight:500;opacity:.8}
.sub{opacity:.85;margin-top:8px}
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:16px;text-align:center}
.stats div{background:rgba(255,255,255,.08);border-radius:10px;padding:8px 4px}
.stats b{display:block;font-size:18px}
.stats span{font-size:12px;opacity:.75}
.sens{font-size:13px;opacity:.8;margin-top:12px}
#msg{background:rgba(138,90,20,.9);border-color:#f6cf7e}
.row{display:flex;gap:10px}
label{flex:1;font-size:13px;opacity:.9}
input{width:100%;margin-top:4px;padding:10px;font:inherit;font-size:18px;color:#fff;background:rgba(255,255,255,.1);border:1px solid rgba(246,207,126,.45);border-radius:10px}
button{flex:1;padding:12px;font:inherit;font-weight:600;color:#fff;background:rgba(255,255,255,.1);border:1px solid rgba(246,207,126,.45);border-radius:10px;cursor:pointer}
.btns{margin-top:12px}
#go{background:#f6cf7e;color:#3b2606;border-color:#f6cf7e}
#stop{background:#8a5a14;border-color:#8a5a14}
.fan{display:flex;align-items:center;gap:8px;margin-top:14px}
.fan span{font-size:13px;opacity:.9;width:36px}
.fan button{padding:8px}
.fan button.on{background:#f6cf7e;color:#3b2606;border-color:#f6cf7e}
</style></head><body><main>
<header><h1>Agarbatti Dryer</h1><span id="st">...</span></header>
<section class="card">
 <div class="temp"><span id="t">--</span><small> &deg;C</small></div>
 <div class="sub">Target <b id="ts">--</b> &deg;C &middot; Humidity <b id="h">--</b> %</div>
 <div class="stats">
  <div><b id="hp">0%</b><span>Heater</span></div>
  <div><b id="fn">Off</b><span>Fan</span></div>
  <div><b id="tl">--</b><span>Time left</span></div>
 </div>
 <div class="sens" id="sn"></div>
</section>
<section class="card" id="msg" hidden></section>
<section class="card">
 <div class="row">
  <label>Target &deg;C<input id="fs" type="number" min="40" max="80" step="1" inputmode="numeric"></label>
  <label>Minutes<input id="fm" type="number" min="1" max="1440" step="1" inputmode="numeric"></label>
 </div>
 <div class="row btns"><button id="go">Start</button><button id="stop">Stop</button></div>
 <div class="fan"><span>Fan</span><button data-m="auto">Auto</button><button data-m="on">On</button><button data-m="off">Off</button></div>
</section>
</main>
<script>
'use strict';
const $=i=>document.getElementById(i);
const fanBtns=document.querySelectorAll('.fan button');
let run=false,edit=0;
const one=v=>v==null?'--':v.toFixed(1);
const hm=s=>{s=Math.max(0,s|0);return (s/3600|0)+'h '+String((s%3600)/60|0).padStart(2,'0')+'m'};
const sensor=(n,t,h)=>'Sensor '+n+' '+(t==null?'offline':t.toFixed(1)+' \u00b0C \u00b7 '+h.toFixed(0)+' %');
for(const i of['fs','fm'])$(i).oninput=()=>{edit=Date.now()};
function show(d){
 $('st').textContent=d.state;$('st').className=d.state;
 $('t').textContent=one(d.t);$('ts').textContent=d.set;$('h').textContent=d.h==null?'--':d.h.toFixed(0);
 $('hp').textContent=d.heat+'%';$('fn').textContent=d.fan?'On':'Off';
 $('tl').textContent=d.state=='RUNNING'?hm(d.left):'--';
 $('sn').textContent=sensor(1,d.t1,d.h1)+'  |  '+sensor(2,d.t2,d.h2);
 $('msg').hidden=!d.msg;$('msg').textContent=d.msg;
 run=d.state=='RUNNING';$('go').textContent=run?'Update':'Start';
 for(const b of fanBtns)b.classList.toggle('on',b.dataset.m==d.fanMode.toLowerCase());
 if(Date.now()-edit>8000){$('fs').value=d.set;$('fm').value=d.min}
}
async function poll(){
 try{const r=await fetch('api',{cache:'no-store'});show(await r.json())}
 catch(e){$('st').textContent='OFFLINE';$('st').className='FAULT'}
}
async function post(p){
 try{const r=await fetch(p,{method:'POST'});if(!r.ok)alert(await r.text())}
 catch(e){alert('Dryer not reachable')}
 edit=0;poll();
}
$('go').onclick=()=>post((run?'set':'start')+'?set='+encodeURIComponent($('fs').value)
 +'&min='+encodeURIComponent($('fm').value));
$('stop').onclick=()=>post('stop');
for(const b of fanBtns)b.onclick=()=>post('fan?mode='+b.dataset.m);
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
  o.reserve(320);
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
  o += ",\"fan\":";  o += fanOn ? "1" : "0";
  o += ",\"fanMode\":\""; o += FAN_MODE_NAME[fanMode];
  o += "\",\"msg\":\""; o += msg;
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
  server.on("/fan", HTTP_POST, [] {                  // ?mode=auto | on | off
    String m = server.arg("mode");
    if (m == "auto")     fanMode = FAN_MODE_AUTO;
    else if (m == "on")  fanMode = FAN_MODE_ON;
    else if (m == "off") fanMode = FAN_MODE_OFF;
    else { server.send(400, "text/plain", "mode must be auto, on or off"); return; }
    Serial.printf("[fan] mode %s\n", FAN_MODE_NAME[fanMode]);
    fanControl();
    server.send(200, "text/plain", FAN_MODE_NAME[fanMode]);
  });
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

// =================================================================== MAIN
void setup() {
  pinMode(PIN_BTS_EN, OUTPUT);              // heater + fan OFF before anything
  digitalWrite(PIN_BTS_EN, LOW);
  pinMode(PIN_BTS_RPWM, OUTPUT);
  digitalWrite(PIN_BTS_RPWM, LOW);
  pinMode(PIN_FAN_EN, OUTPUT);
  digitalWrite(PIN_FAN_EN, !FAN_ON_LEVEL);
  pwmBegin();
  heater(0);

  Serial.begin(115200);
  delay(50);
  Serial.printf("\n%s - ESP32 Dev + 2x DHT22 + BTS7960 (via 555) + fan\n", FW_NAME);

  pinMode(PIN_DHT1, OUTPUT_OPEN_DRAIN | PULLUP);   // idle HIGH (released)
  digitalWrite(PIN_DHT1, HIGH);
  pinMode(PIN_DHT2, OUTPUT_OPEN_DRAIN | PULLUP);
  digitalWrite(PIN_DHT2, HIGH);

  loadSettings();

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASS, 6, 0, 4)) Serial.println("[wifi] hotspot FAILED");
  setupWeb();

  Serial.printf("heater: RPWM GPIO%d (%d Hz, max %d %%) EN GPIO%d | fan EN GPIO%d | "
                "DHT22: GPIO%d + GPIO%d\n", PIN_BTS_RPWM, HEATER_PWM_HZ, HEATER_MAX_PCT,
                PIN_BTS_EN, PIN_FAN_EN, PIN_DHT1, PIN_DHT2);
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
    Serial.printf("[%s] t=%.1f C (%.1f / %.1f) h=%.1f %% target=%d heat=%u %% fan=%s(%s) %s\n",
                  STATE_NAME[state], avgT(), dht1.ok ? dht1.t : NAN, dht2.ok ? dht2.t : NAN,
                  avgH(), setC, (unsigned)heatPct, fanOn ? "on" : "off",
                  FAN_MODE_NAME[fanMode], msg);
  }
  delay(1);
}
