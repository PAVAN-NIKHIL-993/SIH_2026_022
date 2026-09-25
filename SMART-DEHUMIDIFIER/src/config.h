/**
 * @file config.h
 * @brief Central pin map + compiled-in DEFAULT thresholds for the
 *        Smart Dehumidifier (ESP32 + BTS7960 + L298N + 2x AHT10).
 *
 * Everything in the "DEFAULTS" section can be overridden from the website
 * (slide 2 - Custom). The values here are what slide 1 ("Defaults") shows
 * and what gets restored when the user presses "Apply defaults".
 *
 * PIN RULES OBSERVED
 *  - No ADC2 pins while WiFi is on (ADC2 is disabled by the WiFi driver),
 *    so battery sense uses GPIO36 (ADC1_CH0).
 *  - AHT10 has a FIXED I2C address (0x38): two sensors cannot share one
 *    bus, so sensor #1 uses Wire (21/22) and sensor #2 uses Wire1 (25/26).
 *  - L298N ENB (the one pin that energises the fan) is on a
 *    non-strapping pin, so no fan twitch at boot.
 *  - GPIO2 (bypass) carries the on-board LED on most DevKit boards, so
 *    the LED doubles as a "bypass active" indicator.
 */

#pragma once

// =====================================================================
//  PRODUCT IDENTITY (boot splash + website header)
// =====================================================================
#define BRAND_NAME   "ARCHITECTS OF SOLUTIONS"
#define PRODUCT_NAME "SMART DEHUMIDIFIER"
#define BOOT_INIT_MS 10000     // all outputs LOW -> splash + self-test +
                               // power-on beeps for 10 s, then the main screen

// =====================================================================
//  WIFI ACCESS POINT (the ESP32 makes its own hotspot - no internet,
//  no interruptions). Browse to http://192.168.4.1
// =====================================================================
// Firmware version - shown on the serial banner, the website footer and
// /api/data; bump it on every release (OTA makes versions matter).
#define FW_VERSION      "2.0.21"

#define AP_SSID "AgarbattiDryer"
#define AP_PASS "dryer1234"          // min 8 chars
#define AP_CHANNEL 6
#define AP_MAX_CLIENTS 4

// OPTIONAL online UI: the dashboard is also hosted on GitHub Pages and
// loaded through http://192.168.4.1/online (tiny bridge page on the ESP
// relays API calls, because browsers block https->http directly).
// Repo Settings -> Pages -> Source: "GitHub Actions" (.github/workflows/
// pages.yml publishes SMART-DEHUMIDIFIER/docs on every push to main), then
// put the site URL here. No internet -> /online falls back to the built-in UI.
#define ONLINE_UI_URL "https://pavan-nikhil-993.github.io/SIH_2026_022/"

// =====================================================================
//  PIN MAP  (ESP32 DevKit V1 - 30 pin)
// =====================================================================
// ---- AHT10 sensors (fixed 0x38 address -> two separate I2C buses) ----
#define PIN_I2C0_SDA 21              // sensor #1 (top of chamber)
#define PIN_I2C0_SCL 22
#define PIN_I2C1_SDA 32              // sensor #2 (bottom of chamber)
#define PIN_I2C1_SCL 33

// ---- Heater coil: PILLAR CLASS 500 W ----------------------------------
// 12.6 V full battery across 0.32 ohm nichrome (two 0.64 ohm halves in
// parallel, ~2.3 m of 1.0 mm wire) = 500 W peak, 41.7 A through the
// BTS7960 (43 A rated - heatsink mandatory). The 70 % duty cap keeps
// CONTINUOUS draw at ~350 W inside the 33 A PSU; the 550 W solar panel
// carries the full 500 W at peak sun with the battery buffering clouds.
// ---- BTS7960 : heating coil driver (matched to the built board) -----
// Physical build: ESP32 -> 555 timer level shifter (3.3 V -> 5 V PWM,
// RESET-pin trick) -> RPWM; LPWM hard-tied to GND at the module.
#define PIN_BTS_RPWM 25              // 1 kHz PWM -> 555 pin 4 -> 555 pin 3 -> RPWM
#define PIN_BTS_LPWM -1              // tied to GND at the module (-1 = not a GPIO)
#define PIN_BTS_EN   26              // R_EN + L_EN of the module tied together

