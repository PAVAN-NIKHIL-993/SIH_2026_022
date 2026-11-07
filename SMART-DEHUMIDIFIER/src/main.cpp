/**
 * @file main.cpp
 * @brief Smart Dehumidifier - application entry point.
 *
 * Two build modes (config.h: DRYER_RTOS):
 *   0 = cooperative loop (default) - one loop() calls every module,
 *       each module internally time-gated. Deterministic, simplest.
 *   1 = FreeRTOS task architecture - see docs/manual/08-RTOS-ARCHITECTURE.md
 *       taskSensors/taskControl/taskWeb/taskPower/taskPanel, Wire mutex,
 *       web server pinned to core 0 alongside the WiFi stack.
 *
 * Boot order matters (both modes):
 *   1. outputs safe (relay OPEN too if RELAYS_ENABLED)
 *   2. sensors / battery / control / settings / cycle history
 *   3. WiFi AP + web server
 *   4. IDLE, waiting for "Start" on the website (or AUTO_START)
 */
#include <Arduino.h>
#include <WiFi.h>
#if __has_include("esp_bt.h")
#include <esp_bt.h>          // v2.0.18: release BLE RAM at boot
#define BT_RELEASE_SUPPORTED 1
#endif
#include "config.h"
#include "control.h"
#include "sensors.h"
#include "battery.h"
#include "cyclelog.h"
#include "buzzer.h"
#include "keypad.h"
#include "display.h"
#include "scale.h"
#include "door.h"
#include "supply.h"
#include "menu.h"
#include "txdisp.h"
#include "dht.h"
#include "pixel.h"
#include "rtc.h"
#include "web.h"
#if OTA_NETWORK_ENABLED
#include <ESPmDNS.h>          // core built-ins - network OTA from the
#include <ArduinoOTA.h>       // Arduino IDE over the dryer hotspot
#endif
#include "web.h"

#if DRYER_RTOS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#endif

// instances live in control.cpp: sensors, battery, dryer, cfg

static void printBanner() {
  Serial.println(F("\n========================================="));
  Serial.printf("  SMART DEHUMIDIFIER PILLAR  v%s\n", FW_VERSION);
  Serial.println(F("  550W solar - ESP32 + BTS7960 + L298N"));
  Serial.println(F("========================================="));
}

// ---- full automation: start the cycle by itself at power-up ----------
// Waits for the boot grace period, then (if a sensor answers, or another
// 60 s max) fires START. Any earlier user action wins. One cycle per boot.
static void maybeAutoStart() {
#if AUTO_START
  static bool done = false;
  if (done) return;
  uint32_t now = millis();
  if (now < AUTO_START_DELAY_MS) return;
  if (dryer.state() != DState::IDLE) { done = true; return; }   // user acted
  if (!sensors.anyOk() && now < AUTO_START_DELAY_MS + 60000UL) return;
  done = true;
  Serial.printf("[main] AUTO-START: drying begins (%.1fC target, %u min)\n",
                cfg.setTemp, (unsigned)cfg.dryMinutes);
  dryer.start();
#endif
}

// ---- v2.0.1: park every pin the firmware does NOT use ------------------
// Spec: "remaining all pins disabled". A floating CMOS input picks up
// noise and burns power, so every pad that no module owns gets a DEFINED
// state: INPUT_PULLUP where the silicon has one, plain INPUT on the
// input-only pads. Flash/PSRAM/USB/UART0 pins are never touched.
static bool pinUsed(int p) {
  static const int used[] = {
    PIN_I2C0_SDA, PIN_I2C0_SCL, PIN_I2C1_SDA, PIN_I2C1_SCL,
    PIN_BTS_RPWM, PIN_BTS_EN, PIN_L298_ENA, PIN_L298_IN1, PIN_L298_IN2,
    PIN_L298_ENB, PIN_L298_IN3, PIN_L298_IN4,
    PIN_VBAT_ADC, PIN_VBAT_ENABLE,
    PIN_LOAD_RELAY, PIN_BYPASS_CTRL, PIN_SUPPLY_CH1, PIN_SUPPLY_CH2,
    PIN_SOLAR_TOGGLE, PIN_SUPPLY_OPTO,
    PIN_BTN1, PIN_BTN2, PIN_POWER_HOLD, PIN_BUZZER, PIN_PIXEL,
#if DISPLAY_ENABLED          // v2.0.23: a compiled-out module owns no pads,
    PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST,
#endif                       // so its spare pins get parked like the rest
    PIN_SCALE_CLK, PIN_SCALE_DOUT, PIN_DOOR_LOCK, PIN_DOOR_REED,
#if TXDISP_ENABLED           // (S3: 3 + 40/42/47 were left floating)
    PIN_TXDISP_TX,
#endif
    PIN_DHT22_CHAMBER, PIN_DHT11_OUT,
#ifdef PIN_BTN
    PIN_BTN,
#endif
  };
  for (int u : used) if (u == p) return true;
  return false;
}

