/*
 * ============================================================================
 *  esp32_T1 - DEHUMIDIFIER                        (single file, ESP32 Dev)
 *  ESP32 DevKit + 2x DHT22 + BTS7960 heater (via 555) + fan + buzzer
 * ============================================================================
 *  ARDUINO IDE
 *    Board: "ESP32 Dev Module"  (Boards Manager: "esp32 by Espressif Systems")
 *    No libraries to install. Upload, then on the phone join WiFi
 *    "Dehumidifier" / "dryer1234" and open http://192.168.4.1
 *    Serial Monitor 115200 shows the boot log and a status line every 10 s.
 *    Stuck on "Connecting..." while uploading? Hold BOOT until it writes.
 *
 *  WIRING
 *    GPIO 25 -> 555 pin 4 (RESET); 555 pin 3 -> BTS7960 RPWM   (heater PWM)
 *               555: pin 8 -> 5 V, pins 1/2/6 -> GND, pin 5 -> 10 nF -> GND,
 *               pin 7 not connected
 *    GPIO 26 -> BTS7960 R_EN + L_EN (tied together)
 *               BTS7960: LPWM -> GND, VCC -> 5 V, B+/B- -> 12 V (fused),
 *               M+/M- -> heater coil
 *    GPIO 14 -> fan driver EN (L298N ENB: remove the ENB jumper, IN3 -> 5 V,
 *               IN4 -> GND, fan on OUT3/OUT4; or a MOSFET / relay input)
 *    GPIO 13 -> buzzer + (other leg to GND). Passive piezo = real tones;
 *               an active buzzer also works (set BUZZER_PASSIVE to 0)
 *    GPIO 32 -> DHT22 #1 DATA      GPIO 33 -> DHT22 #2 DATA
 *               each: VCC -> 3V3, GND, 10 k DATA -> 3V3 (modules have it).
 *               Both inside the chamber, away from the coil (rated 80 C).
 *    10 k to GND on GPIO 26 and GPIO 14: heater + fan stay off while the
 *    ESP32 boots (GPIO 14 may blip the fan for a moment at power-up).
 *    All grounds common.
 *
 *  WHAT IT DOES
 *    Heater : PWM never above 70 % (hard limit). Full power (the limit) up
 *             to target - 1.5 C, then PID holds the target. Stops by itself
 *             when the drying time is over.
 *    Fan    : Auto = humidity >= 70 % -> runs 3 min, then checks again.
 *             On / Off buttons override it (Auto again after a restart).
 *    Clock  : the ESP32's own RTC. Set it once on the Settings page (or
 *             "use phone time"; an unset clock is taken from the phone
 *             automatically) - it keeps counting from there. It survives
 *             resets, not power cuts.
 *    History: the last 10 batches (start, minutes, target, max temperature,
 *             humidity start -> end, energy, result), kept through power cuts.
 *    Graphs : temperature + humidity of the running / last batch.
 *    Buzzer : tones for boot, start, target reached, done, stop, warning
 *             and a repeating fault alarm. Can be switched off.
 *    Settings: presets + custom parameters (target, time, heater max,
 *             fan humidity, fan minutes, buzzer), "Defaults" restores them.
 *    Safety : heater off at boot, when stopped and on any fault; cut at 90 C
 *             (hottest sensor); no Start above 85 C; one sensor lost =
 *             warning, both lost = fault after 15 s; heater that never
 *             warms the chamber = fault.
 * ============================================================================
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>

// ================================================================ SETTINGS
#define FW_NAME         "esp32_T1 v1.2"
#define AP_SSID         "Dehumidifier"
#define AP_PASS         "dryer1234"      // at least 8 characters

#define PIN_BTS_RPWM    25    // -> 555 pin 4; 555 pin 3 -> BTS7960 RPWM
#define PIN_BTS_EN      26    // -> BTS7960 R_EN + L_EN
#define PIN_FAN_EN      14    // -> fan driver EN
#define PIN_BUZZER      13    // -> buzzer
#define PIN_DHT1        32    // DHT22 #1 DATA
#define PIN_DHT2        33    // DHT22 #2 DATA

#define HEATER_CAP_PCT  70    // HARD LIMIT: heater PWM never above this
#define HEATER_WATTS    500   // coil power at 100 % (for the energy estimate)
#define HEATER_PWM_HZ   1000  // 1 kHz through the 555
#define PWM_BITS        10    // duty 0..1023
#define HEATER_LEDC_CH  0     // LEDC channel 0 = timer 0
#define BUZZER_LEDC_CH  4     // LEDC channel 4 = timer 2 (tones never touch
                              //   the heater's PWM timer)
#define BUZZER_PASSIVE  1     // 1 = passive piezo (tones), 0 = active buzzer
#define FAN_ON_LEVEL    HIGH  // LOW for an active-low relay board

// limits for the custom parameters (Settings page)
#define SET_MIN_C       40
#define SET_MAX_C       80
#define MAX_MINUTES     1440  // 24 h
#define HEAT_MIN_PCT    10
#define FAN_RH_MIN      30
#define FAN_RH_MAX      95
#define FAN_MIN_MAX     30

#define HARD_CUT_C      90.0f // hottest sensor at/above -> heater cut + fault
#define START_BELOW_C   85.0f // Start refused until the chamber is below this

#define BAND_C          1.5f  // full power until (target - BAND_C), then PID
#define REBOOST_C       5.0f  // fell this far below (door opened) -> full power
#define KP              10.0f // heater % per C
#define KI              0.05f // heater % per (C * s)  - suits the slow DHT22
#define KD              5.0f  // heater % per (C / s)

#define READ_MS         2500  // DHT22 cadence (needs >= 2 s between reads)
#define SENSOR_FAIL_MS  15000 // both sensors offline this long -> fault
#define RISE_WINDOW_MS  180000UL  // heater check: every 3 min of high power
#define RISE_MIN_C      0.5f      // ... the chamber must warm this much
#define LOG_MS          10000 // serial status line
#define ALARM_EVERY_MS  30000 // fault alarm repeats this often
#define CLOCK_VALID     1700000000L   // earlier = clock not set yet
#define HIST_N          10    // batches kept
#define GRAPH_N         240   // graph points (resolution halves when full)

// ============================================================== PARAMETERS
struct Params {               // saved in flash; DEFAULTS = factory values
  int16_t  setC;              // target temperature, C
  uint16_t minutes;           // drying time
  uint8_t  heatMax;           // heater ceiling, % (never above HEATER_CAP_PCT)
  uint8_t  fanRh;             // Auto fan starts at/above this humidity, %
  uint8_t  fanMin;            // ... and runs this many minutes
  uint8_t  buzzer;            // 1 = sounds on
};
static const Params DEFAULTS = {60, 120, HEATER_CAP_PCT, 70, 3, 1};
static Params P = DEFAULTS;

// =================================================================== STATE
// All types stay ABOVE the first function: the Arduino IDE inserts its
// automatic function prototypes right before the first function.
enum State : uint8_t { IDLE, RUNNING, DONE, FAULT };
static const char *const STATE_NAME[] = {"IDLE", "RUNNING", "DONE", "FAULT"};
enum FanMode : uint8_t { FAN_MODE_AUTO, FAN_MODE_ON, FAN_MODE_OFF };
static const char *const FAN_MODE_NAME[] = {"AUTO", "ON", "OFF"};
enum Result : uint8_t { RES_NONE, RES_DONE, RES_STOPPED, RES_OVERTEMP, RES_SENSORS, RES_NOHEAT };
static const char *const RES_NAME[] = {"", "Done", "Stopped", "Over-temp", "Sensor fault", "No heat"};

struct Dht { uint8_t pin; float t, h; bool ok; uint8_t miss; };
struct Note { uint16_t hz, ms; };   // buzzer step: hz 0 = pause, ms 0 = end
static Dht dht1 = {PIN_DHT1, NAN, NAN, false, 0};
static Dht dht2 = {PIN_DHT2, NAN, NAN, false, 0};

struct Batch {                // one history entry, 16 bytes, newest first
  uint32_t start;             // clock at Start (0 = clock was not set)
  uint16_t mins;              // minutes it ran
  int16_t  tMax10;            // highest chamber temperature x10
  uint16_t wh;                // heater energy, Wh (estimate)
  uint8_t  target;            // C
  uint8_t  result;            // Result
  uint8_t  h0, h1;            // humidity at start / end, % (255 = unknown)
  uint8_t  pad[2];
};
static Batch hist[HIST_N];
static uint8_t histN = 0;

static WebServer server(80);
static Preferences prefs;

static State    state    = IDLE;
static uint8_t  heatPct  = 0;             // heater output now, %
static FanMode  fanMode  = FAN_MODE_AUTO;
static bool     fanOn    = false, fanBurst = false;
static uint32_t fanStart = 0;
static bool     holding  = false;         // false = full-power heat-up
static bool     reached  = false;         // target reached in this batch
static bool     stuck    = false;         // warm but no longer rising
static float    integ    = 0, lastE = 0;  // PID memory
static uint32_t startMs  = 0, elapsedS = 0;
static uint32_t lastRead = 0, lastLog = 0, failSince = 0, riseSince = 0, lastAlarm = 0;
static float    riseT0   = 0, startT = 0;
static uint32_t batchClock = 0, energyMs = 0;
static float    batchMaxT = NAN, energyWh = 0;
static uint8_t  batchH0  = 255;
static char     msg[128] = "";            // warning / fault text for the page

static int16_t  gT[GRAPH_N], gH[GRAPH_N]; // graph x10 (GRAPH_GAP = no reading)
static uint16_t gN = 0;
static uint32_t gEvery = 10;              // seconds between graph points
#define GRAPH_GAP (-32768)

static long clampL(long v, long lo, long hi) { return v < lo ? lo : v > hi ? hi : v; }
static uint8_t pct8(float v) { return isnan(v) ? 255 : (uint8_t)clampL(lroundf(v), 0, 100); }

// ================================================================== BUZZER
static const Note SND_BOOT[]    = {{1047, 90}, {1319, 90}, {1568, 140}, {0, 0}};
static const Note SND_START[]   = {{1568, 90}, {0, 60}, {2093, 160}, {0, 0}};
static const Note SND_STOP[]    = {{2093, 90}, {0, 60}, {1568, 160}, {0, 0}};
static const Note SND_REACHED[] = {{1760, 120}, {0, 80}, {1760, 120}, {0, 0}};
static const Note SND_DONE[]    = {{1568, 150}, {1760, 150}, {2093, 320}, {0, 180},
                                   {1568, 150}, {1760, 150}, {2093, 320}, {0, 0}};
static const Note SND_WARN[]    = {{880, 140}, {0, 90}, {880, 140}, {0, 0}};
static const Note SND_FAULT[]   = {{2500, 220}, {1500, 220}, {2500, 220}, {1500, 220},
                                   {2500, 220}, {1500, 220}, {0, 0}};
static const Note SND_CLICK[]   = {{2637, 40}, {0, 0}};
static const Note *snd = nullptr;
static uint8_t  sndIdx = 0;
static uint32_t sndAt = 0;

static void toneOut(uint16_t hz) {
#if BUZZER_PASSIVE
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  if (hz) ledcWriteTone(PIN_BUZZER, hz); else ledcWrite(PIN_BUZZER, 0);
#else
  if (hz) ledcWriteTone(BUZZER_LEDC_CH, hz); else ledcWrite(BUZZER_LEDC_CH, 0);
#endif
#else
  digitalWrite(PIN_BUZZER, hz ? HIGH : LOW);
#endif
}

static void playAlways(const Note *s) { snd = s; sndIdx = 0; sndAt = millis(); toneOut(s[0].hz); }
static void play(const Note *s) { if (P.buzzer) playAlways(s); }

static void buzzerLoop() {                // non-blocking note sequencer
  if (!snd || millis() - sndAt < snd[sndIdx].ms) return;
  sndAt += snd[sndIdx].ms;
  sndIdx++;
  if (snd[sndIdx].ms == 0) { toneOut(0); snd = nullptr; return; }
  toneOut(snd[sndIdx].hz);
}

// ================================================================== HEATER
static void pwmBegin() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(PIN_BTS_RPWM, HEATER_PWM_HZ, PWM_BITS, HEATER_LEDC_CH);
#if BUZZER_PASSIVE
  ledcAttachChannel(PIN_BUZZER, 2000, PWM_BITS, BUZZER_LEDC_CH);
  ledcWrite(PIN_BUZZER, 0);
#endif
#else
  ledcSetup(HEATER_LEDC_CH, HEATER_PWM_HZ, PWM_BITS);
  ledcAttachPin(PIN_BTS_RPWM, HEATER_LEDC_CH);
#if BUZZER_PASSIVE
  ledcSetup(BUZZER_LEDC_CH, 2000, PWM_BITS);
  ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CH);
  ledcWrite(BUZZER_LEDC_CH, 0);
#endif
#endif
}

static void pwmDuty(uint32_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_BTS_RPWM, duty);
#else
  ledcWrite(HEATER_LEDC_CH, duty);
#endif
}

// 0..ceiling (never above HEATER_CAP_PCT). EN follows: BTS7960 off at 0 %.
static void heater(int pct) {
  if (pct < 0) pct = 0;
  if (pct > P.heatMax) pct = P.heatMax;
  if (pct > HEATER_CAP_PCT) pct = HEATER_CAP_PCT;
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
    if (s.ok) {
      Serial.printf("[dht] GPIO%d stopped responding\n", s.pin);
      if (state == RUNNING) play(SND_WARN);
    }
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

// =================================================================== CLOCK
// The ESP32's internal RTC holds the time. It is set to LOCAL time (what you
// type / what the phone shows), so no time-zone maths is needed anywhere.
static uint32_t clockNow() {
  time_t t = time(nullptr);
  return (long)t > CLOCK_VALID ? (uint32_t)t : 0;
}
static void clockSet(uint32_t local) {
  struct timeval tv;
  tv.tv_sec = (time_t)local;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}
static void clockText(char *out, size_t n) {
  uint32_t c = clockNow();
  if (!c) { snprintf(out, n, "--:--"); return; }
  time_t tt = (time_t)c;
  struct tm tmv;
  gmtime_r(&tt, &tmv);
  snprintf(out, n, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
}

// ===================================================================== FAN
static void fanSet(bool on) {
  if (on != fanOn) Serial.printf("[fan] %s\n", on ? "ON" : "OFF");
  fanOn = on;
  digitalWrite(PIN_FAN_EN, on ? FAN_ON_LEVEL : !FAN_ON_LEVEL);
}

// Auto: while drying, humidity >= fanRh starts a fanMin-minute run; when it
// ends the humidity is checked again. On / Off = manual.
static void fanControl(uint32_t now) {
  if (fanMode != FAN_MODE_AUTO) { fanBurst = false; fanSet(fanMode == FAN_MODE_ON); return; }
  if (state != RUNNING) { fanBurst = false; fanSet(false); return; }
  if (fanBurst && now - fanStart < (uint32_t)P.fanMin * 60000UL) { fanSet(true); return; }
  float h = avgH();
  fanBurst = !isnan(h) && h >= P.fanRh;
  if (fanBurst) fanStart = now;
  fanSet(fanBurst);
}

// ================================================================== FLASH
static void loadAll() {
  prefs.begin("dryer-t1", false);
  P.setC    = (int16_t)clampL(prefs.getInt("set", DEFAULTS.setC), SET_MIN_C, SET_MAX_C);
  P.minutes = (uint16_t)clampL(prefs.getUShort("min", DEFAULTS.minutes), 1, MAX_MINUTES);
  P.heatMax = (uint8_t)clampL(prefs.getInt("hmax", DEFAULTS.heatMax), HEAT_MIN_PCT, HEATER_CAP_PCT);
  P.fanRh   = (uint8_t)clampL(prefs.getInt("frh", DEFAULTS.fanRh), FAN_RH_MIN, FAN_RH_MAX);
  P.fanMin  = (uint8_t)clampL(prefs.getInt("fmin", DEFAULTS.fanMin), 1, FAN_MIN_MAX);
  P.buzzer  = (uint8_t)clampL(prefs.getInt("buzz", DEFAULTS.buzzer), 0, 1);
  histN = (uint8_t)clampL(prefs.getInt("histN", 0), 0, HIST_N);
  if (histN && prefs.getBytes("hist", hist, sizeof hist) != sizeof hist) histN = 0;
  prefs.end();
}

static void saveParams() {
  prefs.begin("dryer-t1", false);
  prefs.putInt("set", P.setC);
  prefs.putUShort("min", P.minutes);
  prefs.putInt("hmax", P.heatMax);
  prefs.putInt("frh", P.fanRh);
  prefs.putInt("fmin", P.fanMin);
  prefs.putInt("buzz", P.buzzer);
  prefs.end();
}

static void saveHistory() {
  prefs.begin("dryer-t1", false);
  prefs.putBytes("hist", hist, sizeof hist);
  prefs.putInt("histN", histN);
  prefs.end();
}

// =================================================================== BATCH
static void energyTick(uint32_t now) {    // heater energy since the last call
  energyWh += heatPct * (HEATER_WATTS / 100.0f) * (float)(now - energyMs) / 3600000.0f;
  energyMs = now;
}

static void recordBatch(Result r) {
  energyTick(millis());
  memmove(&hist[1], &hist[0], sizeof(Batch) * (HIST_N - 1));
  Batch &b = hist[0];
  memset(&b, 0, sizeof b);
  b.start  = batchClock;
  b.mins   = (uint16_t)((elapsedS + 30) / 60);
  b.tMax10 = isnan(batchMaxT) ? GRAPH_GAP : (int16_t)lroundf(batchMaxT * 10);
  b.wh     = (uint16_t)clampL(lroundf(energyWh), 0, 65535);
  b.target = (uint8_t)P.setC;
  b.result = r;
  b.h0     = batchH0;
  b.h1     = pct8(avgH());
  if (histN < HIST_N) histN++;
  saveHistory();
  Serial.printf("[batch] %s after %u min, max %.1f C, %u Wh\n", RES_NAME[r],
                (unsigned)b.mins, (double)batchMaxT, (unsigned)b.wh);
}

static void graphAdd(uint32_t el) {       // one point per gEvery seconds
  if (el < (uint32_t)gN * gEvery) return;
  if (gN == GRAPH_N) {                    // full: keep every 2nd point
    for (uint16_t i = 0; i < GRAPH_N / 2; i++) { gT[i] = gT[2 * i]; gH[i] = gH[2 * i]; }
    gN = GRAPH_N / 2;
    gEvery *= 2;
    if (el < (uint32_t)gN * gEvery) return;
  }
  float t = avgT(), h = avgH();
  gT[gN] = isnan(t) ? GRAPH_GAP : (int16_t)lroundf(t * 10);
  gH[gN] = isnan(h) ? GRAPH_GAP : (int16_t)lroundf(h * 10);
  gN++;
}

// ================================================================= CONTROL
static void fault(Result r, const char *why) {
  state = FAULT;
  heater(0);
  snprintf(msg, sizeof msg, "%s", why);
  Serial.printf("[FAULT] %s\n", why);
  recordBatch(r);
  play(SND_FAULT);
  lastAlarm = millis();
}

// heater % for chamber temperature t
static int pid(float t) {
  const float dt = READ_MS / 1000.0f;
  const int hmax = P.heatMax;
  float e = (float)P.setC - t;
  if (holding && e > REBOOST_C) holding = false;    // big drop: full power again
  if (!holding) {
    if (e > BAND_C) { lastE = e; return hmax; }      // heat-up at the ceiling
    holding = true;                                  // near the target: PID
    if (!reached) { reached = true; play(SND_REACHED); }
  }
  // The integral is frozen (not reset) during full-power phases: no wind-up,
  // and it keeps the holding power learned so far.
  integ += KI * e * dt;
  if (integ > hmax) integ = hmax;
  if (integ < -20.0f) integ = -20.0f;
  float out = KP * e + integ + KD * (e - lastE) / dt;
  lastE = e;
  if (out < 0) out = 0;
  if (out > hmax) out = hmax;
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
  energyTick(now);

  float t = avgT();
  if (isnan(t)) {                                    // never heat blind
    heater(0);
    if (!failSince) failSince = now ? now : 1;
    if (now - failSince >= SENSOR_FAIL_MS) {
      fault(RES_SENSORS, "Both DHT22 offline - heater cut. Check the sensor wiring, then Start again.");
      return;
    }
    snprintf(msg, sizeof msg, "Both DHT22 offline - heater paused");
    return;
  }
  failSince = 0;
  if (isnan(batchMaxT) || t > batchMaxT) batchMaxT = t;

  float hot = maxT();
  if (hot >= HARD_CUT_C) {
    char why[128];
    snprintf(why, sizeof why, "Over-temperature %.1f C - heater cut. Check the coil "
             "and the sensors, then Start again.", hot);
    fault(RES_OVERTEMP, why);
    return;
  }

  elapsedS = (now - startMs) / 1000UL;
  graphAdd(elapsedS);
  if (elapsedS >= P.minutes * 60UL) {
    heater(0);
    state = DONE;
    snprintf(msg, sizeof msg, "Done - %u min completed", (unsigned)P.minutes);
    recordBatch(RES_DONE);
    play(SND_DONE);
    return;
  }

  heater(pid(t));

  // heater check, every 3 min of heating at >= 80 % of the ceiling while
  // still more than 2 C below the target: never warmed the chamber = fault
  // (coil / fuse / 12 V / EN wire); warmed but stopped rising = warning
  // (the 70 % ceiling may be too low for that target).
  if (heatPct >= P.heatMax * 8 / 10 && t < P.setC - 2.0f) {
    if (!riseSince) { riseSince = now ? now : 1; riseT0 = t; }
    else if (now - riseSince >= RISE_WINDOW_MS) {
      bool rose = t - riseT0 >= RISE_MIN_C;
      if (!rose && t - startT < 3.0f) {
        fault(RES_NOHEAT, "Heater is not heating - check the coil, fuse, 12 V supply "
                          "and EN wire, and that the sensors are inside.");
        return;
      }
      stuck = !rose;
      riseSince = now ? now : 1;  riseT0 = t;
    }
  } else {
    riseSince = 0;
    stuck = false;
  }

  if (!dht1.ok || !dht2.ok)
    snprintf(msg, sizeof msg, "DHT22 #%d offline - running on #%d only",
             dht1.ok ? 2 : 1, dht1.ok ? 1 : 2);
  else if (stuck)
    snprintf(msg, sizeof msg, "Heater at its %d %% limit but the temperature stopped rising "
             "at %.1f C - check the heater or lower the target", P.heatMax, t);
  else
    msg[0] = 0;
}

// runs after every sensor read (READ_MS) and after every command
static void control(uint32_t now) {
  heaterControl(now);
  fanControl(now);
}

// NULL = started, else the reason it was refused
static const char *startCycle() {
  if (state == RUNNING) return NULL;                 // already running
  float t = avgT();
  if (isnan(t)) return "No DHT22 responding - check the sensor wiring.";
  if (maxT() >= START_BELOW_C) return "Too hot to start - wait until the chamber is below 85 C.";
  state = RUNNING;
  startMs = millis();
  elapsedS = 0;
  holding = false;  reached = false;  stuck = false;  integ = 0;  lastE = 0;
  failSince = 0;  riseSince = 0;  msg[0] = 0;
  startT = t;  batchMaxT = t;  batchH0 = pct8(avgH());
  batchClock = clockNow();  energyWh = 0;  energyMs = startMs;
  gN = 0;  gEvery = 10;
  fanBurst = false;
  Serial.printf("[start] target %d C for %u min, heater max %u %%\n", P.setC,
                (unsigned)P.minutes, (unsigned)P.heatMax);
  play(SND_START);
  control(millis());                                 // heater on right away
  return NULL;
}

static void stopCycle() {
  if (state == RUNNING) {
    Serial.println("[stop] stopped from the web page");
    elapsedS = (millis() - startMs) / 1000UL;
    heater(0);
    recordBatch(RES_STOPPED);
    play(SND_STOP);
  }
  state = IDLE;
  elapsedS = 0;
  heater(0);
  msg[0] = 0;
  fanControl(millis());
}

// ===================================================================== WEB
static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dehumidifier</title>
<style>
*{box-sizing:border-box;margin:0}
html{background:#1d48b8}
body{min-height:100vh;padding:16px 14px 24px;color:#fff;font:15px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
body::before{content:"";position:fixed;inset:0;z-index:-1;
 background:radial-gradient(110% 40% at 50% 74%,rgba(247,201,111,.34),transparent 70%),
 linear-gradient(180deg,#2352c9 0%,#1d48b8 36%,#3560c8 55%,#c4914a 68%,#a8741d 80%,#8a5a14 100%)}
main{max-width:440px;margin:0 auto}
header{display:flex;align-items:center;gap:10px;margin-bottom:12px}
h1{flex:1;font-size:20px;font-weight:600;color:#f6cf7e}
#clk{font-size:13px;opacity:.85}
#st{font-size:12px;font-weight:700;letter-spacing:.5px;padding:4px 11px;border-radius:99px;background:rgba(255,255,255,.18)}
#st.RUNNING{background:#f6cf7e;color:#3b2606}
#st.DONE{background:#2f8f5b}
#st.FAULT{background:#b3361f}
nav{display:flex;gap:6px;margin-bottom:12px}
.card{background:rgba(18,42,120,.8);border:1px solid rgba(246,207,126,.35);border-radius:14px;padding:14px;margin-bottom:12px}
.lbl{font-size:11px;letter-spacing:.8px;text-transform:uppercase;color:#f6cf7e}
.avg{display:flex;align-items:baseline;justify-content:space-between;margin:2px 0 12px}
.avg .t{font-size:50px;font-weight:700;line-height:1.05}
.avg .h{font-size:30px;font-weight:600}
.avg small{font-size:18px;font-weight:500;opacity:.8}
.two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.sbox{background:rgba(255,255,255,.08);border-radius:10px;padding:10px}
.sbox b{display:block;font-size:20px;margin-top:2px}
.sbox.off{opacity:.45}
.tiles{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;text-align:center}
.tiles div{background:rgba(255,255,255,.08);border-radius:10px;padding:8px 4px}
.tiles b{display:block;font-size:18px}
.tiles span{font-size:11px;opacity:.75}
.sub{font-size:13px;opacity:.85;margin:10px 0}
.row{display:flex;gap:8px}
button{flex:1;padding:11px 6px;font:inherit;font-weight:600;color:#fff;background:rgba(255,255,255,.1);border:1px solid rgba(246,207,126,.45);border-radius:10px;cursor:pointer}
button.on,button.gold{background:#f6cf7e;color:#3b2606;border-color:#f6cf7e}
button.brown{background:#8a5a14;border-color:#8a5a14}
button:disabled{opacity:.4}
button small{display:block;font-weight:400;font-size:11px}
.seg{display:flex;align-items:center;gap:6px;margin-top:10px}
.seg span{width:34px;font-size:13px;opacity:.9}
.seg button{padding:7px}
canvas{display:block;width:100%;height:120px;margin:4px 0 10px}
#msg{background:rgba(138,90,20,.9);border-color:#f6cf7e}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:0 10px;margin-top:8px}
label{display:block;font-size:13px;margin-bottom:8px}
label i{font-style:normal;font-size:11px;opacity:.6}
input,select{width:100%;margin-top:3px;padding:9px;font:inherit;font-size:16px;color:#fff;background:rgba(255,255,255,.1);border:1px solid rgba(246,207,126,.45);border-radius:9px}
option{color:#1d2b5e}
input::-webkit-calendar-picker-indicator{filter:invert(1)}
.list div{padding:9px 0;border-bottom:1px solid rgba(246,207,126,.2);font-size:13px}
.list div:last-child{border:0}
.ok{color:#9fe3b5}.bad{color:#ffb39e}
.hide{display:none}
footer{text-align:center;font-size:11px;opacity:.7}
</style></head><body><main>
<header><h1>Dehumidifier</h1><span id="clk"></span><span id="st">...</span></header>
<nav><button data-tab="live" class="on">Live</button><button data-tab="hist">History</button><button data-tab="set">Settings</button></nav>

<div id="live">
 <section class="card">
  <div class="lbl">Average</div>
  <div class="avg"><div class="t"><span id="t">--</span><small> &deg;C</small></div><div class="h"><span id="h">--</span><small> %</small></div></div>
  <div class="two">
   <div class="sbox" id="c1"><div class="lbl">DHT22 #1</div><b id="t1">--</b><span id="h1"></span></div>
   <div class="sbox" id="c2"><div class="lbl">DHT22 #2</div><b id="t2">--</b><span id="h2"></span></div>
  </div>
 </section>
 <section class="card" id="msg" hidden></section>
 <section class="card">
  <div class="tiles">
   <div><b id="hp">0%</b><span>Heater</span></div>
   <div><b id="fn">Off</b><span>Fan</span></div>
   <div><b id="tl">--</b><span>Time left</span></div>
  </div>
  <div class="sub" id="pl"></div>
  <div class="row"><button id="go" class="gold">Start</button><button id="stop" class="brown">Stop</button></div>
  <div class="seg"><span>Fan</span><button data-m="auto">Auto</button><button data-m="on">On</button><button data-m="off">Off</button></div>
 </section>
 <section class="card">
  <div class="lbl">Temperature &deg;C</div><canvas id="gt"></canvas>
  <div class="lbl">Humidity %</div><canvas id="gh"></canvas>
  <div class="sub" id="gi"></div>
 </section>
</div>

<div id="hist" class="hide">
 <section class="card"><div class="lbl">Last batches</div><div class="list" id="hl"></div></section>
 <div class="row"><button id="hclr">Clear history</button></div>
</div>

<div id="set" class="hide">
 <section class="card"><div class="lbl">Presets</div><div class="row" id="pre" style="margin-top:8px"></div></section>
 <section class="card">
  <div class="lbl">Parameters <i id="pcap"></i></div>
  <div class="grid2">
   <label>Target &deg;C <i id="d_set"></i><input id="p_set" type="number" min="40" max="80" inputmode="numeric"></label>
   <label>Minutes <i id="d_min"></i><input id="p_min" type="number" min="1" max="1440" inputmode="numeric"></label>
   <label>Heater max % <i id="d_heat"></i><input id="p_heat" type="number" min="10" max="70" inputmode="numeric"></label>
   <label>Buzzer <i id="d_buzz"></i><select id="p_buzz"><option value="1">On</option><option value="0">Off</option></select></label>
   <label>Fan at RH % <i id="d_fanrh"></i><input id="p_fanrh" type="number" min="30" max="95" inputmode="numeric"></label>
   <label>Fan run min <i id="d_fanmin"></i><input id="p_fanmin" type="number" min="1" max="30" inputmode="numeric"></label>
  </div>
  <div class="row"><button id="psave" class="gold">Save</button><button id="pdef">Defaults</button></div>
 </section>
 <section class="card">
  <div class="lbl">Clock</div>
  <div class="sub" id="cnow">--</div>
  <input id="cdt" type="datetime-local">
  <div class="row" style="margin-top:8px"><button id="cset">Set time</button><button id="cphone">Use phone time</button></div>
 </section>
 <section class="card"><div class="lbl">Buzzer</div><div class="row" style="margin-top:8px"><button id="btest">Test sound</button></div></section>
</div>
<footer id="fw"></footer>
</main>
<script>
'use strict';
const $=i=>document.getElementById(i), all=s=>document.querySelectorAll(s);
const PK=['set','min','heat','fanrh','fanmin','buzz'];
const PRESETS=[['Gentle',45,180],['Standard',60,120],['Fast',70,90]];
let tab='live',edit=0,synced=false,nb=-1;
const one=v=>v==null?'--':v.toFixed(1), pc=v=>v==null?'--':v.toFixed(0);
const two=n=>String(n).padStart(2,'0');
const hm=s=>{s=Math.max(0,s|0);return (s/3600|0)+'h '+two((s%3600)/60|0)+'m'};
const when=(e,d)=>{const x=new Date(e*1000);return (d?two(x.getUTCDate())+'/'+two(x.getUTCMonth()+1)+'/'+x.getUTCFullYear()+' ':'')+two(x.getUTCHours())+':'+two(x.getUTCMinutes())};
const phone=()=>Math.floor(Date.now()/1000)-new Date().getTimezoneOffset()*60;

for(const b of all('nav button'))b.onclick=()=>{tab=b.dataset.tab;
 for(const x of all('nav button'))x.classList.toggle('on',x==b);
 for(const id of['live','hist','set'])$(id).classList.toggle('hide',id!=tab);
 if(tab=='hist')loadHist();if(tab=='live')loadGraph()};
for(const k of PK)$('p_'+k).oninput=()=>{edit=Date.now()};
for(const [n,s,m] of PRESETS){const b=document.createElement('button');
 b.innerHTML=n+'<small>'+s+' &deg;C &middot; '+m+' min</small>';
 b.onclick=()=>post('set?set='+s+'&min='+m);$('pre').appendChild(b)}

function show(d){
 $('st').textContent=d.state;$('st').className=d.state;
 $('clk').textContent=d.clock?when(d.clock):'';
 $('t').textContent=one(d.t);$('h').textContent=pc(d.h);
 for(const n of[1,2]){const t=d['t'+n];$('t'+n).textContent=t==null?'offline':one(t)+' \u00b0C';
  $('h'+n).textContent=t==null?'':pc(d['h'+n])+' %';$('c'+n).classList.toggle('off',t==null)}
 const p=d.p;
 $('hp').textContent=d.heat+'%';$('fn').textContent=d.fan?'On':'Off';
 $('tl').textContent=d.state=='RUNNING'?hm(d.left):'--';
 $('pl').textContent='Target '+p.set+' \u00b0C \u00b7 '+p.min+' min \u00b7 heater max '+p.heat+' %';
 $('msg').hidden=!d.msg;$('msg').textContent=d.msg;
 $('go').disabled=d.state=='RUNNING';
 for(const b of all('.seg button'))b.classList.toggle('on',b.dataset.m==d.fanMode.toLowerCase());
 if(Date.now()-edit>10000)for(const k of PK)$('p_'+k).value=p[k];
 for(const k of PK)$('d_'+k).textContent='default '+(k=='buzz'?(d.def[k]?'on':'off'):d.def[k]);
 $('pcap').textContent='(heater limit '+d.cap+' %)';
 $('cnow').textContent=d.clock?'Device time '+when(d.clock,1):'Device time not set';
 if(!d.clock&&!synced){synced=true;post('time?epoch='+phone(),1)}
 if(d.nb!=nb){nb=d.nb;if(tab=='hist')loadHist()}
 $('fw').textContent=d.fw;
}

function plot(cv,v,every,color,ref){
 const g=cv.getContext&&cv.getContext('2d');if(!g)return;
 const W=cv.clientWidth||300,H=cv.clientHeight||120,r=window.devicePixelRatio||1;
 cv.width=W*r;cv.height=H*r;g.setTransform(r,0,0,r,0,0);g.clearRect(0,0,W,H);
 g.font='11px system-ui';g.fillStyle='rgba(255,255,255,.75)';
 const pts=v.map((y,i)=>[i*every,y]).filter(q=>q[1]!=null);
 if(pts.length<2){g.fillText('Starts with the next batch',8,H/2);return}
 let lo=Math.min(...pts.map(q=>q[1])),hi=Math.max(...pts.map(q=>q[1]));
 if(ref!=null){lo=Math.min(lo,ref);hi=Math.max(hi,ref)}
 if(hi-lo<2){hi+=1;lo-=1}const pad=(hi-lo)*.12;lo-=pad;hi+=pad;
 const L=30,R=6,T=6,B=16,end=pts[pts.length-1][0]||1;
 const X=s=>L+(W-L-R)*s/end,Y=y=>T+(H-T-B)*(hi-y)/(hi-lo);
 g.strokeStyle='rgba(255,255,255,.15)';g.lineWidth=1;
 for(let k=0;k<=2;k++){const y=lo+(hi-lo)*k/2;g.beginPath();g.moveTo(L,Y(y));g.lineTo(W-R,Y(y));g.stroke();g.fillText(y.toFixed(0),2,Y(y)+4)}
 const m=Math.round(end/60)+' min';g.fillText('0',L,H-3);g.fillText(m,W-R-g.measureText(m).width,H-3);
 if(ref!=null){g.setLineDash([4,4]);g.strokeStyle='rgba(246,207,126,.85)';g.beginPath();g.moveTo(L,Y(ref));g.lineTo(W-R,Y(ref));g.stroke();g.setLineDash([])}
 g.strokeStyle=color;g.lineWidth=2;g.beginPath();let pen=false;
 v.forEach((y,i)=>{if(y==null){pen=false;return}if(pen)g.lineTo(X(i*every),Y(y));else{g.moveTo(X(i*every),Y(y));pen=true}});
 g.stroke();
}
async function loadGraph(){
 try{const d=await (await fetch('graph',{cache:'no-store'})).json(),f=a=>a.map(y=>y==null?null:y/10);
  plot($('gt'),f(d.t),d.every,'#f6cf7e',d.set);plot($('gh'),f(d.h),d.every,'#9fd0ff',null);
  $('gi').textContent=d.n?d.n+' points, one every '+d.every+' s (dashed = target)':''}catch(e){}
}
async function loadHist(){
 try{const list=await (await fetch('batches',{cache:'no-store'})).json(),hl=$('hl');
  hl.innerHTML=list.length?'':'<div>No batches yet</div>';
  for(const b of list){const e=document.createElement('div');
   e.innerHTML='<b>'+(b.start?when(b.start,1):'clock not set')+'</b> &middot; '+b.mins+' min &middot; '+b.set+' &deg;C<br>max '
    +one(b.tmax)+' &deg;C &middot; humidity '+pc(b.h0)+' &rarr; '+pc(b.h1)+' % &middot; '+b.wh+' Wh &middot; <span class="'
    +(b.res=='Done'?'ok':'bad')+'">'+b.res+'</span>';hl.appendChild(e)}}catch(e){}
}
async function post(p,quiet){
 try{const r=await fetch(p,{method:'POST'});const t=await r.text();if(!r.ok&&!quiet)alert(t)}
 catch(e){if(!quiet)alert('Dehumidifier not reachable')}
 edit=0;poll();
}
async function poll(){
 try{show(await (await fetch('api',{cache:'no-store'})).json())}
 catch(e){$('st').textContent='OFFLINE';$('st').className='FAULT'}
}
$('go').onclick=()=>post('start');
$('stop').onclick=()=>post('stop');
for(const b of all('.seg button'))b.onclick=()=>post('fan?mode='+b.dataset.m);
$('psave').onclick=()=>post('set?'+PK.map(k=>k+'='+encodeURIComponent($('p_'+k).value)).join('&'));
$('pdef').onclick=()=>{if(confirm('Restore all parameters to the defaults?'))post('defaults')};
$('cset').onclick=()=>{const v=$('cdt').value;if(!v)return alert('Pick a date and time first');
 post('time?epoch='+Math.floor(Date.parse(v+(v.length==16?':00':'')+'Z')/1000))};
$('cphone').onclick=()=>post('time?epoch='+phone());
$('btest').onclick=()=>post('beep');
$('hclr').onclick=()=>{if(confirm('Clear the batch history?'))post('clearhist').then(loadHist)};
poll();loadGraph();setInterval(poll,2000);setInterval(()=>{if(tab=='live')loadGraph()},15000);
</script></body></html>)HTML";

static void addNum(String &o, const char *key, float v) {   // "key":12.3 / null
  o += '"';  o += key;  o += "\":";
  if (isnan(v)) o += "null"; else o += String(v, 1);
  o += ',';
}

static void addParams(String &o, const char *key, const Params &p) {
  o += '"';  o += key;
  o += "\":{\"set\":";   o += (int)p.setC;
  o += ",\"min\":";      o += (unsigned long)p.minutes;
  o += ",\"heat\":";     o += (int)p.heatMax;
  o += ",\"fanrh\":";    o += (int)p.fanRh;
  o += ",\"fanmin\":";   o += (int)p.fanMin;
  o += ",\"buzz\":";     o += (int)p.buzzer;
  o += "},";
}

static void handleApi() {
  uint32_t total = P.minutes * 60UL;
  uint32_t el = (state == RUNNING || state == FAULT) ? elapsedS
              : (state == DONE) ? total : 0;
  String o;
  o.reserve(480);
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
  o += "\"heat\":";   o += (int)heatPct;
  o += ",\"fan\":";   o += fanOn ? "1" : "0";
  o += ",\"fanMode\":\""; o += FAN_MODE_NAME[fanMode];
  o += "\",\"el\":";  o += (unsigned long)el;
  o += ",\"left\":";  o += (unsigned long)(el < total ? total - el : 0);
  o += ",\"clock\":"; o += (unsigned long)clockNow();
  o += ",\"nb\":";    o += (int)histN;
  o += ",\"cap\":";   o += (int)HEATER_CAP_PCT;
  o += ",";
  addParams(o, "p", P);
  addParams(o, "def", DEFAULTS);
  o += "\"msg\":\""; o += msg;
  o += "\",\"fw\":\"" FW_NAME "\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", o);
}

static void handleGraph() {
  String o;
  o.reserve(64 + gN * 12);
  o += "{\"every\":"; o += (unsigned long)gEvery;
  o += ",\"n\":";     o += (int)gN;
  o += ",\"set\":";   o += (int)P.setC;
  for (int k = 0; k < 2; k++) {
    const int16_t *a = k ? gH : gT;
    o += k ? "],\"h\":[" : ",\"t\":[";
    for (uint16_t i = 0; i < gN; i++) {
      if (i) o += ',';
      if (a[i] == GRAPH_GAP) o += "null"; else o += (int)a[i];
    }
  }
  o += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", o);
}

static void handleBatches() {
  String o;
  o.reserve(40 + histN * 120);
  o += '[';
  for (uint8_t i = 0; i < histN; i++) {
    const Batch &b = hist[i];
    if (i) o += ',';
    o += "{\"start\":"; o += (unsigned long)b.start;
    o += ",\"mins\":";  o += (unsigned long)b.mins;
    o += ",\"set\":";   o += (int)b.target;
    o += ",\"tmax\":";
    if (b.tMax10 == GRAPH_GAP) o += "null"; else o += String(b.tMax10 / 10.0f, 1);
    o += ",\"h0\":";    if (b.h0 == 255) o += "null"; else o += (int)b.h0;
    o += ",\"h1\":";    if (b.h1 == 255) o += "null"; else o += (int)b.h1;
    o += ",\"wh\":";    o += (unsigned long)b.wh;
    o += ",\"res\":\""; o += RES_NAME[b.result <= (uint8_t)RES_NOHEAT ? b.result : 0];
    o += "\"}";
  }
  o += ']';
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", o);
}

// integer argument (digits, optional leading '-'); false = missing / junk
static bool argNum(const char *k, long &v) {
  if (!server.hasArg(k)) return false;
  String s = server.arg(k);
  if (!s.length()) return false;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (!(isdigit((unsigned char)c) || (i == 0 && c == '-' && s.length() > 1))) return false;
  }
  v = s.toInt();
  return true;
}

// ?set=&min=&heat=&fanrh=&fanmin=&buzz= (any subset). Clamped into range.
static void takeArgs() {
  Params n = P;
  long v;
  if (argNum("set", v))    n.setC    = (int16_t)clampL(v, SET_MIN_C, SET_MAX_C);
  if (argNum("min", v))    n.minutes = (uint16_t)clampL(v, 1, MAX_MINUTES);
  if (argNum("heat", v))   n.heatMax = (uint8_t)clampL(v, HEAT_MIN_PCT, HEATER_CAP_PCT);
  if (argNum("fanrh", v))  n.fanRh   = (uint8_t)clampL(v, FAN_RH_MIN, FAN_RH_MAX);
  if (argNum("fanmin", v)) n.fanMin  = (uint8_t)clampL(v, 1, FAN_MIN_MAX);
  if (argNum("buzz", v))   n.buzzer  = (uint8_t)clampL(v, 0, 1);
  if (memcmp(&n, &P, sizeof n) != 0) {
    P = n;
    saveParams();
    Serial.printf("[set] target %d C, %u min, heater max %u %%, fan %u %% / %u min, buzzer %s\n",
                  P.setC, (unsigned)P.minutes, (unsigned)P.heatMax, (unsigned)P.fanRh,
                  (unsigned)P.fanMin, P.buzzer ? "on" : "off");
  }
}

static void setupWeb() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", PAGE); });
  server.on("/api", HTTP_GET, handleApi);
  server.on("/graph", HTTP_GET, handleGraph);
  server.on("/batches", HTTP_GET, handleBatches);
  server.on("/start", HTTP_POST, [] {
    bool wasRunning = (state == RUNNING);
    takeArgs();
    const char *why = startCycle();
    server.send(why ? 409 : 200, "text/plain",
                why ? why : wasRunning ? "Already running - settings saved" : "Started");
  });
  server.on("/stop", HTTP_POST, [] {
    stopCycle();
    server.send(200, "text/plain", "Stopped");
  });
  server.on("/set", HTTP_POST, [] {
    takeArgs();
    play(SND_CLICK);
    fanControl(millis());
    server.send(200, "text/plain", "Saved");
  });
  server.on("/defaults", HTTP_POST, [] {
    P = DEFAULTS;
    saveParams();
    play(SND_CLICK);
    fanControl(millis());
    Serial.println("[set] factory defaults restored");
    server.send(200, "text/plain", "Defaults restored");
  });
  server.on("/time", HTTP_POST, [] {                 // ?epoch= local time, seconds
    long v;
    if (!argNum("epoch", v) || v <= CLOCK_VALID || v > 2000000000L) {
      server.send(400, "text/plain", "epoch must be a local time in seconds (2023-2033)");
      return;
    }
    clockSet((uint32_t)v);
    char hhmm[8];
    clockText(hhmm, sizeof hhmm);
    Serial.printf("[clock] set to %s\n", hhmm);
    server.send(200, "text/plain", "Time set");
  });
  server.on("/fan", HTTP_POST, [] {                  // ?mode=auto | on | off
    String m = server.arg("mode");
    if (m == "auto")     fanMode = FAN_MODE_AUTO;
    else if (m == "on")  fanMode = FAN_MODE_ON;
    else if (m == "off") fanMode = FAN_MODE_OFF;
    else { server.send(400, "text/plain", "mode must be auto, on or off"); return; }
    Serial.printf("[fan] mode %s\n", FAN_MODE_NAME[fanMode]);
    fanControl(millis());
    server.send(200, "text/plain", FAN_MODE_NAME[fanMode]);
  });
  server.on("/beep", HTTP_POST, [] {
    playAlways(SND_DONE);                            // test even when muted
    server.send(200, "text/plain", "Beep");
  });
  server.on("/clearhist", HTTP_POST, [] {
    histN = 0;
    saveHistory();
    server.send(200, "text/plain", "History cleared");
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
#if !BUZZER_PASSIVE
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
#endif
  pwmBegin();
  heater(0);

  Serial.begin(115200);
  delay(50);
  Serial.printf("\n%s - Dehumidifier: ESP32 Dev + 2x DHT22 + BTS7960 (via 555) + fan + buzzer\n",
                FW_NAME);

  pinMode(PIN_DHT1, OUTPUT_OPEN_DRAIN | PULLUP);   // idle HIGH (released)
  digitalWrite(PIN_DHT1, HIGH);
  pinMode(PIN_DHT2, OUTPUT_OPEN_DRAIN | PULLUP);
  digitalWrite(PIN_DHT2, HIGH);

  loadAll();

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASS, 6, 0, 4)) Serial.println("[wifi] hotspot FAILED");
  setupWeb();

  char hhmm[8];
  clockText(hhmm, sizeof hhmm);
  Serial.printf("heater: RPWM GPIO%d (%d Hz, limit %d %%) EN GPIO%d | fan GPIO%d | buzzer GPIO%d | "
                "DHT22 GPIO%d + GPIO%d\n", PIN_BTS_RPWM, HEATER_PWM_HZ, HEATER_CAP_PCT, PIN_BTS_EN,
                PIN_FAN_EN, PIN_BUZZER, PIN_DHT1, PIN_DHT2);
  Serial.printf("target %d C, %u min | clock %s | %u batches in history | WiFi \"%s\" / \"%s\" -> http://%s\n",
                P.setC, (unsigned)P.minutes, hhmm, (unsigned)histN, AP_SSID, AP_PASS,
                WiFi.softAPIP().toString().c_str());
  play(SND_BOOT);
  lastRead = millis();                      // first read after READ_MS (sensor warm-up)
  lastLog = millis();
}

void loop() {
  server.handleClient();
  buzzerLoop();
  uint32_t now = millis();
  if (now - lastRead >= READ_MS) {
    lastRead = now;
    readDht(dht1);
    readDht(dht2);
    control(millis());
  }
  if (state == FAULT && P.buzzer && now - lastAlarm >= ALARM_EVERY_MS) {
    lastAlarm = now;
    play(SND_FAULT);                        // repeats until Stop / Start
  }
  if (now - lastLog >= LOG_MS) {
    lastLog = now;
    char hhmm[8];
    clockText(hhmm, sizeof hhmm);
    Serial.printf("[%s %s] t=%.1f C (%.1f / %.1f) h=%.1f %% target=%d heat=%u %% fan=%s(%s) %s\n",
                  hhmm, STATE_NAME[state], avgT(), dht1.ok ? dht1.t : NAN, dht2.ok ? dht2.t : NAN,
                  avgH(), P.setC, (unsigned)heatPct, fanOn ? "on" : "off",
                  FAN_MODE_NAME[fanMode], msg);
  }
  delay(1);
}