// ---- L298N : ONE outlet fan (v2.0: the intake fan was removed) -------
// The system has a single EXHAUST fan: RH > fanTrigRH for fanTrigMin
// minutes -> the fan runs at 100% for fanBurstS seconds (burst venting).
// ENA/IN1/IN2 (the old intake channel) are free -> see the buttons and
// buzzer blocks below.
#define PIN_L298_ENA -1              // (intake channel removed)
#define PIN_L298_IN1 -1
#define PIN_L298_IN2 -1
#define PIN_L298_ENB 14              // outlet (exhaust) fan PWM - the
                                     // ONLY wired L298N pin (v2.0.10)
#define PIN_L298_IN3 -1              // direction HARD-WIRED on the
#define PIN_L298_IN4 -1              // module: IN3 -> 5V, IN4 -> GND.
                                     // REMOVE the ENB jumper cap; add
                                     // 10k ENB->GND (fan off at boot).
                                     // GPIO 5 + 4 spare again.
#define FAN_FIXED_DIR  1             // ENB-only PWM (no IN-pin drive)
#define FAN_PWM_FLOOR  40            // enable-PWM below ~40% just hums

// ---- Battery sense ---------------------------------------------------
#define PIN_VBAT_ADC    36           // ADC1_CH0 (VP), input-only, WiFi-safe
#define PIN_VBAT_ENABLE 19           // gates the divider's low-side transistor
                                     // (pulls the divider down only while
                                     // sampling). Set to -1 if the divider
                                     // is hard-wired to GND.
// Divider: panel/battery(+) --[Rtop]--+--> ADC pin
//                                      |
//                                    [Rbot]
//                                      |
//                  PIN_VBAT_ENABLE --[N-MOSFET 2N7000 / NPN]-- GND
// Defaults: Rtop = 100 k, Rbot = 15 k  -> RATIO = (100+15)/15 = 7.667
// (keeps 4S Li-ion 16.8 V at ~2.19 V on the ADC - inside the ~2.45 V
//  linear window of the ESP32 ADC at 11 dB attenuation)
#define VBAT_DIV_RTOP   100000.0f
#define VBAT_DIV_RBOT   15000.0f
#define VBAT_DIV_RATIO  ((VBAT_DIV_RTOP + VBAT_DIV_RBOT) / VBAT_DIV_RBOT)
#define VBAT_ADC_REF    3.30f        // nominal; calibrate if you have a DMM
#define VBAT_CAL_OFFSET 0.0f         // volts, added to the computed value

// ---- Power path + supply selector (v2.0) -------------------------------
// The chamber is fed from EITHER the solar/battery rail (solar mode) or
// the bypass supply (bypass mode) through a 2-channel relay; an optocou-
// pler input verifies which supply is actually live. The MCU switches
// the relay ONLY with the loads off (heater+fan quiet) - a mode change
// requested while a supply is under load = refused + error beep.
#define PIN_LOAD_RELAY  23           // (display-shared on classic: unused)
#define PIN_BYPASS_CTRL 2            // (display-shared on classic: unused)

// 2-channel supply relay (classic has no free outputs for it - the mode
// is reported and verified on screen/site, switching is manual there;
// the S3 variant switches it automatically on GPIO 6/7).
#define PIN_SUPPLY_CH1   -1          // relay CH1: solar feed enable
#define PIN_SUPPLY_CH2   -1          // relay CH2: bypass feed enable
#define SUPPLY_RELAYS_ENABLED 0

// ---- DHT sensors: OFF on classic (v2.0.9) ------------------------------
// S3 layout: chamber #2 = DHT22 (cool return) + DHT11 outdoor. Classic
// keeps its 2x AHT10 (SENS2_DHT 0) and has no spare GPIO for a DHT11.
#define SENS2_DHT           0     // 1 = chamber source #2 is a DHT22
#define DHT_CHAMBER_ENABLED 0
#define PIN_DHT22_CHAMBER   -1
#define DHT_CHAMBER_MS      3000
#define DHT_OUT_ENABLED     0
#define PIN_DHT11_OUT       -1
#define DHT_READ_MS         10000

// Inputs (work on BOTH variants):
#define PIN_SOLAR_TOGGLE  27         // physical toggle: SOLAR requested
#define PIN_SUPPLY_OPTO   35         // optocoupler: selected supply live
                                     // (input-only GPIO - perfect)
#define OPTO_ACTIVE_HIGH  1          // opto output HIGH = supply present