static void pinsUnusedSafe() {
  uint8_t parked = 0;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  const int kMax = 48;
#else
  const int kMax = 39;
#endif
  for (int p = 0; p <= kMax; p++) {
    if (pinUsed(p)) continue;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    if (p == 19 || p == 20 || p == 43 || p == 44) continue; // USB / UART0
    if (p >= 26 && p <= 32) continue;                       // flash / PSRAM
    if (p == 35 || p == 36 || p == 37) continue;            // R8 PSRAM lines
    if (p == 45 || p == 46 || p == 0) continue;             // boot straps
    pinMode(p, INPUT_PULLDOWN); parked++;                   // v2.0.18: DISABLED
#else
    if (p == 1 || p == 3) continue;                         // UART0 = console
    if (p == 0) continue;                                   // boot strap
    if (p >= 6 && p <= 11) continue;                        // flash
    if (p == 20 || p == 24 || (p >= 28 && p <= 31)) continue; // not bonded
    if (p >= 34) { pinMode(p, INPUT); continue; }           // input-only pads
    pinMode(p, INPUT_PULLDOWN); parked++;                   // v2.0.18: DISABLED
#endif
  }
  Serial.printf("[pins] %u unused pads parked DISABLED (pull-down / input)\n",
                parked);
}

// ---- hex keypad (v2.0 map: full on-device menu) ------------------------
//   2=UP 4=LEFT 6=RIGHT 8=DOWN   1/3/5/7/9/0 = digits   * = HOME   # = BACK
//   A = ENTER/OK    B = MENU     C = MODE cycle         D = RUN/STOP
void dryerKey(char k) {          // v2.0.19: physical keypad AND web /api/key
  Serial.printf("[key] '%c'\n", k);
  buzzer.beep(1, 30);                       // key press feedback
  bz::play(BP::KEY);                       // #1: every valid key click
  if (menu::key(k)) return;                 // menu consumed it
  switch (k) {
    case 'A':                                 // OK on the main screen: start
    case 'D':                                 // RUN - or STOP while running
      if (dryer.state() == DState::RUNNING) dryer.stop();
      else dryer.start();
      break;
    case 'C':                                 // MODE: AGARBATTI->USER->SILICA
      dryer.applyMode((dryer.mode() + 1) % 3);
      buzzer.beep(1, 150);
      break;
    default: break;
  }
}