// Master power button + soft-latch (v2.0 hard power-off):
// press BUTTON-1 -> P-MOSFET latch feeds the MCU -> firmware asserts
// PIN_POWER_HOLD within ms of boot. BUTTON-1 held 3 s = firmware drops
// the hold -> the WHOLE system loses power (hard off). Held 10 s =
// reboot. See manual 02 sect.4.11 for the latch circuit.
#define PIN_BTN1        15           // master power button sense (input)
#define PIN_BTN2        18           // "default automation" button
#define PIN_POWER_HOLD  16           // keeps the P-MOSFET latch closed
#define BTN1_OFF_MS     3000         // held this long -> hard power off
#define BTN1_RESET_MS   10000        // held this long -> reboot instead

// Relay polarity: most green modules are ACTIVE-HIGH, most blue modules
// are ACTIVE-LOW. Set accordingly so the load is OFF at boot.
#define RELAY_ACTIVE_LOW false

// =====================================================================
//  PWM
// =====================================================================
#define HEATER_PWM_FREQ 1000         // Hz  (BTS7960 switches this easily)
#define FAN_PWM_FREQ    1000         // Hz
#define PWM_RES_BITS    10           // 0..1023 duty

// =====================================================================
//  TIMING
// =====================================================================
#define SENSOR_PERIOD_MS   2000      // AHT10 polling interval
#define AHT10_HOT_C        82.0f     // warn: sensor near its 85 C max
#define CONTROL_PERIOD_MS  1000      // PID / fan law / state machine tick
#define BATT_PERIOD_MS     5000      // battery sampling interval
#define LOG_PERIOD_MS      10000     // one CSV record every 10 s
#define LOG_MAX            2160      // 6 h of records in RAM (16 B each)
#define SENSOR_FAIL_GRACE  15000     // both sensors dead this long -> fault

// =====================================================================
//  DEFAULTS  (shown on slide 1 of the website, applied with one tap)
//  These values are tuned for agarbatti / incense-stick drying.
// =====================================================================
#define DEF_SET_TEMP     60.0f   // deg C   chamber target (v2.0: agarbatti
                                     //   default 60; 80 is the parameter
                                     //   ceiling, 95 the hard safety cut)
#define DEF_TEMP_HYST    1.5f    // deg C   PID smooth band (informational)
#define DEF_MAX_TEMP     95.0f   // deg C   HARD safety cut (chamber rated
                                     //   to 100C; AHT10s must sit in the cool
                                     //   return path above 85C - see datasheet)
#define DEF_HUM_LOW      40.0f   // %RH     below this: fans stop (retain heat,
                                 //         don't over-dry the sticks)
#define DEF_HUM_HIGH     60.0f   // %RH     above this: fans ramp up to vent
                                 //         moist air
#define DEF_HUM_TARGET   35.0f   // %RH     optional "dry enough" criterion
#define DEF_REQUIRE_HUM  false   // true: cycle also waits for humTarget
#define DEF_DRY_MINUTES  120     // manual drying time (user sets on slide 2)
#define DEF_FAN_MIN      0       // %       continuous floor (0 = bursts only)
#define DEF_FAN_IN       100     // %       (intake fan removed in v2.0)
#define DEF_FAN_OUT      100     // %       outlet fan burst speed (100 %)
#define DEF_FAN_SLOPE    6       // %/RH    (legacy continuous law, unused)
#define DEF_FAN_TRIG_RH  60      // %RH     RH at/above -> burst timer starts
#define DEF_FAN_TRIG_MIN 1       // min     RH high this long -> fan fires
#define DEF_FAN_BURST_S  60      // s       fan run time per burst (100 %)
#define DEF_TARGET_G     0.0f    // g       target batch weight (0 = off);
                                 //         within 5% of it at time-up =
                                 //         complete, else DONE-WITH-WARNING
#define DEF_HEATER_MAX   100     // %       full 500 W (BTS7960 heatsink!)
#define DEF_COOLDOWN_S   45      // s       purge fans before power is cut
#define DEF_BYPASS_PCT   20      // %       battery at/below -> bypass ON
#define DEF_CUTOFF_PCT   10      // %       battery at/below -> safe shutdown
#define DEF_BATT_TYPE    3       // 0=3S Li-ion 1=4S Li-ion 2=12V SLA 3=4S LiFePO4 (product default: longevity + safety)
                                 // 3=4S LiFePO4
#define DEF_TZ_MINUTES   330     // local offset from UTC in minutes (330=IST)
#define DEF_SMART_VENT   true    // pause venting when outside air is wetter
                                 // than the chamber (uses weather data)
#define DEF_BOOST_HEAT   true    // BTS at MAX output until the chamber
                                 // reaches setTemp (minus the band), then
                                 // PID holds it there
#define DIAG_PERIOD_MS   15000   // serial status line interval
#define TIME_SAVE_MS     (30UL*60UL*1000UL)  // persist wall clock to NVS
                                             // (restored at boot if the
                                             // battery was disconnected)

// Web-knob manual heat override: exact BTS duty for this long, then the
// MCU returns to automatic control (boost / PID) by itself.
#define MANUAL_HEAT_MS   60000UL // 60 s manual window (re-armed on every turn)

// =====================================================================
//  WEATHER (relayed by the phone's browser -> ESP, ESP has no internet)
// =====================================================================
#define WX_STALE_MS      (2UL*60UL*60UL*1000UL)  // data older than 2 h = stale

// ---- OTA firmware updates ------------------------------------------------
// (a) WEBSITE OTA (always on): any browser on the dryer hotspot ->
//     http://192.168.4.1 -> "Firmware update" -> pick the .bin -> Update.
//     Refused while a cycle is RUNNING (stop it first).
// (b) NETWORK OTA (Arduino IDE): the PC joins WiFi "AgarbattiDryer" and
//     Arduino IDE can upload over WiFi directly - Tools > Port >
//     "SMART-DEHUMIDIFIER at 192.168.4.1". Uses only core built-ins (no extra
//     libraries to install). Set 0 to compile it out.
#define OTA_NETWORK_ENABLED 1
#define OTA_HOSTNAME "SMART-DEHUMIDIFIER"

// ---- Buzzer (active module, e.g. KY-012) ---------------------------------
// MANDATORY since v2.0 (full beep-pattern set). Classic: GPIO13 (the pin
// freed by removing the intake fan); S3: GPIO38.
#define PIN_BUZZER       13
// ---- RGB status pixel: off on classic (SPI bus + no on-board LED) -----
#define PIXEL_ENABLED  0
#define PIN_PIXEL      -1
#define PIXEL_COUNT    1
#define BUZZER_ACTIVE_HIGH 1
#define BUZZER_ENABLED   1

// ---- Relays: NOT FITTED in the current build --------------------------
// 0 = no load relay and no bypass relay wired; the cycle simply ends with
//     heater + fans off (DONE). Flip to 1 the day you fit the modules.
#define RELAYS_ENABLED   0

// ---- Full automation: start drying by itself at power-up -------------
// true = plug in -> (delay) -> RUNNING automatically, one cycle per boot.
// false = MANUAL (default): press Start on the website and change any
//         parameter live there - NOT plug-and-play.
#define AUTO_START            false
#define AUTO_START_DELAY_MS   15000UL   // grace for sensors/battery/WebAP

// ---- OPTIONAL RTOS architecture ---------------------------------------
// 0 = cooperative loop (default): one simple loop(), deterministic,
//     no shared-bus contention, easiest to debug.
// 1 = full FreeRTOS task architecture: sensor/control/web/power/panel
//     tasks with priorities + Wire mutex (web pinned to core 0).
//     See docs/manual/08-RTOS-ARCHITECTURE.md. Enable here or with
//     build_flags -DDRYER_RTOS=1 (platformio.ini) in Arduino IDE:
//     Tools > Erase... no - just set it to 1 here and re-flash.
#ifndef DRYER_RTOS
#define DRYER_RTOS 0
#endif

// ---- TFT status display: REMOVED from this build (v2.0.7) -------------
// The WEBSITE is the display: "/" = full control, "/display" = the kiosk
// screen (a mounted phone/tablet becomes the hardware display).
// Set 1 to bring the ILI9488 screen back (pins: SCK 12, MOSI 0, CS 2,
// DC 17, RST 23 - the slots shared with relays/buzzer).
#define DISPLAY_ENABLED  0
#define PIN_TFT_SCK      12          // SPI clock (any output GPIO)
#define PIN_TFT_MOSI     0           // SPI data
#define PIN_TFT_CS        2          // chip select
#define PIN_TFT_DC       17          // data/command
#define PIN_TFT_RST      23          // reset
#define TFT_SPI_HZ       26000000    // 26 MHz (GPIO-matrix safe)
// If colours look wrong/swapped: change 0x48 (MADCTL) or remove INVON.