// ---- USB/serial command console (service + demo without a phone) ------
// Type plain commands in the serial monitor (115200, line ending ANY):
//   help         list commands
//   start/stop   cycle control        power   re-energise after DONE/FAULT
//   stat         print the [stat] line now
//   temp 60      set target 60 C (live)      time 90   set drying time 90 min
//   knob 100     manual heat 100 % for 60 s  defaults reset to factory
static bool diagForce = false;            // 'stat' command bypasses the 15 s gate
static void serviceSerial() {
  static char buf[48]; static uint8_t n = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c != '\n' && c != '\r') {
      if (n < sizeof(buf) - 1) buf[n++] = c;
      continue;
    }
    buf[n] = 0; n = 0;
    if (!buf[0]) continue;
    Serial.printf("[cmd] %s\n", buf);
    if      (!strcmp(buf, "help"))    Serial.print(F("start stop power stat | temp <C> time <min> knob <%> defaults | tare | cal <g> | door unlock | mode <0=agarbatti 1=user 2=silica> | supply switch | power off | rtc | rtcset | pixel 48|38\n"));
    else if (!strncmp(buf, "mode ", 5)) dryer.applyMode(constrain(atoi(buf + 5), 0, 2));
    else if (!strcmp(buf, "supply switch")) supply::requestSwitch();
    else if (!strcmp(buf, "power off"))    supply::powerOff();
    else if (!strcmp(buf, "tare"))    scale.tare();
    else if (!strncmp(buf, "cal ", 4)) { scale.calibrate(atof(buf + 4));
                                         if (scale.ok()) door::markCalibrated(); }
    else if (!strcmp(buf, "door unlock")) door::serviceUnlock();
    else if (!strcmp(buf, "start"))   dryer.start();
    else if (!strcmp(buf, "stop"))    dryer.stop();
    else if (!strcmp(buf, "power"))   dryer.powerOn();
    else if (!strcmp(buf, "stat"))    diagForce = true;
    else if (!strncmp(buf, "temp ", 5))  { cfg.setTemp = constrain(atof(buf + 5), 40.0f, 80.0f);
                                           dryer.applySettings(cfg);
                                           Serial.printf("[cmd] setTemp=%.1fC (40-80 v2.0)\n", cfg.setTemp); }
    else if (!strncmp(buf, "time ", 5))  { cfg.dryMinutes = atoi(buf + 5); dryer.applySettings(cfg); Serial.printf("[cmd] dryMinutes=%u\n", (unsigned)cfg.dryMinutes); }
    else if (!strncmp(buf, "knob ", 5))  dryer.setManualHeat((uint8_t)constrain(atoi(buf + 5), 0, 100));
    else if (!strcmp(buf, "rtc"))     Serial.printf("[cmd] %s\n", rtc::statusText());
    else if (!strcmp(buf, "rtcset"))  { rtc::writeNow(); Serial.printf("[cmd] %s\n", rtc::statusText()); }
#if PIXEL_ENABLED
    else if (!strncmp(buf, "pixel ", 6)) { int p = atoi(buf + 6);
      if (p == 48 || p == 38) pixel::rePin(p);
      else Serial.println(F("[cmd] pixel: 48 (v1.0 boards) or 38 (v1.1)")); }
#endif
    else if (!strcmp(buf, "defaults"))   { cfg = defaultSettings(); saveSettings(cfg); dryer.applySettings(cfg); Serial.println(F("[cmd] factory defaults applied")); }
    else Serial.println(F("[cmd] unknown - type help"));
  }
}

// ---- BOOT button = physical start/stop (S3 variant; zero wiring) -------
#if BTN_ENABLED
static void serviceButton() {
  static bool prev = true; static uint32_t tDown = 0;
  bool pressed = (digitalRead(PIN_BTN) == LOW);
  uint32_t now = millis();
  if (pressed && !prev) tDown = now;                 // press edge
  if (!pressed && prev && tDown) {                   // release edge
    if (now - tDown > 2000) dryer.powerOn();         // long: power on
    else if (dryer.state() == DState::RUNNING) dryer.stop();
    else dryer.start();                              // short: start/stop
    tDown = 0;
  }
  prev = pressed;
}
#else
static inline void serviceButton() {}
#endif

// ---- hardware error detection: catch the failures software CAN see -----
// Runs on the same 15 s cadence as maybeDiag (edge-triggered warns):
//   frozen sensors - clone/dead AHT10s freeze at one exact value
//   sensor disagreement - 15 C apart for 2+ min = placement/wiring
//   heater ineffective - 90 %+ duty for 10 min with no temperature rise
//   battery ADC dead - reads ~0 V for minutes (divider wiring)
//   battery over-voltage - above the chemistry window for 2+ min
//   door reed stuck - fitted but never reports closed
//   scale restless - big g/min swings while IDLE (vibration / mounting)
// NOT detectable without extra hardware: fan RPM (no tach pin), heater
// current (no shunt), display backlight - documented in manual 05.
static void hardwareWatch() {
  uint32_t now = millis();

  // -- frozen sensor values (exact same reading, healthy flags) ----------
  static float pT1 = -999, pH1 = -999, pT2 = -999, pH2 = -999;
  static uint8_t frz1 = 0, frz2 = 0;
  float t1 = sensors.t1(), h1 = sensors.h1(), t2 = sensors.t2(), h2 = sensors.h2();
  if (sensors.s1ok() && t1 == pT1 && h1 == pH1) { if (++frz1 == 8)
      Serial.println(F("[warn] AHT10 #1 FROZEN (same value 2 min) - clone/dead sensor?")); }
  else frz1 = 0;
  if (sensors.s2ok() && t2 == pT2 && h2 == pH2) { if (++frz2 == 8)
      Serial.println(F("[warn] AHT10 #2 FROZEN (same value 2 min) - clone/dead sensor?")); }
  else frz2 = 0;
  pT1 = t1; pH1 = h1; pT2 = t2; pH2 = h2;

  // -- sensors disagree (15 C apart for 2+ min) ---------------------------
  static uint8_t dis = 0;
  if (sensors.s1ok() && sensors.s2ok() && fabsf(t1 - t2) >= 15.0f) {
    if (++dis == 8) Serial.println(F("[warn] AHT10 #1/#2 disagree >=15C - check placement/wiring"));
  } else dis = 0;

  // (heater-ineffective detection moved INTO the controller as a FAULT:
  //  "temperature constant for 3 min" while heating - spec, v2.0)

  // -- battery ADC ---------------------------------------------------------
  static uint16_t zeroV = 0, overV = 0;
  float v = battery.volts();
  if (!battery.valid() && v < 0.5f) { if (++zeroV == 20)
      Serial.println(F("[warn] battery ADC reads ~0 V for 5 min - divider wiring / ADC pin"));}
  else zeroV = 0;
  float vmax = cfg.battType == 1 ? 17.0f : (cfg.battType == 3 ? 14.8f :
               (cfg.battType == 2 ? 14.9f : 13.2f));
  if (battery.valid() && v > vmax) { if (++overV == 8)
      Serial.printf("[warn] battery OVER-VOLTAGE %.2fV (limit %.1f) - wrong battType?\n",
                    (double)v, (double)vmax); }
  else overV = 0;

  // -- door reed stuck open (fitted, phase past calibration) ---------------
#if DOOR_ENABLED
  static uint32_t reedOpen = 0;
  if (door::calibrated() && !door::closed() && dryer.state() == DState::IDLE) {
    if (reedOpen == 0) reedOpen = now;
    else if (now - reedOpen > 600000UL) {
      Serial.println(F("[warn] door sensor never reports closed (10 min) - check the reed/magnet"));
      reedOpen = now;                    // re-arm: one line per 10 min
    }
  } else reedOpen = 0;
#endif

  // -- scale restless while idle (vibration / loose mounting) --------------
  static uint16_t restless = 0;
  if (scale.ok() && dryer.state() == DState::IDLE && !isnan(scale.rate()) &&
      fabsf(scale.rate()) > 50.0f) {
    if (++restless == 20) Serial.println(F("[warn] scale restless at idle (>50 g/min) - vibration / mounting"));
  } else restless = 0;

  // -- heap ----------------------------------------------------------------
  static bool lowHeap = false;
  if (!lowHeap && ESP.getFreeHeap() < 40000) {
    Serial.printf("[warn] LOW HEAP %u B - reboot advised\n", (unsigned)ESP.getFreeHeap());
    lowHeap = true;
  }
}

// ---- serial diagnostics: instant MISSING/OK warnings + status line ----
// Everything questionable shows up here (and faults also stop the cycle):
//   [warn] lines fire the moment a module's health CHANGES
//   [stat] line prints every DIAG_PERIOD_MS with the full picture
static const char *clkStr() {          // for [stat] / [diag]
  static char b[20];
  time_t tN = time(nullptr);
  if (tN > (time_t)1700000000) {
    struct tm m; localtime_r(&tN, &m);
    strftime(b, sizeof(b), "%y-%m-%d %H:%M", &m);
  } else snprintf(b, sizeof(b), "unset");
  return b;
}