// ---- Serial display bridge (v2.0.3) ------------------------------------
// Streams the status screen to a companion Arduino driving a PARALLEL
// "UNO-shield" TFT (manual 02 sect.4.12). The classic ESP32 has NO free
// output pin for the bridge TX -> keep 0 (the website + keypad remain
// the UI). With DISPLAY_ENABLED 0 you could reuse pin 12/0/17/23.
#define TXDISP_ENABLED  0
#define PIN_TXDISP_TX   -1
#define TXDISP_BAUD     9600

// ---- DS1302 RTC: NOT fitted on the classic build (v2.0.21) -------------
// The classic 30-pin devkit has no free 3-wire slot; the date/time there
// stays phone-sync + NVS. The S3 variant carries the DS1302 (RST 40 /
// SCLK 42 / I-O 47) - see variants/esp32-s3/config-s3.h.
#define RTC_ENABLED    0
#define PIN_RTC_RST    -1
#define PIN_RTC_SCLK   -1
#define PIN_RTC_IO     -1

// ---- Weigh scale: 2 x half-bridge load cells + HX711 ("dry to weight") -
// OFF on the classic ESP32: with the display fitted there is NO free
// output pin for the HX711 clock. Your options:
//   a) ESP32-S3 variant (recommended): scale on GPIO 1 (CLK) + 2 (DOUT),
//      enabled there by default - see variants/esp32-s3/
//   b) DISPLAY_ENABLED 0 here -> set SCALE_ENABLED 1, CLK=12, DOUT=34
//   c) RELAYS stay un-used slots only if display is off (same pin pool)
#define SCALE_ENABLED   0
#define PIN_SCALE_CLK   12          // (unused when disabled)
#define PIN_SCALE_DOUT  34          // input-only pin - perfect for DOUT

// ---- Door lock + sensor (the calibrate->load->ready workflow) ----------
// Classic ESP32 has NO free pins left for a lock/reed -> the workflow
// runs fully in SOFTWARE (start refused everywhere until the scale is
// calibrated). The S3 variant has the hardware: reed GPIO21 (lock slot 41).
#define DOOR_ENABLED       0
#define PIN_DOOR_LOCK      -1          // (unused when disabled)
#define PIN_DOOR_REED      -1
#define DOOR_CLOSED_LEVEL  LOW         // switch closed = LOW
#define DOOR_LOCK_ACTIVE   HIGH        // solenoid energized = locked
#define DOOR_LOCK_ENABLED  0           // no lock fitted (limit switch only)

// ---- 16-key hex keypad on a PCF8574 I2C backpack ----------------------
// Shares Wire (GPIO21/22) with AHT10 #1 - no conflict (keypad 0x20..0x26,
// AHT10 0x38). Buy PCF8574, NOT PCF8574A (0x38 = AHT10 collision!).
// v2.0 key map (full on-device menu, see display.cpp):
//   2=UP  4=LEFT  6=RIGHT  8=DOWN   1/3/5/7/9/0 = digits (in edit screens)
//   * = HOME     # = BACK              A = ENTER/OK
//   B = MENU     C = MODE (AGARBATTI -> USER -> SILICAGEL)   D = RUN
#define KEYPAD_ENABLED   1
#define KEYPAD_ADDR      0x20

// =====================================================================
//  CYCLE HISTORY (LittleFS - each drying cycle saved as its own file)
// =====================================================================
#define CYCLE_DIR        "/cycles"
#define CYCLE_MAX_FILES  40      // oldest files auto-deleted beyond this

// ---- AT24C256 32 kB I2C EEPROM: long-term cycle registry (v2.0.17) ----
// Summaries of EVERY cycle (~817 slots) outlive the LittleFS files.
// Wiring: VCC->3V3, GND->GND, SDA/SCL on I2C0 (S3: 8/9), A0/A1/A2->GND
// = 0x50. Auto-detected at boot; absent chip = registry simply off.
#define ELOG_ENABLED  1
#define ELOG_ADDR     0x50
#define DEF_KP           10.0f   // heater PID (duty-% per deg C)
#define DEF_KI           0.2f    // duty-% per (deg C * s)
#define DEF_KD           5.0f    // duty-% per (deg C / s)