static void maybeDiag() {
  static uint32_t last = 0;
  static bool pS1 = true, pS2 = true, pKp = false;
  bool s1 = sensors.s1ok(), s2 = sensors.s2ok(), by = battery.bypass();
  bool kp = keypad.ok(), sc = scale.ok();

  if (s1 != pS1) { Serial.printf("[warn] AHT10 #1 (top) %s\n",  s1 ? "OK again" : "MISSING!"); pS1 = s1; }
  if (s2 != pS2) { Serial.printf("[warn] AHT10 #2 (bottom) %s\n",s2 ? "OK again" : "MISSING!"); pS2 = s2; }
  if (kp != pKp) { Serial.printf("[warn] hex keypad %s\n",       kp ? "detected" : "not responding (optional)"); pKp = kp; }
  static bool pSc = false;
  if (sc != pSc) { Serial.printf("[warn] weight scale %s\n",      sc ? "OK" : "no data (optional)"); pSc = sc; }
#if RELAYS_ENABLED
  static bool pBy = false;
  if (by != pBy) { Serial.printf("[warn] BYPASS %s\n",           by ? "ENGAGED (battery low)" : "released"); pBy = by; }
#endif
  float w1 = sensors.t1(), w2 = sensors.t2();       // AHT10 abs max = 85 C
  static bool pH1 = false, pH2 = false;
  bool h1 = !isnan(w1) && w1 >= AHT10_HOT_C, h2 = !isnan(w2) && w2 >= AHT10_HOT_C;
  if (h1 != pH1) { Serial.printf("[warn] AHT10 #1 %s (%.1fC)\n", h1 ? "HOT - near 85C limit, move sensor to the cool return path for 90C+ recipes" : "back in range", w1); pH1 = h1; }
  if (h2 != pH2) { Serial.printf("[warn] AHT10 #2 %s (%.1fC)\n", h2 ? "HOT - near 85C limit, move sensor to the cool return path for 90C+ recipes" : "back in range", w2); pH2 = h2; }

  uint32_t now = millis();
  if (!diagForce && last != 0 && now - last < DIAG_PERIOD_MS) return;
  diagForce = false;
  last = now ? now : 1;
  hardwareWatch();


  String miss;
  if (!s1) miss += "AHT10#1 ";
  if (!s2) miss += SENS2_DHT ? "DHT22#2 " : "AHT10#2 ";
  if (!kp) miss += "keypad ";
  if (!sc) miss += "scale ";
  if (!battery.valid()) miss += "batteryADC ";
  if (miss.length()) miss.remove(miss.length() - 1); else miss = "none";

  Serial.printf("[stat] %s%s t=%.1f/%.1fC rh=%.0f/%.0f%% heat=%u%% fan=%u "
                "fanOut=%u%% bat=%.2fV %u%%%s wt=%.0fg/%.1fg/min door:%s/%s pwr:%s %s out:%s clk:%s deg:%s | miss:%s | fault:%s\n",
                stateName(dryer.state()),
                dryer.boosting() ? " (MAX heat-up)" : "",
                sensors.t1(), sensors.t2(), sensors.h1(), sensors.h2(),
                dryer.heatDuty(), dryer.fanDuty(),
                dryer.fanOutDuty(),
                battery.volts(), battery.percent(),
                by ? " BYPASS" : "",
                scale.ok() && !isnan(scale.grams()) ? scale.grams() : 0.0f,
                scale.ok() && !isnan(scale.rate())  ? scale.rate()  : 0.0f,
                door::phase(), door::locked() ? "LCK" : "open",
                supply::modeName(),
                cfg.mode == 0 ? "AGARBATTI" : cfg.mode == 2 ? "SILICAGEL" : "USER",
                (dht::outdoor.ok && !isnan(dht::outdoor.t))
                  ? [](){ static char ob[14]; snprintf(ob, sizeof(ob),
                      "%.0fC/%.0f%%", dht::outdoor.t, dht::outdoor.h); return ob; }()
                  : "--",
                clkStr(),
                dryer.warnActive() ? eName(dryer.warnRec().code) : "-",
                miss.c_str(),
                dryer.faultWhy()[0] ? dryer.faultWhy() : "none");
}

// common bring-up for both modes
static void initSystem() {
  Serial.begin(115200);

  // 0. v2.0: keep the soft-latch closed - we must assert POWER_HOLD
  //    within milliseconds of boot or the pillar powers itself down
  supply::begin();

  // 1. loads OFF during boot; every pin the firmware does not use is
  //    parked in a defined state (never floating, never half-driven)
  pinsUnusedSafe();

#ifdef BT_RELEASE_SUPPORTED
  // 1a. v2.0.18: BLE is never started in this firmware - release the
  //     reserved controller + stack RAM (lower power, more free heap).
  esp_err_t btE = esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
  if (btE != ESP_OK)                 // BLE-only chips (S3) want this mode
    btE = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  esp_bt_mem_release(ESP_BT_MODE_BTDM);
  Serial.printf("[pwr] BLE OFF - controller + stack RAM released (%s)\n",
                esp_err_to_name(btE));
#endif

  // 1b. relay outputs (if fitted) OFF
#if RELAYS_ENABLED
  pinMode(PIN_LOAD_RELAY, OUTPUT);
#if RELAY_ACTIVE_LOW
  digitalWrite(PIN_LOAD_RELAY, HIGH);
#else
  digitalWrite(PIN_LOAD_RELAY, LOW);
#endif
#endif

  printBanner();
  delay(200);

  // 2. settings + subsystems
  cfg = loadSettings();
  Serial.printf("[cfg] setTemp=%.1fC max=%.0fC RH %0.f-%0.f%% time=%umin\n",
                cfg.setTemp, cfg.maxTemp, cfg.humLow, cfg.humHigh,
                (unsigned)cfg.dryMinutes);

  sensors.begin();
  Serial.printf("[sens] S1(top)=%s  S2(bottom)=%s\n",
                sensors.s1ok() ? "OK" : "MISSING",
                sensors.s2ok() ? "OK" : "MISSING");

  battery.begin();
  battery.setType(cfg.battType);
  Serial.printf("[batt] %.2f V (%u%%)\n", battery.volts(), battery.percent());

  dryer.begin(cfg);

  // 3. cycle history (LittleFS) - first boot formats, takes a few seconds
  cyclelog::begin();
  Serial.println("[hist] cycle storage ready");

#if BTN_ENABLED
  pinMode(PIN_BTN, INPUT_PULLUP);        // BOOT button = start/stop
  Serial.println("[btn] BOOT button armed (short=start/stop, 2s=power on)");
#endif

  // 3b. buzzer (if fitted) + keypad + display + weigh scale (if fitted)
  buzzer.begin();
  bz::powerOn();                 // v2.0: 2 beeps/s for 3 s at power-on
  keypad.begin(Wire, KEYPAD_ADDR);
  Serial.printf("[keypad] PCF8574 @0x%02X %s\n", KEYPAD_ADDR,
                keypad.ok() ? "found" : "not found (optional)");
  display::begin();
  txdisp::begin();                       // display bridge -> companion Arduino
  dht::begin();                          // DHT22 return + DHT11 outdoor
  pixel::begin();                        // RGB status pixel (GPIO48)
  scale.setFactor(cfg.scaleCal);          // persisted calibration + tare
  scale.setOffset(cfg.scaleOffset);
  scale.begin();
  scale.setFactor(cfg.scaleCal);          // begin() leaves these untouched;
  scale.setOffset(cfg.scaleOffset);       //  order-safe re-assert
  door::begin(false);                     // pins up, lock stays LOW (init window)

  // ---- hardware self-test summary (everything, one glance) -------------
  Serial.println(F("[diag] ---------- self-test ----------"));
  Serial.printf("[diag] AHT10#1 %s  %s#2 %s  keypad %s  scale %s%s  DHT11-out %s\n",
                sensors.s1ok() ? "OK" : "MISSING",
                SENS2_DHT ? "DHT22" : "AHT10",
                sensors.s2ok() ? "OK" : "MISSING",
                keypad.ok()   ? "OK" : "absent",
                scale.ok()    ? "OK" : "absent",
                door::calibrated() ? " (calibrated)" : " (NOT calibrated)",
                dht::outdoor.ok ? "OK" : "absent");
  Serial.printf("[diag] battery %s (%.2fV)  door %s%s  display %s  clock %s  heap %u kB\n",
                battery.valid() ? "OK" : "ADC NOT reading",
                battery.volts(),
                door::fitted() ? "fitted, " : "software workflow, ",
                door::closed() ? "closed" : "OPEN",
                "up",
                clkStr(),
                (unsigned)(ESP.getFreeHeap() / 1024));
  Serial.println(F("[diag] --------------------------------"));
  // owner-spec boot indications (v2.0.15)
  if (cyclelog::fileCount() >= CYCLE_MAX_FILES - 4)
    bz::play(BP::STORE_FULL);                    // #30: >=90 % of history
  if (cyclelog::cycleNo() > 1 && cyclelog::cycleNo() % 50 == 0)
    bz::play(BP::MAINT);                         // #33: every 50 cycles
  if (!sensors.s1ok())
    bz::play(BP::INIT_FAIL);                     // #34: the top sensor!
  Serial.printf("[diag] supply %s (toggle)  opto %s  button-1 %s\n",
                supply::modeName(),
                PIN_SUPPLY_OPTO >= 0 ? "fitted" : "absent",
                supply::latched() ? "latch ok" : "no latch (USB power?)");
#if RTC_ENABLED
  rtc::begin();                    // detect the DS1307 (I2C0 @ 0x68) BEFORE
  Serial.printf("[diag] %s\n", rtc::statusText());   // reporting it
#endif

  // v2.0: INITIALISATION WINDOW - every output stays LOW, the splash
  // (ARCHITECTS OF SOLUTIONS / SMART DEHUMIDIFIER) holds, the power-on beeps
  // play out; the user can read the self-test before anything energises.
  Serial.printf("[init] ALL PINS LOW for %u s - initialisation + calibration\n",
                (unsigned)(BOOT_INIT_MS / 1000UL));
  {
    uint32_t t0 = millis();
    while (millis() - t0 < BOOT_INIT_MS) {
      // initialisation runs INSIDE the window: first sensor readings,
      // battery sample, scale detection with its saved calibration,
      // buzzer pattern + splash frames - all with the loads quiet
      sensors.update();
      battery.update();
      scale.update();
      dht::update();       // first DHT frames land inside the window
      buzzer.update();
      display::update();          // splash frames (also keeps beeping)
      delay(20);
    }
  }
  // window over: engage per state - supply relay follows the toggle;
  // the (optional) lock would close here; the start gate is software
  supply::engage();
  door::engage();
  display::splash(false);         // hand over to the main screen
  Serial.println(F("[init] initialisation + calibration checks complete - starting"));

  // 3c. DS1307 RTC (v2.0.23; DS1302 in v2.0.21-22): coin-cell date & time
  // that survives a FULL power-down. Detected in the self-test above;
  // restored here, BEFORE web::begin(), so a good RTC time wins over the
  // (stale) NVS restore - clockBoot() then sees a live clock and backs
  // off. No chip / untrusted time = today's phone-sync clock.
#if RTC_ENABLED
  {
    time_t rtcEp = 0;
    if (rtc::readTime(&rtcEp)) {
      web::clockSetManual(rtcEp);        // set + flag synced + NVS backup
      Serial.printf("[rtc] %s -> system clock set\n", rtc::statusText());
    } else {
      Serial.printf("[rtc] %s\n", rtc::statusText());
    }
  }
#endif

  // 4. hotspot + website
  web::begin();

#if OTA_NETWORK_ENABLED
  // 4b. network OTA: Arduino IDE -> Tools > Port -> "SMART-DEHUMIDIFIER at
  //     192.168.4.1" (PC must join WiFi "AgarbattiDryer"). Website OTA
  //     stays available at http://192.168.4.1/update either way.
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([]() { Serial.println(F("[ota] network update started")); });
  ArduinoOTA.onEnd([]() { Serial.println(F("[ota] update complete - rebooting")); });
  ArduinoOTA.onProgress([](uint32_t done, uint32_t total) {
    if (total == 0) return;
    static uint8_t lastPct = 255;
    uint8_t pct = (uint8_t)(done * 100UL / total);
    if (pct != lastPct && pct % 10 == 0) {
      lastPct = pct;
      Serial.printf("[ota] %u%%\n", (unsigned)pct);
    }
  });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[ota] error %u\n", (unsigned)e); });
  if (!MDNS.begin(OTA_HOSTNAME)) Serial.println(F("[ota] mDNS failed - use the website OTA"));
  else                           MDNS.addService("http", "tcp", 80);
  ArduinoOTA.begin();
  Serial.printf("[ota] network OTA ready - Arduino IDE: Tools > Port > %s at 192.168.4.1\n",
                OTA_HOSTNAME);
#endif

  // 5. energise the loads -> IDLE
  dryer.powerOn();
#if AUTO_START
  Serial.printf("[main] ready - AUTO-START in %u s (setTemp %.1fC, %u min)\n",
                (unsigned)(AUTO_START_DELAY_MS / 1000UL),
                cfg.setTemp, (unsigned)cfg.dryMinutes);
#else
  Serial.println("[main] ready - connect to AP and press Start (or: serial 'help')");
#endif
}

/* =====================================================================
 *  MODE 0 (default): cooperative loop
 * ===================================================================== */
#if !DRYER_RTOS

void setup() {
  initSystem();
}

void loop() {
  web::handle();       // website + captive DNS
#if OTA_NETWORK_ENABLED
  ArduinoOTA.handle(); // network upload from the Arduino IDE
#endif
  sensors.update();    // AHT10 x2, non-blocking
  battery.update();    // ADC sample every 5 s
  dryer.tick();        // 1 s control cadence: PID, fans, state, safety
  char k = keypad.update();   // hex keypad (optional, 15 ms debounce)
  if (k) dryerKey(k);
  scale.update();      // weigh scale (2 Hz reads, internally gated)
  dht::update();       // DHT22 return (3 s) + DHT11 outdoor (10 s)
  door::update();      // lock/reed workflow (calibrate -> load -> ready)
  supply::update();    // power button + solar/bypass selector
  serviceSerial();     // USB/serial command console
  serviceButton();     // BOOT button (S3): start/stop
  buzzer.update();     // finish any beep pattern in progress
  pixel::update();     // status pixel colour (10 Hz)
  display::update();    // 3.5" status screen (1 Hz, internally gated)
  txdisp::update();    // serial display bridge (1 Hz packet burst)
  maybeAutoStart();    // full automation: start by itself at power-up
  maybeDiag();         // serial status + missing/fault warnings
}

/* =====================================================================
 *  MODE 1 (optional): FreeRTOS task architecture
 * ===================================================================== */
#else

// Wire (GPIO21/22) serves AHT10 #1; the mutex keeps future I2C additions
// safe if they ever share the bus (each update() performs at most one
// short transaction burst).
static SemaphoreHandle_t wireMutex = nullptr;

// ---- sensor task: 2 x AHT10 (non-blocking SM) --------------------------
static void taskSensors(void *) {
  for (;;) {
    if (xSemaphoreTake(wireMutex, pdMS_TO_TICKS(100))) {   // AHT10 #1 = Wire
      sensors.update();                   // internally 2 s per sensor
      xSemaphoreGive(wireMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ---- control task: 1 s laws + safety + logging -------------------------
static void taskControl(void *) {
  for (;;) {
    dryer.tick();                         // internally 1 s gated
    scale.update();                       // weigh scale (2 Hz)
    dht::update();                        // DHT sensors (3 s / 10 s)
    door::update();                       // door workflow + safety
    maybeAutoStart();                     // full automation at power-up
    maybeDiag();                          // serial status + warnings
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// ---- web task: HTTP + captive DNS (core 0, next to the WiFi stack) ----
static void taskWeb(void *) {
  for (;;) {
    web::handle();
#if OTA_NETWORK_ENABLED
    ArduinoOTA.handle();                  // network OTA (IDE upload)
#endif
    serviceSerial();                      // USB/serial command console
    vTaskDelay(pdMS_TO_TICKS(2));         // breathe; WDT-safe
  }
}

// ---- power task: battery ADC + bypass/threshold housekeeping ----------
static void taskPower(void *) {
  for (;;) {
    battery.update();                     // internally 5 s gated
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ---- panel task: keypad scan (Wire!) + buzzer + display ----------------
static void taskPanel(void *) {
  for (;;) {
    if (xSemaphoreTake(wireMutex, pdMS_TO_TICKS(20))) {
      char k = keypad.update();           // internally 15 ms debounced
      xSemaphoreGive(wireMutex);
      if (k) dryerKey(k);
    }
    buzzer.update();                      // ms-accurate beep edges
    pixel::update();                      // status pixel colour
    display::update();                     // 1 Hz status screen (menu aware)
    txdisp::update();                     // display bridge burst
    serviceButton();                      // BOOT button (S3)
    supply::update();                     // power button + supply selector
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  initSystem();

  wireMutex = xSemaphoreCreateMutex();

  //                fn            name        stack  param      prio | handle | core
  xTaskCreatePinnedToCore(taskWeb,     "web",     8192, nullptr, 3, nullptr, 0);
  xTaskCreatePinnedToCore(taskSensors, "sensors", 4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(taskControl, "control", 4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(taskPanel,   "panel",   3072, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(taskPower,   "power",   3072, nullptr, 1, nullptr, 1);

  Serial.printf("[rtos] 5 tasks up (web@core0, rest@core1), mode=DRYER_RTOS\n");
}

// Arduino loopTask idles; all work happens in the tasks above.
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

#endif  // DRYER_RTOS
