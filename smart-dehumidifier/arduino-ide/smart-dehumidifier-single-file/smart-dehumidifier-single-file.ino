/*
 * ============================================================================
 *  SMART DEHUMIDIFIER - SINGLE-FILE SKETCH (whole project, one file)
 * ============================================================================
 *  Generated from the multi-file sources by tools/single-file/assemble.js -
 *  do not edit by hand; edit src/ and regenerate.
 *
 *  VARIANT: classic ESP32 DevKit V1  (board "ESP32 Dev Module")
 *
 *  ARDUINO IDE 2 - HOW TO FLASH (no libraries needed, seriously):
 *    1. Boards Manager (icon left) -> search "esp32" -> install
 *       "esp32 by Espressif Systems".
 *    2. Tools -> Board -> esp32 -> "ESP32 Dev Module".
 *    3. Plug the ESP32 with a USB DATA cable. Tools -> Port -> select it.
 *    4. Click Upload (->). If it hangs on "Connecting...", hold the BOOT
 *       button on the ESP32 until "Writing..." starts, then release.
 *    5. Serial Monitor at 115200 shows the boot log.
 *    6. Phone -> WiFi "AgarbattiDryer" / "dryer1234" -> open
 *       http://192.168.4.1
 *
 *  PIN QUICK-REFERENCE (extracted from the config at build time):
 *    GPIO21   I2C0 SDA  - AHT10 #1 + keypad backpack
 *    GPIO22   I2C0 SCL
 *    GPIO32   I2C1 SDA  - AHT10 #2 (own bus, fixed 0x38)
 *    GPIO33   I2C1 SCL
 *    GPIO25   BTS7960 RPWM (via the 555 level shifter)
 *    GPIO26   BTS7960 R_EN + L_EN (jumpered)
 *    GPIO14   L298N ENB - the ONE outlet fan PWM
 *    GPIO36   battery divider -> ADC
 *    GPIO27   SOLAR/BYPASS toggle (HIGH = solar)
 *    GPIO35   supply optocoupler (HIGH = feed live)
 *    GPIO16   P-MOSFET latch hold (hard power-off)
 *    GPIO15   BUTTON-1: 3 s hard off / 10 s reboot
 *    GPIO18   BUTTON-2: default automation
 *    GPIO13   buzzer KY-012
 *    GPIO12   ILI9488 TFT SCK
 *    GPIO 0   ILI9488 TFT MOSI
 *    GPIO 2   ILI9488 TFT CS
 *    GPIO17   ILI9488 TFT DC
 *    GPIO23   ILI9488 TFT RST
 *
 *    NOTE: weigh scale OFF on this variant (config: SCALE_ENABLED)
 *    NOTE: door hardware OFF - the calibrate->load->ready workflow runs in software
 *    NOTE: supply relay not wired - mode shown + opto verified, switching is manual
 *    NOTE: keypad PCF8574 at 0x20 (never PCF8574A - AHT10 clash)
 *
 *  WHAT'S INSIDE (v2.0):
 *    - BOOT: all outputs LOW, splash + power-on beeps, 10 s init window
 *    - modes: AGARBATTI 60C / USER DEFINED / SILICAGEL 80C (C key,
 *      BUTTON-2, website); setTemp clamped 40-80 C, hard cut 95 C
 *    - door-gated workflow: LOCKED until the scale is calibrated ->
 *      unlock -> load -> close (batch weighed) -> READY -> Start ->
 *      locked during the cycle (opening = instant FAULT)
 *    - control: full-power heat-up -> PID hold; ONE outlet fan with
 *      BURST venting (RH >= 60 % for 1 min -> 100 % for 60 s);
 *      FAULTs: heater failure (temp flat 3 min), fan error (RH high
 *      5 min), coil overheat, sensor death, battery cutoff
 *    - target weight: initial/target/diff live, warned 5 min before
 *      time-up (+min suggestion), within 5 % at time-up = complete
 *      else DONE-WITH-WARNING
 *    - power: P-MOSFET latch hard off (BUTTON-1 3 s / 10 s reboot),
 *      solar<->bypass toggle + optocoupler check + 2-ch feed relay
 *      (switched only with loads quiet; refused under load = error beep)
 *    - buzzer pattern set: 3 s start, 5 s end, 2/s x 3 s power-on,
 *      5 s errors, 1 s door, 3 s mode change
 *    - on-device menu: B menu, 2/4/6/8 arrows, digits edit, A save,
 *      # back, * home - full status screen incl. cycle graph
 *    - OTA two ways: website "Firmware update" page (.bin upload,
 *      refused while RUNNING) AND Arduino-IDE network upload (join
 *      WiFi AgarbattiDryer -> Tools > Port -> smart-dehumidifier at 192.168.4.1)
 *    - hotspot website: dashboard + graph + knob, Parameters with
 *      one-click defaults, weight tools, history + CSV download,
 *      phone-synced clock, phone-relayed weather
 *    - serial console 115200: type "help"
 *
 *  Full docs: docs/manual/ (02 wiring, 05 errors, 09 flashing)
 *             + docs/PIN-MAP.md (every component x every pin)
 * ============================================================================
 */

/* ==========================  src/config.h  ========================== */
/**
 * @file config.h
 * @brief Central pin map + compiled-in DEFAULT thresholds for the
 *        Smart Dehumidifier (ESP32 + BTS7960 + L298N + 2x AHT10).
 *
 * Everything in the "DEFAULTS" section can be overridden from the website
 * (slide 2 - Custom). The values here are what slide 1 ("Defaults") shows
 * (slide 2 - Custom). The values here are what slide 1 ("Defaults") shows
 * and what gets restored when the user presses "Apply defaults".
 *
 * PIN RULES OBSERVED
 *    so battery sense uses GPIO36 (ADC1_CH0).
 *  - AHT10 has a FIXED I2C address (0x38): two sensors cannot share one
 *    bus, so sensor #1 uses Wire (21/22) and sensor #2 uses Wire1 (25/26).
 *  - L298N ENB (the one pin that energises the fan) is on a
 *    non-strapping pin, so no fan twitch at boot.
 *  - GPIO2 (bypass) carries the on-board LED on most DevKit boards, so
 *    the LED doubles as a "bypass active" indicator.
 */


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
// Enable Pages in repo Settings (main branch, /docs folder) and put the
// URL here. Leave as-is and /online simply falls back to the built-in UI.
#define ONLINE_UI_URL "https://pavan-nikhil-993.github.io/arena/"

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
                                     // ONLY wired L298N pin (v2.0.10)
#define PIN_L298_IN3 -1              // direction HARD-WIRED on the
#define PIN_L298_IN4 -1              // module: IN3 -> 5V, IN4 -> GND.
                                     // REMOVE the ENB jumper cap; add
                                     // 10k ENB->GND (fan off at boot).
                                     // GPIO 5 + 4 spare again.
#define FAN_FIXED_DIR  1             // ENB-only PWM (no IN-pin drive)
#define FAN_PWM_FLOOR  40            // enable-PWM below ~40% just hums

// ---- Battery sense ---------------------------------------------------
//  TIMING
// =====================================================================
#define SENSOR_PERIOD_MS   2000      // AHT10 polling interval
#define AHT10_HOT_C        82.0f     // warn: sensor near its 85 C max
#define CONTROL_PERIOD_MS  1000      // PID / fan law / state machine tick
#define BATT_PERIOD_MS     5000      // battery sampling interval
#define LOG_PERIOD_MS      10000     // one CSV record every 10 s
//                                      |
//                                    [Rbot]
//                                      |
//                  PIN_VBAT_ENABLE --[N-MOSFET 2N7000 / NPN]-- GND
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
// ---- Power path + supply selector (v2.0) -------------------------------
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
//     "smart-dehumidifier at 192.168.4.1". Uses only core built-ins (no extra
//     libraries to install). Set 0 to compile it out.
#define OTA_NETWORK_ENABLED 1
#define OTA_HOSTNAME "smart-dehumidifier"

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

#define BTN1_OFF_MS     3000         // held this long -> hard power off
#define BTN1_RESET_MS   10000        // held this long -> reboot instead
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
//     "smart-dehumidifier at 192.168.4.1". Uses only core built-ins (no extra
//     libraries to install). Set 0 to compile it out.
#define OTA_NETWORK_ENABLED 1
#define OTA_HOSTNAME "smart-dehumidifier"

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
// calibrated). The S3 variant has the hardware: lock GPIO33 + reed GPIO34.
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
/* ==========================  src/pwm.h  ========================== */
/**
 * @file pwm.h
 * @brief Tiny LEDC wrapper that compiles on BOTH Arduino-ESP32 core 2.x
 *        (ledcSetup/ledcAttachPin) and 3.x (ledcAttach), so the sketch
 *        builds in old Arduino IDE, new Arduino IDE and PlatformIO.
 */
#include <Arduino.h>

void pwmInitPin(int pin, uint32_t freqHz, uint8_t resBits);
void pwmWritePin(int pin, uint32_t duty);   // 0 .. (2^resBits - 1)
/* ==========================  src/aht10.h  ========================== */
/**
 * @file aht10.h
 * @brief Minimal, dependency-free, NON-BLOCKING AHT10 driver.
 *
 * The AHT10 has a factory-fixed I2C address (0x38) - it cannot be changed -
 * so TWO sensors must sit on TWO separate I2C buses. The ESP32 has two
 * hardware I2C peripherals, which is exactly why sensor #1 lives on
 * Wire (21/22) and sensor #2 on Wire1 (25/26).
 *
 * Usage:
 *   AHT10 s1;  s1.begin(Wire);
 *   loop: if (s1.update(millis())) { float t = s1.tempC(); ... }
 */

#include <Arduino.h>
#include <Wire.h>

class AHT10 {
public:
  void begin(TwoWire &bus, uint8_t addr = 0x38, uint32_t periodMs = 2000);

  /** Call as often as possible. Returns true exactly once per new reading. */
  bool update(uint32_t now);

  bool    ok()      const { return _ok; }
  float   tempC()   const { return _t; }
  float   humRH()   const { return _h; }
  uint32_t lastOkMs() const { return _lastOk; }

private:
  enum class Stage : uint8_t { IDLE, MEASURING };
  bool cmd3(uint8_t a, uint8_t b, uint8_t c);
  bool readStatus(uint8_t &st);
  bool readResult();

  TwoWire *_bus = nullptr;
  uint8_t  _addr = 0x38;
  uint32_t _period = 2000;
  uint32_t _lastReq = 0;
  uint32_t _lastOk = 0;
  Stage    _stage = Stage::IDLE;
  bool     _ok = false;
  uint8_t  _fails = 0;
  uint8_t  _readTries = 0;
  float    _t = NAN, _h = NAN;
};
/* ==========================  src/dht.h  ========================== */
/**
 * @file dht.h
 * @brief DHT humidity/temperature sensors - library-free, one wire each.
 *
 * v2.0.9 layout (S3): a DHT22 on the cool-return path is chamber
 * source #2 (averages + RH-peak keep working, one AHT10 fewer to buy);
 * a DHT11 outdoors feeds weather / smart venting. Both models decode
 * through the same 40-bit frame - the DHT11 sends whole degrees/%,
 * the DHT22 tenths (the is11 flag picks the decode).
 *
 * Wiring per sensor: VCC->3V3, GND->GND, DATA->GPIO + 10 k pull-up
 * DATA->3V3 (resistor at the sensor end of long cables). One frame
 * blocks ~5 ms; chamber cadence 3 s (spec min 2 s), outdoor 10 s.
 * 3 missed frames -> ok=false (last values kept for display).
 * pin < 0 = not fitted -> every call is a no-op (classic variant).
 */
#include <Arduino.h>

namespace dht {
  struct Dev {
    int8_t   pin    = -1;        // DATA GPIO (-1 = not fitted)
    bool     is11   = false;     // true = DHT11 decode
    float    t      = NAN;       // deg C, last good frame
    float    h      = NAN;       // %RH
    bool     ok     = false;
    uint32_t gen    = 0;         // bumps on every good frame
    uint8_t  miss   = 0;         // consecutive failed frames
    uint32_t last   = 0;
    uint32_t everyMs= 10000;
    void update();               // cadence gate + one frame
  };
  extern Dev chamber;            // DHT22, cool-return path (chamber #2)
  extern Dev outdoor;            // DHT11, outside in shade (weather)
  void begin();                  // pins up + boot log
  void update();                 // both devices
}
/* ==========================  src/sensors.h  ========================== */
/**
 * @file sensors.h
 * @brief Chamber sensors + the derived averages used for control.
 * Source #1: AHT10 at the TOP of the chamber (I2C0) - the hottest
 * point, so the safety cut reacts first there.
 * Source #2 (v2.0.9): S3 = DHT22 on the cool-return path (GPIO10);
 * classic = AHT10 #2 on I2C1. Same API either way, so averages,
 * RH-peak tracking and the safety cut work identically.
 */
#include <Arduino.h>
#if SENS2_DHT
#endif

class SensorModule {
public:
  bool begin();                 // true if at least one sensor answered
  void update();                // call from loop() - non-blocking

  bool  anyOk()  const { return s1ok() || s2ok(); }
  bool  s1ok()   const { return _s1.ok(); }
  float t1()     const { return _s1.tempC(); }
  float h1()     const { return _s1.humRH(); }
#if SENS2_DHT
  bool  s2ok()   const { return dht::chamber.ok; }
  float t2()     const { return dht::chamber.t; }
  float h2()     const { return dht::chamber.h; }
#else
  bool  s2ok()   const { return _s2.ok(); }
  float t2()     const { return _s2.tempC(); }
  float h2()     const { return _s2.humRH(); }
#endif

  float tAvg()   const { return _tAvg; }   // mean of healthy sensors (heat PID)
  float hAvg()   const { return _hAvg; }
  float hMax()   const { return _hMax; }   // wettest sensor (fan law, completion)
  float tMax()   const { return _tMax; }   // hottest sensor (safety cut)

private:
  void recompute();
  AHT10 _s1;                    // chamber top (I2C0)
#if !SENS2_DHT
  AHT10 _s2;                    // classic: bottom of chamber (I2C1)
#endif
  float _tAvg = NAN, _hAvg = NAN, _hMax = NAN, _tMax = NAN;
};

/* ==========================  src/battery.h  ========================== */
/**
 * @file battery.h
 * @brief Battery voltage / percentage via the resistor divider + low-side
 *        transistor pull-down on the ESP32 ADC, plus bypass-mode control.
 *
 * Hardware:
 *   battery(+) --[100k]--+----> GPIO36 (ADC)
 *                        |
 *                      [15k]
 *                        |
 *   GPIO19 --[1k]--| GATE   (2N7000 N-MOSFET, or NPN + 1k base resistor)
 *                  | D
 *                  +---- divider bottom node above
 *                  S ---> GND
 *
 * GPIO19 high  -> divider pulled to GND -> ADC sees the battery voltage.
 * GPIO19 low   -> divider floating      -> ~zero standby current.
 */
#include <Arduino.h>

struct BatteryProfile {
  const char *name;
  float v100;   // volts at 100 %
  float v0;     // volts at 0 %
};

class BatteryMonitor {
public:
  void begin();
  void update();                       // call from loop(), samples every 5 s

  float   volts()  const { return _v; }
  uint8_t percent()const { return _pct; }
  bool    bypass() const { return _bypass; }
  bool    valid()  const { return _valid; }

  /** Driven by the control loop: engages the bypass supply when the
   *  battery falls to/below the configured percentage. */
  void setBypass(bool on);

  /** Chemistry from saved settings (0=3S Li-ion .. 3=4S LiFePO4). */
  void setType(uint8_t t) { _type = t; }
  uint8_t type() const { return _type; }

  static const BatteryProfile &profile(uint8_t id);
  static uint8_t percentFor(float v, uint8_t type);

private:
  float readVoltsOnce();
  float _v = 0;
  uint8_t _pct = 0;
  uint8_t _type = DEF_BATT_TYPE;
  bool  _bypass = false;
  bool  _valid = false;
  uint32_t _last = 0;
};
/* ==========================  src/buzzer.h  ========================== */
/**
 * @file buzzer.h
 * @brief Buzzer pattern engine - the owner's 37-alert indication spec
 *        (v2.0.15) on a plain ACTIVE buzzer (one tone: cadence carries
 *        the identity; the 13-frequency palette needs a passive piezo).
 *
 * bz::play(BP)       one-shot pattern (higher priority preempts)
 * bz::startRepeat()  nagging pattern, re-fires every periodMs
 * bz::stopRepeat()   silence that pattern
 *
 * The classic v2.0 law stays (POWER-ON 2/s x3s, RUN 3s, END 5s, ERROR
 * 5s, DOOR 1s, MODE 3s) - it is the doc's Step 0/1/5/11 language.
 * Full table: docs/PARAMETERS.md.
 */
#include <Arduino.h>

enum class BP : uint8_t {
  NONE = 0,
  // -- owner spec #1..#37 (n/a: #10 tray prompts, #20 pause, #31/#32 charging)
  KEY, INVALID, TICK, MODE, SAVED, BACK,                    // 1-6
  DOOR_AJAR, NO_TRAYS, UNSTABLE, READY, BATT_LOW, BATT_CRIT,// 7-12(+13)
  SETPOINT, FAN_ON, DOOR_OPEN_RUN, MIDWAY, APPROACH, TIMEOUT5, //14-19
  WARN, CRITICAL, CRITICAL_R, RECOVERED, E05_LOAD,          // 23-26
  AP_UP, CLIENT, LOG_SAVED, STORE_FULL, BROWNOUT_RET,       // 27-30,21
  ANOMALY, MAINT, INIT_FAIL, COOL_DONE, SHUTDOWN, FACT_RESET, //22,33-37
  // -- classic law
  POWER_ON, CYCLE_START, CYCLE_DONE, ERROR, DOOR, MODE_CHANGE
};

namespace bz {
  void play(BP p);                        // one-shot
  void startRepeat(BP p, uint32_t periodMs);
  void stopRepeat(BP p);
  void stopAllRepeats();
  bool repeating(BP p);
  // classic v2.0 API (kept - old call sites)
  void powerOn(); void cycleStart(); void cycleDone(); void error();
  void door(); void modeChange();
}

#if BUZZER_ENABLED

class Buzzer {
public:
  void begin();
  void update();                       // call from loop()
  void beep(uint8_t n, uint16_t onMs, uint16_t offMs = 100);
                                       // legacy v2.0 cadence API (kept -
                                       // old call sites: stop/warn/key/menu)
  bool busy() const { return _p != BP::NONE || _bTot != 0; }
  // engine (used by bz::)
  void playPat(BP p);
  void addRepeat(BP p, uint32_t periodMs);
  void delRepeat(BP p);
  void stopAll();
  bool repeating(BP p);
  void pump();                         // re-fire due repeats
private:
  void drive(bool on);
  BP        _p = BP::NONE;             // playing pattern
  uint8_t   _seg = 0, _repLeft = 0;
  uint32_t  _tEdge = 0;
  bool      _on = false;
  // legacy beep() mini-cadence (n x onMs with offMs gaps)
  uint8_t   _bTot = 0, _bDone = 0;
  uint16_t  _bOnMs = 100, _bOffMs = 100;
  bool      _bOn = false;
  struct R { BP p; uint32_t period; uint32_t last; } _r[4] = {};
};

extern Buzzer buzzer;

#else

class Buzzer {
public:
  void begin() {}
  void update() {}
  void beep(uint8_t, uint16_t, uint16_t = 100) {}
  bool busy() const { return false; }
  void playPat(BP) {}
  void addRepeat(BP, uint32_t) {}
  void delRepeat(BP) {}
  void stopAll() {}
  bool repeating(BP) { return false; }
  void pump() {}
};

extern Buzzer buzzer;

#endif
  POWER_ON, CYCLE_START, CYCLE_DONE, ERROR, DOOR, MODE_CHANGE
};

namespace bz {
  void play(BP p);                        // one-shot
  void startRepeat(BP p, uint32_t periodMs);
  void stopRepeat(BP p);
  void stopAllRepeats();
  bool repeating(BP p);
  // classic v2.0 API (kept - old call sites)
  void powerOn(); void cycleStart(); void cycleDone(); void error();
  void door(); void modeChange();
}

#if BUZZER_ENABLED

class Buzzer {
public:
  void begin();
  void update();                       // call from loop()
  bool busy() const { return _p != BP::NONE; }
  // engine (used by bz::)
  void playPat(BP p);
  void addRepeat(BP p, uint32_t periodMs);
  void delRepeat(BP p);
  void stopAll();
  bool repeating(BP p);
  void pump();                         // re-fire due repeats
private:
  void drive(bool on);
  BP        _p = BP::NONE;             // playing pattern
  uint8_t   _seg = 0, _repLeft = 0;
  uint32_t  _tEdge = 0;
  bool      _on = false;
  struct R { BP p; uint32_t period; uint32_t last; } _r[4] = {};
};

extern Buzzer buzzer;

#else

class Buzzer {
public:
  void begin() {}
  void update() {}
  bool busy() const { return false; }
  void playPat(BP) {}
  void addRepeat(BP, uint32_t) {}
  void delRepeat(BP) {}
  void stopAll() {}
  bool repeating(BP) { return false; }
  void pump() {}
};

extern Buzzer buzzer;

#endif
/* ==========================  src/eelog.h  ========================== */
/**
 * @file eelog.h
 * @brief AT24C256 (32 kB I2C EEPROM) long-term cycle registry - v2.0.17.
 *
 * LittleFS keeps the detailed per-cycle CSVs (last 40); this chip keeps a
 * SUMMARY of EVERY cycle (~817 slots, years of batches) that outlives any
 * filesystem reformat. Wiring: VCC->3V3, GND->GND, SDA/SCL on I2C0
 * (GPIO 8/9), A0/A1/A2->GND = address 0x50. Auto-detected at boot - no
 * chip, no problem (everything else works without it).
 *
 * Layout: page 0 = header (magic, version, count, head, seq); records of
 * 40 bytes start at 64. Ring buffer: when full, the oldest summary is
 * overwritten. One append = one record page-write + header rewrite
 * (~10 ms, once per cycle - wear is a non-issue at 1 M cycles/cell).
 * Each record carries a CRC8; a torn write (power loss mid-append) is
 * rolled back at the next boot.
 */
#include <Arduino.h>

namespace eelog {

// one cycle summary, 40 bytes on the chip (packed by hand in eelog.cpp)
struct EeRec {
  uint32_t seq;          // 1, 2, 3, ... (0 = never written)
  uint32_t startEpoch;   // unix time the cycle started
  uint32_t durS;         // duration in seconds
  uint8_t  mode;         // 0 agarbatti / 1 user / 2 silica
  uint8_t  endR;         // 0 done / 1 stopped / 2 fault / 3 timeout / 4 interrupted
  uint8_t  ecode;        // E-code (0 = none)
  uint8_t  flags;        // bit0 scaleLost · bit1 interrupted · bit2 targetReached
  int16_t  setT10;       // setpoint  x10 C
  int16_t  tAvg10;       // avg temp  x10 C
  int16_t  tMax10;       // max temp  x10 C
  int16_t  hMax10;       // max RH    x10 %
  int16_t  outT10;       // avg outdoor temp x10 C
  int16_t  wtS10;        // start weight  x10 g
  int16_t  wtE10;        // end weight    x10 g
  int16_t  wtT10;        // target weight x10 g
  int16_t  vbS10;        // battery at start x10 V
  int16_t  vbE10;        // battery at end   x10 V
  uint8_t  crc;          // CRC8 over bytes 0..37
  uint8_t  spare;
};

#if ELOG_ENABLED

void begin();                 // detect the chip, validate/repair the header
bool ok();                    // chip present and healthy
void append(EeRec &r);        // add one summary (r.seq assigned here)
uint16_t count();             // summaries stored (<= slots())
uint16_t slots();             // ring capacity (817 on a 24C256)
bool get(uint16_t i, EeRec &r);   // i = 0 (oldest) .. count()-1
void clear();                 // logical wipe (header count = 0)

#else   // no EEPROM fitted: everything compiles away to nothing

inline void begin() {}
inline bool ok()                   { return false; }
inline void append(EeRec &)        {}
inline uint16_t count()            { return 0; }
inline uint16_t slots()            { return 0; }
inline bool get(uint16_t, EeRec &) { return false; }
inline void clear()                {}

#endif

}  // namespace eelog
/* ==========================  src/rtc.h  ========================== */
/**
 * @file rtc.h
 * @brief DS1302 real-time clock (date & time) - 3-wire bit-banged,
 *        auto-detected, library-free (v2.0.21).
 *
 * The DS1302 keeps the wall clock on its own CR2032 coin cell, so the
 * date & time survive a FULL power-down (even a battery disconnect).
 * Without the chip the firmware falls back to the old phone-sync + NVS
 * clock - nothing else changes.
 *
 * Wiring (DS1302 module -> S3):  VCC -> 3V3, GND -> GND,
 *   SCLK -> PIN_RTC_SCLK,  I/O -> PIN_RTC_IO,  RST -> PIN_RTC_RST
 *   (the module's BZ buzzer pin is unused; its battery stays ON the
 *    module and holds the time when the pillar is off).
 *
 * Behaviour:
 *  - begin(): detects the chip (two identical reads). Write-protect is
 *    always cleared (its power-on state is undefined). A factory-fresh
 *    module ships with the CH (halt) flag set + garbage registers, so
 *    such a time is untrusted until the first real set.
 *  - readTime(): 24 h LOCAL wall clock from the chip (converted to an
 *    absolute epoch using the configured timezone - no TZ env needed).
 *  - writeNow(): system clock -> chip (local time, tzMinutes applied).
 *  - All calls are no-ops (present() == false) when no chip is found
 *    or the RTC pins are -1 (classic variant).
 */
#include <Arduino.h>

namespace rtc {
bool  begin();                  // detect + clear write-protect; true = present
bool  present();                // chip found (does not mean time is good)
bool  readTime(time_t *outEp);  // true = chip running with a sane time
void  writeNow();               // system clock -> chip (no-op if absent)
const char *statusText();       // short one-liner for serial/console
}
/* ==========================  src/keypad.h  ========================== */
/**
 * @file keypad.h
 * @brief Optional 16-key hex keypad on a PCF8574 I2C backpack.
 *
 * Wired to the PCF8574: rows on P0-P3, columns on P4-P7. Shares Wire
 * (GPIO21/22) with AHT10 #1 - the PCF8574 answers at 0x20..0x26, the
 * AHT10 at 0x38, so no address conflict (buy PCF8574, NOT PCF8574A!).
 * KEYPAD_ENABLED 0 compiles it out completely.
 */
#include <Arduino.h>
#include <Wire.h>

#if KEYPAD_ENABLED

class I2CKeypad {
public:
  void begin(TwoWire &bus, uint8_t addr);   // scans once, sets ok()
  char update();                            // call from loop(); 15 ms debounce,
                                            // returns a key on NEW press only
  bool ok()   const { return _ok; }         // PCF8574 answered recently
  char last() const { return _lastKey; }    // last accepted key

private:
  char scan();                              // one matrix sweep, 0 if none
  static const char kMap[4][4];             // row/col -> key
  TwoWire *_bus = nullptr;
  uint8_t  _addr = 0x20;
  bool     _ok = false;
  char     _lastKey = 0;
  char     _raw = 0, _prev = 0;             // debounce state
  bool     _fired = false;                  // one event per press
  uint32_t _tEdge = 0, _tScan = 0;
};

extern I2CKeypad keypad;

#else

class I2CKeypad {                           // stub: compiled out
public:
  void begin(TwoWire &, uint8_t) {}
  char update() { return 0; }
  bool ok()   const { return false; }
  char last() const { return 0; }
};

extern I2CKeypad keypad;

#endif

/* ==========================  src/scale.h  ========================== */
/**
 * @file scale.h
 * @brief Optional weigh scale: 2 x half-bridge load cells + HX711 24-bit
 *        ADC - the "dry to weight" sensor (calibration by the batch).
 *
 * Library-free bit-bang driver. RATE output mode (10 Hz), channel A
 * gain 128, 25 pulses per read. Tare + calibration factor live in the
 * Settings (NVS) so they survive reboots; the runtime object is fed
 * them by main at boot and after every settings save.
 *
 * PINS (see config.h): S3 variant uses dedicated GPIO 1 (CLK) + 2 (DOUT).
 * The classic ESP32 has NO free output pin while the display is fitted,
 * so the scale is compile-out there by default (options in config.h).
 */
#include <Arduino.h>

#if SCALE_ENABLED

class LoadScale {
public:
  void begin();                     // pins up; HX711 powers & self-checks
  void update();                    // call from loop; ~2 Hz reads, non-blocking-ish
  bool ok() const { return _ok; }   // HX711 is responding
  float grams() const { return _g; }        // calibrated + tared, NAN before first fix
  float rate() const { return _rate; }      // g/min over the last ~5 min, NAN if unknown
  void tare();                      // zero at the current reading
  void calibrate(float knownGrams); // set factor with a known weight on top
  float calFactor() const { return _factor; }
  void setFactor(float f) { _factor = f > 0.0f ? f : 1.0f; }
  void setOffset(int32_t o) { _offset = o; }
  int32_t offset() const { return _offset; }
private:
  bool readRaw(int32_t &v);         // one 24-bit conversion
  int32_t _raw = 0, _offset = 0;
  float   _factor = 1.0f;           // raw units per gram
  float   _g = NAN, _rate = NAN;
  uint32_t _lastTry = 0, _lastOk = 0;
  bool    _ok = false;
  // rolling history for the rate: 10 samples x 30 s = 5 min window
  float   _hG[10] = {0};
  uint32_t _hT[10] = {0};
  uint8_t _hN = 0, _hIdx = 0;
};

extern LoadScale scale;

#else

class LoadScale {                   // stub: compiled out
public:
  void begin() {}
  void update() {}
  bool ok() const { return false; }
  float grams() const { return NAN; }
  float rate() const { return NAN; }
  void tare() {}
  void calibrate(float) {}
  float calFactor() const { return 1.0f; }
  void setFactor(float) {}
  void setOffset(int32_t) {}
  int32_t offset() const { return 0; }
};

extern LoadScale scale;

#endif
/* ==========================  src/door.h  ========================== */
/**
 * @file door.h
 * @brief Door limit switch + the CALIBRATE -> LOAD -> READY workflow.
 *
 * This build: LIMIT SWITCH ONLY (open/closed status) - no solenoid lock.
 * Software gates: START refused until the scale is calibrated, batch
 * weighed on door close (READY), opening mid-cycle = instant FAULT.
 * A 12 V lock can be fitted later (DOOR_LOCK_ENABLED 1).
 *
 * The weigh scale is the master of the workflow:
 *   1. NOT calibrated  -> START LOCKED. Start is refused everywhere
 *      (website, keypad, BOOT button, serial).
 *   2. Calibrated      -> gate opens. "Load the trays."
 *   3. Door closed with the scale stable -> batch weight = the diff
 *      vs the tare captured at calibration. "READY: 1234 g - Start".
 *   4. RUNNING         -> opening the door mid-cycle is a FAULT
 *      (heat + fans stop, purge, power off).
 *   5. DONE/FAULT      -> ready for unloading.
 *
 * Classic ESP32 has no free pins for the switch -> DOOR_ENABLED 0: the same
 * workflow runs in software (start still refused until calibration).
 */
#include <Arduino.h>

namespace door {
  void begin(bool engageLock = true);  // pins + calibrated flag (NVS);
                                        // engageLock=false: outputs LOW (boot)
  void engage();                // after the 10 s init window: lock per state
  void update();                // call from loop: reed edges, lock, workflow
  bool fitted();                // lock/reed hardware present
  bool locked();                // lock output engaged
  bool closed();                // reed says closed (always true if unfitted)
  bool calibrated();            // scale calibrated at least once
  void markCalibrated();        // called after a successful known-weight cal
  void serviceUnlock();         // service override: unlock (serial only)
  float batchG();               // measured batch weight, 0 until measured
  const char *phase();          // CALIBRATE / LOAD / READY / RUNNING
}
/* ==========================  src/supply.h  ========================== */
/**
 * @file supply.h
 * @brief Power latch + supply selector (v2.0): solar <-> bypass with an
 *        optocoupler live-check, the 2-channel feed relay, the master
 *        power button (hard off / reboot) and the default-automation
 *        button.
 *
 * HARD POWER-OFF CIRCUIT (manual 02 sect.4.11): BUTTON-1 connects the
 * P-MOSFET latch gate to GND while pressed -> the MCU gets power ->
 * this module asserts PIN_POWER_HOLD within milliseconds of boot.
 *   BUTTON-1 held 3 s  -> firmware stops the loads, drops the hold ->
 *                         the WHOLE pillar loses power.
 *   BUTTON-1 held 10 s -> reboot instead (ESP.restart()).
 *
 * SUPPLY MODE: a physical toggle requests SOLAR (or BYPASS). The MCU
 * drives the 2-channel relay (CH1 = solar feed, CH2 = bypass feed) but
 * ONLY when the loads are quiet - a change requested while a supply is
 * under load is refused with a 5 s error beep. The optocoupler input
 * verifies the selected feed is actually live; a mismatch is an error.
 * At battery cutoff, if the bypass feed is live, the MCU switches to
 * bypass automatically (and beeps).
 */
#include <Arduino.h>

namespace supply {
  void begin();              // latch hold UP FIRST, pins, relay LOW (boot)
  void engage();             // after the init window: relay follows the toggle
  void update();             // buttons, toggle, opto check, relay switching
  bool solarRequested();     // toggle position: true = solar mode
  const char *modeName();    // "SOLAR MODE" / "BYPASS MODE"
  bool optoLive();           // optocoupler: selected feed is live
  bool relayAuto();          // the MCU can switch the feed relay (S3)
  bool latched();            // soft-latch hardware present
  void powerOff();           // safe-stop then drop the latch (hard off)
  void requestSwitch();      // follow the toggle now if safe (serial too)
}
/* ==========================  src/menu.h  ========================== */
/**
 * @file menu.h
 * @brief v2.0 on-device menu: keypad navigation + parameter entry.
 *
 * Key map (spec): 2=UP 4=LEFT 6=RIGHT 8=DOWN, 1/3/5/7/9/0 digits,
 * * = HOME, # = BACK, A = ENTER/OK, B = MENU, C = MODE, D = RUN.
 *
 * The state machine lives here (works with or without a display - every
 * action is also echoed on serial); the TFT renderer in display.cpp
 * reads the getters when menu::active().
 */
#include <Arduino.h>

namespace menu {
  bool key(char k);             // feed one keypad key; true = consumed
  bool active();                // a menu/edit screen is up
  bool editing();               // value-entry screen
  bool info();                  // OTA firmware-update info screen
  uint8_t cursor();             // selected row in the list
  uint8_t itemCount();
  const char *itemName(uint8_t i);
  const char *itemValue(uint8_t i);   // current value, formatted
  const char *editBuffer();           // digits typed so far
  const char *editHint();             // unit / range hint
}
/* ==========================  src/txdisp.h  ========================== */
/**
 * @file txdisp.h
 * @brief v2.0.3: serial display bridge - streams the status screen as
 *        tiny text packets (~150 bytes/s, 1 Hz) to a companion Arduino
 *        that drives a PARALLEL (D0-D7 + WR + RD "UNO-shield") TFT.
 *
 * Why: the parallel display cannot connect to the ESP32-S3 (no SPI pins,
 * not enough GPIO for an 8-bit bus). But it plugs straight onto an
 * Arduino UNO/Mega - so the Arduino becomes the display driver and the
 * S3 just tells it what to show. One wire, one direction:
 *
 *   S3 GPIO3 (TX, 3.3 V) ---> Arduino RX (UNO/Nano pin 4 SoftwareSerial,
 *                              Mega pin 19 RX1)      + GND common
 *   NEVER wire the Arduino's TX (5 V) back to the S3!
 *
 * Packet format (plain lines, key,value - see display-bridge-uno.ino):
 *   $ST,DRYING   $T,60.5,58.2   $H,45,50   $SET,60,120
 *   $E,3600,3600 $P,34,100,12.4,78  heat,fan,batV,bat%
 *   $W,1234,1000,234  cur,target,diff   $D,READY,1,1234  phase,closed,batch
 *   $S,SOLAR MODE,1,0 $F,<fault text>   $V,2.0.3
 *
 * The S3 firmware stays zero-library; the ARDUINO side uses its own
 * graphics stack (Adafruit GFX + MCUfriend_kbv) - manual 02 sect.4.12.
 */

namespace txdisp {
  void begin();     // UART up on PIN_TXDISP_TX at TXDISP_BAUD
  void update();    // 1 Hz gated: one $-packet burst (call from loop)
}
/* ==========================  src/display.h  ========================== */
/**
 * @file display.h
 * @brief Optional 3.5" SPI TFT status display (ILI9488, 480x320, non-touch).
 *
 * Library-free driver on remapped SPI pins (see config.h: the ESP32's
 * default SPI pins are all taken by the dryer hardware, so the display
 * uses the free/repurposed GPIO pool). DISPLAY_ENABLED 0 compiles it out.
 * Sharing rule: the display occupies GPIO 12/0/2/17/23 - it replaces the
 * (not fitted) relays and buzzer. Set DISPLAY_ENABLED 0 to free them.
 */
#include <Arduino.h>

#if DISPLAY_ENABLED
namespace display {
  void begin();                  // SPI up, panel init, first frame
  void update();                 // 1 Hz gated - splash / menu / status
  void splash(bool on);          // v2.0 boot splash (ARCHITECTS OF SOLUTIONS)
}
#else
namespace display {
  inline void begin() {}
  inline void update() {}
  inline void splash(bool) {}
}
#endif
/* ==========================  src/pixel.h  ========================== */
/**
 * @file pixel.h
 * @brief On-board RGB status pixel (WS2812/NeoPixel) - zero libraries.
 *
 * The DevKitC-1's RGB LED is a WS2812 on ONE data pin (v1.0: GPIO48,
 * v1.1: GPIO38 - check the silkscreen; 48 is this build's default and
 * stays free). Driven through the hardware SPI peripheral at 6.4 MHz:
 * every WS2812 bit becomes 8 SPI bits (2 high = "0", 5 high = "1"),
 * which gives rock-solid timing without any NeoPixel library. The SPI
 * bus is free - the display is gone (website is the screen).
 *
 * Status law (owner spec "indication"):
 *   boot        blue
 *   door open   yellow            (load your trays)
 *   IDLE        dim cyan pulse
 *   RUNNING     green breathing   (boost heat-up: orange blink)
 *   battery<20% yellow blink      (while running)
 *   PURGING     teal blink
 *   DONE        SOLID GREEN       = the spec's "completion LED"
 *   FAULT       fast red blink
 * PIXEL_COUNT > 1 chains an external strip on the same data pin.
 */
#include <Arduino.h>

namespace pixel {
  void begin();      // SPI up on PIN_PIXEL, LED off
  void update();     // call ~10 Hz from loop: state -> colour
  bool rePin(int);   // move the pixel to another data pin (48/38) at runtime
}
/* ==========================  src/control.h  ========================== */
/**
 * @file control.h
 * @brief Dryer state machine + control laws + settings (persisted in NVS).
 *
 * State flow:
 *   IDLE -> RUNNING -> COOLDOWN -> DONE (relay opens, power cut)
 *     ^                                |
 *     +---------- "Power On" ----------+
 *   any state -> FAULT (sensor loss / over-temperature / battery empty)
 *
 * Heaters : BTS7960, PID holds chamber temp at settings.setTemp
 * Fans    : L298N, speed follows humidity band settings.humLow/humHigh
 * Relay   : cuts power to heater+fans when the cycle completes (or on fault)
 */
#include <Arduino.h>
#include <Preferences.h>
enum class DState : uint8_t { IDLE = 0, RUNNING, COOLDOWN, DONE, FAULT };
const char *stateName(DState s);

// ---- E-code fault system (owner spec, v2.0.14) --------------------------
// E01..E20 mirror the owner's documentation. sev 0 = WARNING (cycle runs
// on, operator nudged), sev 1 = CRITICAL (cycle halted, power cut,
// manual reset via Power On). Reserved (needs hardware): E08 door-sensor
// self-test, E09 blower RPM proof, E10 heater-current sense.
struct FaultRec {
  uint8_t  code = 0;             // 0 = none; 1..20 = E01..E20
  uint8_t  sev  = 0;
  bool     active = false;
  uint32_t sinceMs = 0;
  float    val = NAN;            // detection value (e.g. 92.4 C)
  float    limit = NAN;          // threshold it crossed
};
const char *eName(uint8_t code); // "HEATER OVERHEATING" ...

struct Settings {
  uint32_t magic;         // 'SDRY'
  uint16_t ver;           // bump when the struct changes
// on, operator nudged), sev 1 = CRITICAL (cycle halted, power cut,
// manual reset via Power On). Reserved (needs hardware): E08 door-sensor
// self-test, E09 blower RPM proof, E10 heater-current sense.
struct FaultRec {
  uint8_t  code = 0;             // 0 = none; 1..20 = E01..E20
  uint8_t  sev  = 0;
  bool   requireHum;      // also wait for humTarget before finishing
  uint32_t dryMinutes;    // manual drying duration
  uint8_t fanMin;         // % circulation speed inside the band
  uint8_t fanIn;          // % intake fan scaling of the computed duty
  uint8_t fanOut;         // % exhaust fan scaling of the computed duty
  uint8_t fanSlope;       // % duty added per RH point above humHigh
  uint8_t heaterMax;      // % soft cap on coil duty
  uint16_t cooldownSec;   // purge time before the relay opens
  uint8_t bypassPct;      // battery % that engages bypass

  uint8_t battType;       // 0=3S Li-ion 1=4S Li-ion 2=12V SLA 3=4S LiFePO4
  int16_t tzMinutes;      // local UTC offset in minutes (330 = IST)
  bool   smartVent;       // pause venting when outside RH >= chamber RH
  bool   boostHeat;       // full power to setTemp, then PID holds
  float  kp, ki, kd;      // heater PID gains
  bool   requireWeight;   // dry-to-weight: also wait for weight to settle
  float  weightRateG;     // g/min - "settled" means |rate| below this
  uint16_t weightMinY;    // minutes the rate must stay low before ending
  float  scaleCal;        // HX711 calibration: raw units per gram
  int32_t scaleOffset;    // HX711 tare offset (raw)
  float  targetG;         // g   target batch weight (0 = off): within 5 %
  // ---- v2.0.16: stick & paste calculator (owner spec) ------------------
  // The moisture is a property of the PASTE, so it lives here as a
  // recipe parameter. stickCount > 0 -> targetG is COMPUTED:
  //   target = N x stickWetG x (1 - (pasteWater% - targetMoist%)/100)
  uint16_t stickCount;    // sticks in the batch (0 = calculator off)
  float  stickWetG;       // g   average WET stick mass (typ. 2.5)
  float  pasteWaterPct;   // %   water in the raw paste (typ. 30-40)
  float  targetMoistPct;  // %   residual moisture wanted (typ. 8-10)
                          //     of it at time-up = complete, else warning
  uint8_t fanTrigRH;      // %RH burst trigger (spec: 60)
  uint8_t fanTrigMin;     // min RH high before the fan fires (spec: 1)
  uint8_t fanBurstS;      // s   fan run time per burst at 100 % (spec: 60)
  uint8_t mode;           // 0=AGARBATTI 1=USER DEFINED 2=SILICAGEL
};

Settings defaultSettings();
  float  humHigh;         // %RH    - fans ramp above
  float  humTarget;       // %RH    - optional completion criterion
  bool   requireHum;      // also wait for humTarget before finishing
  uint32_t dryMinutes;    // manual drying duration
  uint8_t fanMin;         // % circulation speed inside the band
  uint8_t fanIn;          // % intake fan scaling of the computed duty
  uint8_t fanOut;         // % exhaust fan scaling of the computed duty
  uint8_t fanSlope;       // % duty added per RH point above humHigh
  uint8_t heaterMax;      // % soft cap on coil duty
  uint16_t cooldownSec;   // purge time before the relay opens
  int16_t  fan;      // %
  int16_t  vb10;     // battery V  x10
  int16_t  bat;      // battery %
  int16_t  wt10;     // batch weight x10 g (INT16_MIN = no scale)
  int16_t  ot10 = INT16_MIN;   // outdoor temp x10 (DHT11; MIN = none)
  int16_t  oh10 = INT16_MIN;   // outdoor RH   x10 (DHT11; MIN = none)
};

// Latest outdoor weather - pushed by the phone's browser (it has mobile
  float  kp, ki, kd;      // heater PID gains
  bool   requireWeight;   // dry-to-weight: also wait for weight to settle
  float  weightRateG;     // g/min - "settled" means |rate| below this
  uint16_t weightMinY;    // minutes the rate must stay low before ending
  float  scaleCal;        // HX711 calibration: raw units per gram
  int32_t scaleOffset;    // HX711 tare offset (raw)
  float  targetG;         // g   target batch weight (0 = off): within 5 %
  // ---- v2.0.16: stick & paste calculator (owner spec) ------------------
  // The moisture is a property of the PASTE, so it lives here as a
  // recipe parameter. stickCount > 0 -> targetG is COMPUTED:
  //   target = N x stickWetG x (1 - (pasteWater% - targetMoist%)/100)
  uint16_t stickCount;    // sticks in the batch (0 = calculator off)

bool weatherFresh();           // received recently enough to trust
const char *outdoorSrc();      // "sensor" | "live" | "manual" | "stale" | "none"
bool getOutdoor(float &t, float &h);   // merged outdoor values (phone forecast/manual)

class Dryer {
public:
  uint8_t mode;           // 0=AGARBATTI 1=USER DEFINED 2=SILICAGEL
};

Settings defaultSettings();
Settings loadSettings();            // NVS if valid, else defaults
  void powerOn();                    // DONE/FAULT -> IDLE, relay closed again
  void addMinutes(int m);            // extend/shorten remaining time live
  void applySettings(const Settings &s);   // from the web UI
  void faultNow(const char *why);          // external safety stop (door!)
  void faultNowE(uint8_t ecode, const char *why,
                 float val = NAN, float limit = NAN);   // E-coded stop
  void warnE(uint8_t ecode, const char *why,
             float val = NAN, float limit = NAN);       // E-coded warning
  const FaultRec &faultRec() const { return _fr; }      // last critical
  const FaultRec &warnRec()  const { return _wr; }      // last warning
  bool warnActive() const { return _wr.active && _wr.code != 0; }
  void clearWarn(uint8_t code);        // condition fixed -> CLEARED + relief
  bool scaleLost()  const { return _scaleLost; }        // E15 degraded mode
  void applyMode(uint8_t m);               // 0 agarbatti / 1 user / 2 silica
  uint8_t mode() const { return _cfg.mode; }
  float    wtStartG()  const { return _wtStart; }     // weight at start
  float    finalG()    const { return _finalG; }      // weight at DONE
  float    moistureG() const                                 // grams removed
           { return (_wtStart - _finalG); }
  uint32_t endElapsedS() const { return _endElapsed; } // duration at DONE
  float    suggestMin() const { return _suggestMin; } // +min to target
  void setManualHeat(uint8_t pct);   // web knob: exact duty, auto-releases

  DState     state()    const { return _st; }
  const char*faultWhy() const { return _why; }
  uint8_t    heatDuty() const { return _heatDuty; }
  uint8_t    fanDuty()  const { return _fanDuty; }   // the law's demand
  uint8_t    fanInDuty()  const { return _fanInD; }  // actually applied
  uint8_t    fanOutDuty() const { return _fanOutD; }
  bool       boosting()  const { return _boosting; }  // max-power heat-up on
  bool       manualOn()  const;                       // knob window active
  uint8_t    manualPct() const { return _manPct; }    // last knob position
  uint16_t   manualLeftS() const;                     // 0 = back on automatic
  bool       relayOn()  const { return _relayOn; }
  uint32_t   elapsedS() const { return _elapsed; }      // RUNNING time
  uint32_t   remainingS() const;                        // 0 when not running
  int16_t  bat;      // battery %
  int16_t  wt10;     // batch weight x10 g (INT16_MIN = no scale)
  int16_t  ot10 = INT16_MIN;   // outdoor temp x10 (DHT11; MIN = none)
  int16_t  oh10 = INT16_MIN;   // outdoor RH   x10 (DHT11; MIN = none)
};

// Latest outdoor weather - pushed by the phone's browser (it has mobile
// data even while on the dryer hotspot). ESP itself never goes online.
struct Weather {
  float    tempC   = NAN;
  float    humRH   = NAN;
  float    rainPct = NAN;
  float    windKmh = NAN;
  uint8_t  code    = 100;      // WMO weather code (100 = unknown)
  Settings   _cfg;
  DState     _st = DState::IDLE;
  char       _why[40] = {0};
  float      _finalG = NAN;               // v2.0.15 completion summary
  uint32_t   _endElapsed = 0, _doneAt = 0;
  bool       _spReached = false, _midway = false, _anomWarned = false;
  float      _lastG = NAN;
  FaultRec   _fr, _wr;                     // E-code records (v2.0.14)
  bool       _scaleWasOk = false;          // E15 baseline
  uint32_t   _scaleLostSince = 0, _voltLowSince = 0,
             _voltHighSince = 0, _tHighWarnSince = 0;
  char       _endReason[40] = "completed";   // cycle-history entry reason
  bool       _scaleLost = false;             // E15: running without the scale
  bool       _singleWarned = false;          // one chamber sensor left
  uint32_t   _tStart = 0, _elapsed = 0, _cdStart = 0;
  uint8_t    _heatDuty = 0, _fanDuty = 0, _fanInD = 0, _fanOutD = 0;
  // v2.0 fan bursts + watchdogs + target weight
  uint32_t   _trigSince = 0;      // RH above the trigger since (burst timer)
  uint32_t   _fanBurstUntil = 0;  // fan 100 % until this millis
  uint32_t   _rhErrSince = 0;     // RH above humHigh since (fan error 5 min)
  uint32_t   _flatSince = 0;      // heater-failure: heat-up flatline since
  float      _flatT0 = NAN;       // temperature when the flatline started
  float      _wtStart = NAN;      // batch weight captured at start
  float      _suggestMin = -1;    // suggested extra minutes to target
  bool       _tWarned = false;    // T-5-min target warning fired
  bool       _relayOn = false;
  float      _integ = 0, _lastE = 0;
  bool       _boosting = false;
  uint32_t   _manUntil = 0;          // millis() deadline of the knob window
  uint8_t    _manPct = 0;            // knob duty the user asked for
  uint32_t   _sensFailSince = 0;
  uint32_t   _wtGoodSince = 0;      // weight-settle window start
  uint32_t   _lastTick = 0, _lastLog = 0;

  static LogRec _log[LOG_MAX];
  void begin(const Settings &s);
  void tick();                       // call from loop() - 1 s control cadence

  void start();                      // IDLE/DONE/FAULT -> RUNNING
  void stop();                       // user stop -> purge -> power cut
  void powerOn();                    // DONE/FAULT -> IDLE, relay closed again
  void addMinutes(int m);            // extend/shorten remaining time live
  void applySettings(const Settings &s);   // from the web UI
  void faultNow(const char *why);          // external safety stop (door!)
  void faultNowE(uint8_t ecode, const char *why,
                 float val = NAN, float limit = NAN);   // E-coded stop
  void warnE(uint8_t ecode, const char *why,
             float val = NAN, float limit = NAN);       // E-coded warning
  const FaultRec &faultRec() const { return _fr; }      // last critical
  const FaultRec &warnRec()  const { return _wr; }      // last warning
  bool warnActive() const { return _wr.active && _wr.code != 0; }
  void clearWarn(uint8_t code);        // condition fixed -> CLEARED + relief
  bool scaleLost()  const { return _scaleLost; }        // E15 degraded mode
  void applyMode(uint8_t m);               // 0 agarbatti / 1 user / 2 silica
  uint8_t mode() const { return _cfg.mode; }
  float    wtStartG()  const { return _wtStart; }     // weight at start
  float    finalG()    const { return _finalG; }      // weight at DONE
  float    moistureG() const                                 // grams removed
           { return (_wtStart - _finalG); }
  uint32_t endElapsedS() const { return _endElapsed; } // duration at DONE
  float    suggestMin() const { return _suggestMin; } // +min to target
  void setManualHeat(uint8_t pct);   // web knob: exact duty, auto-releases

  DState     state()    const { return _st; }
  const char*faultWhy() const { return _why; }
  uint8_t    heatDuty() const { return _heatDuty; }
  uint8_t    fanDuty()  const { return _fanDuty; }   // the law's demand
  uint8_t    fanInDuty()  const { return _fanInD; }  // actually applied
  uint8_t    fanOutDuty() const { return _fanOutD; }
  bool       boosting()  const { return _boosting; }  // max-power heat-up on
  bool       manualOn()  const;                       // knob window active
  uint8_t    manualPct() const { return _manPct; }    // last knob position
  uint16_t   manualLeftS() const;                     // 0 = back on automatic
  bool       relayOn()  const { return _relayOn; }
  uint32_t   elapsedS() const { return _elapsed; }      // RUNNING time
  uint32_t   remainingS() const;                        // 0 when not running

  // ---- data log (RAM ring buffer) ----
  uint16_t   logCount() const { return _logN; }
  uint16_t   logStart() const { return 0; }
  const LogRec &logAt(uint16_t i) const;                // 0 = oldest
  void      clearLog() { _logN = 0; _logHead = 0; }

private:
  void pidStep();
  void fanStep();
  void outputsAllOff();
  void setRelay(bool on);
  void raiseFault(const char *why);

  Settings   _cfg;
  DState     _st = DState::IDLE;
  char       _why[40] = {0};
  float      _finalG = NAN;               // v2.0.15 completion summary
  uint32_t   _endElapsed = 0, _doneAt = 0;
  bool       _spReached = false, _midway = false, _anomWarned = false;
  float      _lastG = NAN;
  FaultRec   _fr, _wr;                     // E-code records (v2.0.14)
  bool       _scaleWasOk = false;          // E15 baseline
  uint32_t   _scaleLostSince = 0, _voltLowSince = 0,
             _voltHighSince = 0, _tHighWarnSince = 0;
  char       _endReason[32] = "completed";   // cycle-history entry reason
  bool       _scaleLost = false;             // E15: running without the scale
  bool       _singleWarned = false;          // one chamber sensor left
  uint32_t   _tStart = 0, _elapsed = 0, _cdStart = 0;
  uint8_t    _heatDuty = 0, _fanDuty = 0, _fanInD = 0, _fanOutD = 0;
  // v2.0 fan bursts + watchdogs + target weight
  uint32_t   _trigSince = 0;      // RH above the trigger since (burst timer)
  uint32_t   _fanBurstUntil = 0;  // fan 100 % until this millis
  uint32_t   _rhErrSince = 0;     // RH above humHigh since (fan error 5 min)
  uint32_t   _flatSince = 0;      // heater-failure: heat-up flatline since
  float      _flatT0 = NAN;       // temperature when the flatline started
  float      _wtStart = NAN;      // batch weight captured at start
  float      _suggestMin = -1;    // suggested extra minutes to target
  bool       _tWarned = false;    // T-5-min target warning fired
  bool       _relayOn = false;
  float      _integ = 0, _lastE = 0;
  bool       _boosting = false;
  uint32_t   _manUntil = 0;          // millis() deadline of the knob window
  uint8_t    _manPct = 0;            // knob duty the user asked for
  uint32_t   _sensFailSince = 0;
  uint32_t   _wtGoodSince = 0;      // weight-settle window start
  uint32_t   _lastTick = 0, _lastLog = 0;

  static LogRec _log[LOG_MAX];
  uint16_t _logN = 0, _logHead = 0;
};

extern SensorModule    sensors;
extern BatteryMonitor  battery;
extern Dryer           dryer;
extern Settings        cfg;
extern Weather         weather;
/* ==========================  src/cyclelog.h  ========================== */
/**
 * @file cyclelog.h
 * @brief Per-cycle history stored in LittleFS (survives power loss).
 *
 * Every drying cycle is written to its own CSV file in /cycles when the
 * cycle ends (completed / stopped by user / fault / battery empty), and
 * the website can list + download them. Oldest files are auto-pruned
 * beyond CYCLE_MAX_FILES.
 */
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
/* ==========================  src/webui.h  ========================== */
/**
 * @file webui.h
 * @brief The dryer dashboard, embedded in flash (PROGMEM).
 *
 * Two slides:
 *   [Dashboard ]  live dump of all data + gauges + multi-series graph + controls
 *   [Parameters]  EVERY setting editable on one page + one-click reset to
 *                 factory defaults + manually-set drying time + Save & Start
 *
 * 100 % offline: no CDN, no internet - the ESP32 hotspot IS the network.
 * Pro toolkit: SVG ring gauges, canvas chart engine with dual axes,
 * hover crosshair + tooltip, legend toggles, past-cycle graph viewer.
 *
 * NOTE FOR MAINTAINERS: many element ids and a few exact markup anchors
 * (the <nav> tag, the btnClearCyc controls block, 'use strict'; ) are
 * depended on by tools/online/sync-online.js and tools/ui-test - keep them.
 */


static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Dehumidifier</title>
<style>
:root{
--bg:#070b14;--bg2:#0a1120;--card:#0d1526;--card2:#111b30;--line:#1b2740;
--line2:#26365a;--tx:#eef3fb;--dim:#a9b6c9;--dim2:#7c8aa3;
--ok:#34d399;--warn:#e6c25a;--hot:#f87171;--cy:#4cc3ff;--am:#d4af37;
--acc:#3b82f6;--acc2:#1d4ed8;--vio:#a78bfa;--gold:#d4af37;--gold2:#f0d078;
--sh:0 10px 30px rgba(0,0,0,.45);--r:16px}
*{box-sizing:border-box;margin:0;padding:0}
html{-webkit-text-size-adjust:100%}
body{background:radial-gradient(1200px 500px at 85% -10%,#101d3a 0%,var(--bg) 55%),
 repeating-linear-gradient(180deg,rgba(158,175,199,.05) 0 1px,transparent 1px 4px),
 var(--bg);background-attachment:fixed;color:var(--tx);font:14px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
padding-bottom:56px;font-variant-numeric:tabular-nums}
button,input,select{font:inherit}
/* ---------- header ---------- */
header{display:flex;flex-wrap:wrap;gap:10px 16px;align-items:center;
justify-content:space-between;padding:16px 20px 14px;
border-bottom:1px solid var(--line);
background:linear-gradient(180deg,rgba(212,175,55,.07),transparent)}
.brand{display:flex;gap:12px;align-items:center}
.logo{width:40px;height:40px;border-radius:12px;display:grid;place-items:center;
font-size:20px;background:linear-gradient(135deg,#1e3a8a,#3b82f6);
box-shadow:inset 0 0 0 1px rgba(212,175,55,.5),var(--sh)}
header h1{font-size:17px;font-weight:700;letter-spacing:.2px}
header .sub{color:var(--dim);font-size:11.5px;margin-top:1px}
.hstat{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.pill{display:inline-flex;align-items:center;gap:7px;padding:6px 13px;border-radius:999px;
font-size:12px;font-weight:700;letter-spacing:.6px;border:1px solid var(--line2);
background:var(--card);color:var(--dim)}
.pill .dot{width:8px;height:8px;border-radius:50%;background:currentColor;
box-shadow:0 0 10px currentColor;animation:pulse 2s infinite}
@keyframes pulse{50%{opacity:.45}}
.p-idle{color:var(--dim)}.p-run{color:var(--ok);border-color:#1d4ed8}
.p-purge{color:var(--warn);border-color:#6b5410}.p-done{color:var(--acc);border-color:#1e3a8a}
.p-fault{color:var(--hot);border-color:#881337}.p-by{color:var(--warn)}
#clock{font-size:12.5px;color:var(--dim);border:1px solid var(--line);
border-radius:10px;padding:5px 11px;background:var(--bg2)}
/* ---------- tabs ---------- */
nav{display:flex;gap:8px;padding:14px 16px 4px;max-width:940px;margin:0 auto;
position:sticky;top:0;z-index:40;background:linear-gradient(180deg,var(--bg) 82%,transparent)}
nav button{flex:1;padding:12px 8px;border:2px solid rgba(76,195,255,.9);border-radius:13px;
font-size:13.5px;font-weight:750;cursor:pointer;transition:.18s;letter-spacing:.3px;
box-shadow:0 0 12px rgba(76,195,255,.5),inset 0 0 10px rgba(255,255,255,.22)}
nav button.fire{background:linear-gradient(120deg,#92400e,#f59e0b 30%,#fde68a 50%,#f59e0b 70%,#92400e);
background-size:200% 100%;color:#3b1d00}
nav button.water{background:linear-gradient(120deg,#1e3a8a,#3b82f6 30%,#bfdbfe 50%,#3b82f6 70%,#1e3a8a);
background-size:200% 100%;color:#061638}
nav button:hover{transform:translateY(-1px);filter:brightness(1.15);
box-shadow:0 0 16px rgba(76,195,255,.75),0 0 22px rgba(212,175,55,.3)}
nav button.on{animation:shine 3s linear infinite;
box-shadow:0 0 18px rgba(76,195,255,.85),0 0 28px rgba(212,175,55,.4),inset 0 0 14px rgba(255,255,255,.3)}
@keyframes shine{0%{background-position:0% 0}100%{background-position:200% 0}}
.bgsw{display:inline-block;width:27px;height:27px;border-radius:9px;margin:3px;cursor:pointer;
border:1px solid var(--line2);box-shadow:inset 0 0 7px rgba(255,255,255,.25),0 0 6px rgba(76,195,255,.25)}
body.nolines{background-image:radial-gradient(1200px 500px at 85% -10%,#101d3a 0%,var(--bg) 55%)}
/* ---------- layout ---------- */
section{display:none;padding:14px 16px;max-width:940px;margin:0 auto;animation:fadein .25s}
section.on{display:block}
@keyframes fadein{from{opacity:0;transform:translateY(4px)}to{opacity:1}}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(215px,1fr));gap:12px}
.card{background:linear-gradient(180deg,var(--card),var(--bg2));border:1px solid var(--line);
border-radius:var(--r);padding:15px 16px;box-shadow:var(--sh);position:relative;overflow:hidden}
.card h3{color:var(--dim);font-size:10.5px;text-transform:uppercase;letter-spacing:1.1px;
font-weight:700;margin-bottom:8px;display:flex;align-items:center;gap:7px}
.card h3 .sp{flex:1}
.big{font-size:31px;font-weight:750;letter-spacing:-.5px}
.unit{font-size:13px;color:var(--dim);font-weight:500}
.sml{font-size:11.5px;color:var(--dim);margin-top:5px}
.sml b{color:var(--tx);font-weight:650}
.dot{display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--ok);
margin-right:4px;box-shadow:0 0 8px rgba(52,211,153,.7)}
.dot.off{background:var(--hot);box-shadow:0 0 8px rgba(251,113,133,.7)}
/* ---------- gauges ---------- */
.gwrap{display:flex;align-items:center;gap:14px}
.gring{width:104px;height:104px;flex:none}
.gring .bgc{fill:none;stroke:#1c2836;stroke-width:9}
.gring .fgc{fill:none;stroke-width:9;stroke-linecap:round;
transition:stroke-dashoffset .8s cubic-bezier(.22,1,.36,1)}
.gring .mk{stroke:var(--tx);stroke-width:2.5;stroke-linecap:round;opacity:.85}
.gval{font-size:27px;font-weight:750;line-height:1.05}
.gmeta{font-size:11px;color:var(--dim);margin-top:3px}
/* ---------- bars ---------- */
.bar{height:9px;border-radius:6px;background:#0c1118;border:1px solid var(--line);
overflow:hidden;margin-top:9px}
.bar i{display:block;height:100%;border-radius:6px;transition:width .8s}
.bar.heat i{background:linear-gradient(90deg,#b45309,var(--hot));box-shadow:0 0 12px rgba(251,113,133,.45)}
.bar.fan i{background:linear-gradient(90deg,#0369a1,var(--cy));box-shadow:0 0 12px rgba(56,189,248,.4)}
/* ---------- controls ---------- */
.controls{display:flex;flex-wrap:wrap;gap:10px;margin:14px 0}
button.act{padding:12px 20px;border:none;border-radius:13px;font-size:14px;font-weight:750;
cursor:pointer;transition:.16s;letter-spacing:.3px;box-shadow:0 6px 16px rgba(0,0,0,.35)}
button.act:hover:not(:disabled){transform:translateY(-2px);filter:brightness(1.12)}
button.act:active:not(:disabled){transform:translateY(0)}
button.act:disabled{opacity:.32;cursor:not-allowed;box-shadow:none}
#btnStart{background:linear-gradient(135deg,#3b82f6,#1d4ed8);color:#fff;
box-shadow:0 6px 18px rgba(59,130,246,.35)}
#btnStop{background:linear-gradient(135deg,#e11d48,#be123c);color:#fff}
#btnPower{background:linear-gradient(135deg,#0284c7,#0369a1);color:#e0f2fe}
#btnAdd{background:var(--card2);color:var(--tx);border:1px solid var(--line2)}
/* ---------- chart ---------- */
.chartbox{background:linear-gradient(180deg,var(--card),var(--bg2));border:1px solid var(--line);
border-radius:var(--r);padding:15px;margin-top:12px;box-shadow:var(--sh)}
.chartbox>b{font-size:13px}
.legend{display:flex;flex-wrap:wrap;gap:6px;margin:10px 0 6px}
.lg{display:inline-flex;align-items:center;gap:6px;padding:5px 11px;border-radius:999px;
border:1px solid var(--line2);background:var(--bg2);color:var(--dim);font-size:11.5px;
font-weight:650;cursor:pointer;transition:.15s;user-select:none}
.lg:hover{color:var(--tx)}
.lg i{width:9px;height:9px;border-radius:3px;background:var(--c)}
.lg.on{color:var(--tx);border-color:color-mix(in srgb,var(--c) 55%,transparent);
background:color-mix(in srgb,var(--c) 14%,var(--bg2))}
canvas{width:100%;display:block;border-radius:10px}
#chart,#cycChart{height:250px}
.tip{position:absolute;pointer-events:none;background:rgba(10,15,21,.94);border:1px solid var(--line2);
border-radius:10px;padding:8px 11px;font-size:11.5px;line-height:1.7;opacity:0;
transition:opacity .12s;z-index:60;white-space:nowrap;box-shadow:var(--sh)}
.tip b{font-weight:700}
/* ---------- tables ---------- */
table{width:100%;border-collapse:collapse;font-size:13px}
td,th{padding:9px 8px;border-bottom:1px solid var(--line);text-align:left}
tr:last-child td{border-bottom:none}
td:last-child,th:last-child{text-align:right}
th{color:var(--dim2);font-size:10px;text-transform:uppercase;letter-spacing:1px}
tbody tr{transition:.12s}
tbody tr:hover{background:rgba(56,189,248,.05)}
td:last-child{font-weight:700}
.badge{display:inline-block;padding:3px 10px;border-radius:20px;font-size:11px;
font-weight:750;letter-spacing:.4px}
.b-idle{background:#1d2839;color:var(--dim)}.b-run{background:#14264d;color:#8ab4ff}
.b-purge{background:#3a3010;color:var(--gold2)}.b-done{background:#14264d;color:#8ab4ff}
.b-fault{background:#3d1420;color:var(--hot)}.b-by{background:#3a2f10;color:var(--warn)}
a.dl{color:var(--cy);font-size:13px;text-decoration:none;font-weight:650}
a.dl:hover{text-decoration:underline}
.vbtn{background:var(--card2);border:1px solid var(--line2);color:var(--cy);border-radius:9px;
padding:5px 10px;font-size:11.5px;font-weight:700;cursor:pointer}
.vbtn:hover{border-color:var(--cy)}
/* ---------- forms ---------- */
form{background:none;border:none;padding:0}
.fsec{background:linear-gradient(180deg,var(--card),var(--bg2));border:1px solid var(--line);
border-radius:var(--r);padding:15px;margin-bottom:12px;box-shadow:var(--sh)}
.fsec>h4{font-size:11px;text-transform:uppercase;letter-spacing:1.1px;color:var(--acc);
margin-bottom:11px;display:flex;gap:8px;align-items:center}
.frow{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px;margin-bottom:10px}
.frow:last-child{margin-bottom:0}
label{font-size:11px;color:var(--dim);display:block;margin-bottom:5px;font-weight:600;
letter-spacing:.2px}
input,select{width:100%;padding:10px 12px;border-radius:11px;border:1px solid var(--line2);
background:#0b1119;color:var(--tx);font-size:14.5px;transition:.15s}
input:focus,select:focus{outline:none;border-color:var(--acc);
box-shadow:0 0 0 3px rgba(45,212,191,.15)}
input[type=number]::-webkit-inner-spin-button{opacity:.4}
details{margin:10px 0 0;padding:11px 13px;border:1px dashed var(--line2);border-radius:12px;
background:rgba(0,0,0,.12)}
summary{color:var(--dim);cursor:pointer;font-size:12.5px;font-weight:600}
summary:hover{color:var(--tx)}
.chk{display:flex;align-items:center;gap:11px;font-size:13.5px;color:var(--tx);
padding:10px 2px;cursor:pointer;user-select:none}
.chk input{appearance:none;-webkit-appearance:none;width:42px;height:24px;border-radius:999px;
background:#233044;border:1px solid var(--line2);position:relative;cursor:pointer;
transition:.2s;flex:none}
.chk input::after{content:"";position:absolute;top:2px;left:2px;width:18px;height:18px;
border-radius:50%;background:#8aa0b4;transition:.2s}
.chk input:checked{background:linear-gradient(135deg,var(--acc),var(--acc2));
border-color:transparent}
.chk input:checked::after{left:20px;background:#04211c}
.chk input:checked+span{color:var(--tx)}
/* ---------- misc ---------- */
.fault{background:linear-gradient(135deg,#3d1420,#250a12);border:1px solid #881337;
color:#fda4af;padding:12px 15px;border-radius:13px;margin-bottom:12px;display:none;
font-weight:650;box-shadow:var(--sh)}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%) translateY(8px);
background:linear-gradient(135deg,var(--acc),var(--acc2));color:#03271d;padding:11px 22px;
border-radius:999px;font-weight:750;font-size:13.5px;opacity:0;transition:.25s;
pointer-events:none;z-index:99;box-shadow:0 10px 30px rgba(45,212,191,.35)}
#toast.err{background:linear-gradient(135deg,#e11d48,#be123c);color:#fff}
.note{font-size:11.5px;color:var(--dim2);margin-top:8px}
.note b{color:var(--dim)}
.wxbox{display:flex;gap:13px;align-items:flex-start}
.wxicon{font-size:37px;line-height:1;filter:drop-shadow(0 4px 10px rgba(0,0,0,.4))}
.wxrows{font-size:12px;color:var(--dim);line-height:1.85;margin-top:2px}
.wxrows b{color:var(--tx);font-weight:650}
/* v2.0.19: virtual keypad drawer (touch displays) */
#vpDrawer{display:none;position:fixed;top:0;right:0;bottom:0;width:min(400px,100vw);
background:var(--card);border-left:1px solid var(--line2);box-shadow:var(--sh);
z-index:80;flex-direction:column;padding:14px;gap:10px;overflow:auto}
#vpDrawer.on{display:flex}
#vpHead{display:flex;justify-content:space-between;align-items:center}
#vpText{flex:1;overflow:auto;font-size:13px;line-height:2;color:var(--dim);
border:1px dashed var(--line2);border-radius:10px;padding:10px;min-height:110px}
.vpRow{display:flex;justify-content:space-between;gap:10px}
.vpSel{color:var(--tx);font-weight:700}
.vpPad{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.vpPad button{padding:12px 0;font-size:17px;font-weight:700;border-radius:12px;
border:1px solid var(--line2);background:#101923;color:var(--tx);cursor:pointer}
.vpPad button:active{transform:scale(.93);background:#182535}
.vpPad button small{display:block;font-size:9px;font-weight:600;color:var(--dim)}
@media(max-width:560px){.big{font-size:26px}.gval{font-size:23px}
header{padding:13px 14px}nav button{font-size:12.5px;padding:10px 4px}}
</style></head><body>
<header>
  <div class="brand"><div class="logo">&#127807;</div>
    <div><h1 id="brand">ARCHITECTS OF SOLUTIONS</h1>
    <div class="sub">v<span id="fw">--</span> &middot; solar &middot; offline &middot; yours &mdash; 192.168.4.1</div></div></div>
  <div class="hstat">
    <span id="stBadge" class="pill p-idle"><span class="dot"></span>IDLE</span>
    <span id="clock">&#9201; --</span>
    <span id="doorPill" class="pill p-purge" style="display:none">&#128274; CALIBRATE</span>
    <span id="supPill" class="pill p-purge">&#9728; SOLAR</span>
    <a href="/display" target="_blank" class="pill p-run" style="text-decoration:none">&#128421; Display view</a>
    <button class="pill" onclick="vpOpen()" title="Virtual 4x4 keypad - the on-device menu, by touch">&#9000; Keypad</button>
    <span style="position:relative">
      <button class="pill" onclick="toggleBg()" title="Change background">&#127912;</button>
      <div id="bgPanel" style="display:none;position:absolute;right:0;top:44px;z-index:60;
        background:var(--card);border:1px solid var(--line2);border-radius:14px;padding:12px;
        box-shadow:var(--sh);width:236px">
        <div style="font-size:12px;color:var(--dim);margin-bottom:8px;font-weight:700">BACKGROUND</div>
        <div id="bgSwatches"></div>
        <label style="display:flex;gap:7px;align-items:center;font-size:12px;color:var(--dim);margin-top:9px">
          <input type="checkbox" id="bgLines" onchange="bgSet()"> hair lines</label>
        <label style="display:flex;gap:7px;align-items:center;font-size:12px;color:var(--dim);margin-top:6px">
          custom colour <input type="color" id="bgColor" onchange="bgSet(this.value)"></label>
      </div>
    </span>
  </div>
</header>

<nav>
  <button id="tabDash" class="on fire" onclick="showTab('Dash')">&#128293; Dashboard</button>
  <button id="tabSet" class="water" onclick="showTab('Set')">&#128167; Parameters</button>
</nav>

<!-- ================= SLIDE: DASHBOARD ================= -->
<section id="secDash" class="on">
  <div id="faultBox" class="fault"></div>
  <div id="warnBox" class="fault" style="background:linear-gradient(135deg,#3a2f10,#241d08);border-color:#a16207;display:none"></div>
  <div id="doneCard" class="fault" style="background:linear-gradient(135deg,#0d2818,#05140b);border-color:#166534;display:none"></div>
  <div class="controls">
    <button class="act" id="btnStart" onclick="api('/api/start')">&#9654; Start</button>
    <button class="act" id="btnStop" onclick="api('/api/stop')">&#9632; Stop</button>
    <button class="act" id="btnAdd" onclick="api('/api/addtime?min=15')">+15 min</button>
    <button class="act" id="btnPower" onclick="api('/api/power')">&#9211; Power On</button>
  </div>
  <!-- v2.0.18: cycle-data downloads right on the home page -->
  <div class="note" style="margin:8px 0 0">&#128190; <b>Cycle data:</b>
    <a class="dl" href="/eelog.csv" download>&#11015; ALL cycles &mdash; EEPROM registry (<span id="eeN2">--</span>)</a>
    &nbsp;&middot;&nbsp;
    <a class="dl" href="/lastcycle.csv" download>&#11015; latest cycle &mdash; full detail</a>
    &nbsp;&middot;&nbsp; per-batch files: &ldquo;Past cycles&rdquo; below</div>

  <div class="grid">
    <!-- gauge: temperature -->
    <div class="card"><h3>&#127777; Chamber temp</h3>
      <div class="gwrap">
        <svg class="gring" viewBox="0 0 120 120">
          <circle class="bgc" cx="60" cy="60" r="52"/>
          <circle class="fgc" id="g_t" cx="60" cy="60" r="52" stroke="#d4af37"
            stroke-dasharray="326.7" stroke-dashoffset="326.7"
            transform="rotate(-90 60 60)"/>
          <line class="mk" id="g_tmark" x1="60" y1="6" x2="60" y2="17"
            transform="rotate(0 60 60)" style="display:none"/>
        </svg>
        <div><div class="gval"><span id="tAvg">--</span><span class="unit"> &deg;C</span></div>
        <div class="gmeta">target <b id="setT">--</b> &deg;C</div></div>
      </div>
      <div class="sml" style="margin-top:9px"><span class="dot" id="d1"></span>S1 <b id="t1">--</b>&deg;
      &nbsp; <span class="dot" id="d2"></span>S2 <b id="t2">--</b>&deg;</div>
    </div>
    <!-- gauge: humidity -->
    <div class="card"><h3>&#128167; Humidity</h3>
      <div class="gwrap">
        <svg class="gring" viewBox="0 0 120 120">
          <circle class="bgc" cx="60" cy="60" r="52"/>
          <circle class="fgc" id="g_h" cx="60" cy="60" r="52" stroke="#4cc3ff"
            stroke-dasharray="326.7" stroke-dashoffset="326.7"
            transform="rotate(-90 60 60)"/>
          <line class="mk" id="g_hlo" x1="60" y1="6" x2="60" y2="17" style="display:none"/>
          <line class="mk" id="g_hhi" x1="60" y1="6" x2="60" y2="17" style="display:none"/>
        </svg>
        <div><div class="gval"><span id="hAvg">--</span><span class="unit"> %RH</span></div>
        <div class="gmeta">band <b id="setH">--</b> %RH</div></div>
      </div>
      <div class="sml" style="margin-top:9px">S1 <b id="h1">--</b> &middot; S2 <b id="h2">--</b>
      &middot; peak <b id="hMax">--</b></div>
    </div>
    <!-- gauge: battery -->
    <div class="card"><h3>&#128267; Battery</h3>
      <div class="gwrap">
        <svg class="gring" viewBox="0 0 120 120">
          <circle class="bgc" cx="60" cy="60" r="52"/>
          <circle class="fgc" id="g_b" cx="60" cy="60" r="52" stroke="#5fa8ff"
            stroke-dasharray="326.7" stroke-dashoffset="326.7"
            transform="rotate(-90 60 60)"/>
        </svg>
        <div><div class="gval"><span id="bat">--</span><span class="unit"> %</span></div>
        <div class="gmeta"><b id="bv">--</b> V &middot; <span id="batType">--</span></div></div>
      </div>
      <div class="sml" style="margin-top:9px">safe shutdown &le; <b id="setCut">--</b> %</div>
    </div>
    <!-- weigh scale -->
    <div class="card"><h3>&#9878; Batch weight <span class="sp"></span><span id="wtBadge"
      class="badge b-idle" style="display:none">&#10004; SETTLED</span></h3>
      <div class="big"><span id="wt">--</span><span class="unit"> g</span></div>
      <div class="sml" id="wtTgt">no target set</div>
      <div class="sml">losing <b id="wtRate">--</b> g/min &middot; dry-to-weight
      <b id="wtMode">off</b></div>
      <div style="margin-top:8px;display:flex;gap:8px">
        <button class="act" style="flex:1;padding:9px" onclick="espFetch('/api/scale?tare=1',{method:'POST'}).then(r=>toast(r.ok?'Tared':'Error')).catch(()=>toast('Offline',1))">&#9878; Tare</button>
        <input type="number" id="calG" placeholder="known g" step="10" min="10"
               style="width:90px;background:var(--bg2);border:1px solid var(--line2);border-radius:10px;color:var(--tx);padding:6px 8px">
        <button class="act" style="flex:1;padding:9px" onclick="scaleCal()">&#9878; Calibrate</button>
      </div>
      <div class="sml">put a known weight on the trays, type its grams, Calibrate</div>
    </div>
    <!-- heater -->
    <div class="card"><h3>&#128293; Heater <span class="sp"></span><span id="boostBadge"
      class="badge b-purge" style="display:none">&#11014; MAX HEAT-UP</span></h3>
      <div class="big"><span id="heat">--</span><span class="unit"> % duty</span></div>
      <div class="bar heat"><i id="heatBar" style="width:0%"></i></div>
      <div style="margin-top:8px;display:flex;align-items:center;gap:8px">
        <input type="range" id="heatKnob" min="0" max="100" step="5" value="0"
               style="flex:1;accent-color:var(--gold);height:34px"
               oninput="knobDrag()" onchange="knobSend()">
        <b id="knobPct" style="min-width:3.2em;text-align:right">--</b>
        <span class="badge b-run" id="manBadge">AUTO</span>
      </div>
      <div class="sml">Drag the knob to drive the BTS yourself (while RUNNING) &middot;
      the MCU takes back automatic control after <b>60 s</b> &middot;
      over-temperature cut stays active &middot; PID cap <b id="setCap">--</b> %</div>
    </div>
    <!-- fans -->
    <div class="card"><h3>&#127744; Fans <span class="sp"></span>L298N</h3>
      <div class="big"><span id="fan">--</span><span class="unit"> % demand</span></div>
      <div class="bar fan"><i id="fanBar" style="width:0%"></i></div>
      <div class="sml">outlet <b id="fanOut">--</b>% (burst venting)
      (gentle breeze)</div>
    </div>
    <!-- cycle -->
    <div class="card"><h3>&#9201; Cycle</h3>
      <div class="big"><span id="rem">--:--</span></div>
      <div class="sml">elapsed <b id="el">--</b> &middot; set <b id="setD">--</b> min</div>
      <div class="sml">purge <b id="setPurge">--</b> s before power cut</div>
    </div>
    <!-- weather -->
    <div class="card" style="grid-column:1/-1"><h3>&#127787; Outdoor
      <span class="sp"></span><span id="wxBadge" class="badge b-idle">--</span></h3>
      <div class="wxbox">
        <div class="wxicon" id="wxIcon"></div>
        <div style="flex:1">
          <div class="big" style="font-size:24px"><span id="wxT">--</span><span class="unit">
          &deg;C &middot; RH </span><span id="wxH" class="unit" style="font-size:18px">--</span><span class="unit">%</span></div>
          <div class="wxrows">rain <b id="wxR">--</b>% &middot; wind <b id="wxW">--</b> km/h
          &middot; <span id="wxWhen">no data yet</span></div>
          <div class="sml" id="wxOut" style="margin-top:4px"></div>
          <details class="note" style="margin-top:8px"><summary>location &amp; manual entry</summary>
            <div class="frow" style="margin-top:9px">
              <div><label>Town / city (for live fetch)</label><input id="wxCity" placeholder="e.g. Razam"></div>
              <div><label>&nbsp;</label><button class="act" onclick="saveCity()"
                style="width:100%;background:var(--card2);color:var(--tx);border:1px solid var(--line2)">Save</button></div>
            </div>
            <div class="frow">
              <div><label>Manual temp (&deg;C)</label><input type="number" id="wxMt" step="0.5"></div>
              <div><label>Manual RH (%)</label><input type="number" id="wxMh" step="1" min="0" max="100"></div>
              <div><label>&nbsp;</label><button class="act" onclick="sendManualWx()" style="width:100%">Send</button></div>
            </div>
            <div id="wxDays" style="margin-top:9px"></div>
            <div class="note">Next 5 days above: typical values estimated from the date &amp; time.
            Live weather (badge LIVE) is fetched by your phone's browser and pushed to the ESP;
            no phone online? Type values manually.</div>
          </details>
        </div>
      </div>
    </div>
  </div>

  <!-- live multi-series chart -->
  <div class="chartbox" style="position:relative">
    <b>&#128200; Live trend</b>
    <div class="legend" id="legend"></div>
    <canvas id="chart"></canvas>
    <div class="tip" id="chartTip"></div>
  </div>

  <!-- past cycle viewer -->
  <div class="chartbox" id="cycView" style="display:none;margin-top:12px;position:relative">
    <b id="cycTitle">&#128230; Cycle</b>
    <button class="vbtn" style="float:right" onclick="closeCycle()">&#10005; close</button>
    <div class="legend" id="cycLegend"></div>
    <canvas id="cycChart"></canvas>
  </div>

  <!-- past cycles -->
  <div class="chartbox">
    <b>&#128230; Past cycles</b> <span class="note">(saved in ESP flash &mdash; survive power loss)</span>
    <div style="overflow-x:auto;margin-top:8px"><p class="note">Long-term registry (AT24C256 EEPROM): <b id="eeN">--</b> cycles stored &middot; <a class="dl" href="/eelog.csv" download>&#11015; Download ALL cycle summaries (CSV)</a></p>
<table id="cycTable">
      <tr><th>Started</th><th>Duration</th><th>Status</th><th>Time left</th><th>CSV</th><th></th></tr>
    </table></div>
    <div class="controls" style="margin:10px 0 0">
      <button class="act" id="btnClearCyc" onclick="clearCycles()"
        style="background:#3d1420;color:#fda4af">&#128465; Clear history</button></div>
    <div class="note">Download shows every reading of that run: time, temp, RH, heater %,
      fan %, battery. Status: <b style="color:var(--ok)">completed</b> &middot;
      <b style="color:var(--warn)">stopped</b> &middot; <b style="color:var(--hot)">fault</b>
      &middot; press <b>view</b> to graph any run</div>
  </div>

  <div class="note"><a class="dl" href="/api/log.csv" download>&#11015; Download this run (CSV)</a>
  &middot; <span id="logN">0</span> records &middot; free heap <span id="heap">--</span> B
  &middot; keypad last key <b id="kpLast">&#8211;</b>
  (A start &middot; B stop &middot; C power on &middot; D +15 min)
  &middot; <a class="dl" href="/update">&#11014; Firmware update</a> (OTA)
  &middot; <a class="dl" href="/online">&#127760; online version</a> (UI from
  GitHub Pages, needs internet on your phone - falls back to this page)</div>
</section>

<!-- ================= SLIDE 1: DEFAULTS ================= -->
<!-- ================= SLIDE: PARAMETERS (edit + one-click defaults) ====== -->
<section id="secSet">
  <h2 style="margin-bottom:4px;font-size:17px">&#9881; Parameters &mdash; all on one page</h2>
  <p class="note" style="margin-bottom:12px">Change any value, then <b>Save</b> (or <b>Save &amp; Start</b>).
  One click puts every parameter back to the factory defaults.</p>
  <div class="controls" style="margin-bottom:12px">
    <button class="act" id="btnDef" onclick="applyDefaults()">&#8635; Reset all to defaults</button>
  </div>
  <form onsubmit="return false" id="cfgForm">
    <div class="fsec"><h4>&#127777; Temperature</h4>
      <div class="frow" style="margin-bottom:9px">
        <div style="flex:2"><label>Mode preset (C key / BUTTON-2 on the unit)</label>
          <div class="frow" style="gap:8px">
            <button type="button" class="tabbtn" data-mdbtn="0" onclick="setMode(0)">AGARBATTI 60&deg;C</button>
            <button type="button" class="tabbtn" data-mdbtn="1" onclick="setMode(1)">USER DEFINED</button>
            <button type="button" class="tabbtn" data-mdbtn="2" onclick="setMode(2)">SILICAGEL 80&deg;C</button>
          </div></div>
      </div>
      <div class="frow">
        <div><label>Target temperature (&deg;C, max 80)</label><input type="number" id="f_setTemp" step="0.5" min="40" max="80"></div>
        <div><label>Control band &plusmn; (&deg;C)</label><input type="number" id="f_tempHyst" step="0.1" min="0.2" max="5"></div>
        <div><label>Safety cutoff (&deg;C)</label><input type="number" id="f_maxTemp" step="1" min="35" max="110"></div>
      </div>
      <label class="chk"><input type="checkbox" id="f_boostHeat"><span>Full-power heat-up &mdash; BTS at MAX output until the chamber reaches target, then PID holds it</span></label></div>
    <div class="fsec"><h4>&#128167; Humidity &amp; airflow</h4>
      <div class="frow">
        <div><label>Humidity &ndash; fans ramp above (%RH)</label><input type="number" id="f_humHigh" step="1" min="20" max="95"></div>
        <div><label>Humidity &ndash; fans stop below (%RH)</label><input type="number" id="f_humLow" step="1" min="10" max="80"></div>
        <div><label>Target RH for "dry" (%RH)</label><input type="number" id="f_humTarget" step="1" min="5" max="70"></div>
      </div>
      <label class="chk"><input type="checkbox" id="f_requireHum"><span>Cycle only finishes when target RH is also reached (else time alone decides)</span></label>
      <label class="chk"><input type="checkbox" id="f_smartVent"><span>Outdoor-aware venting &mdash; pause venting when outside air is wetter than the chamber (needs weather data)</span></label>
      <div class="frow" style="margin-top:9px">
        <div><label>Minimum fan speed (%)</label><input type="number" id="f_fanMin" step="1" min="0" max="60"></div>
        <div><label>Heater power cap (%)</label><input type="number" id="f_heaterMax" step="5" min="10" max="100"></div>
      </div>
      <div class="frow">
        <div><label>Target batch weight (g, 0 = off)</label><input type="number" id="f_targetG" step="50" min="0" max="9000"></div>
        <div><label>Fan fires when RH &ge; (%)</label><input type="number" id="f_fanTrigRH" step="5" min="30" max="90"></div>
        <div><label>RH high for (min) before fan</label><input type="number" id="f_fanTrigMin" step="1" min="1" max="10"></div>
        <div><label>Fan burst length (s)</label><input type="number" id="f_fanBurstS" step="10" min="10" max="300"></div>
        <div><label>Exhaust fan speed (%)</label><input type="number" id="f_fanOut" step="5" min="10" max="100"></div>
        <div><label>Ramp per RH point (%/RH)</label><input type="number" id="f_fanSlope" step="1" min="1" max="12"></div>
      </div>
      <div class="note" style="margin:2px 0 8px">&#127788; Gentle recipe: intake 40&ndash;60, exhaust 60&ndash;80,
      min 5&ndash;10, slope 2&ndash;3 &mdash; a soft breeze that takes the humidity out without cooling the sticks.</div></div>
    <div class="fsec"><h4>&#9201; Drying time &amp; end of cycle</h4>
      <div class="frow">
        <div><label>Drying time &ndash; hours</label><input type="number" id="f_hrs" step="1" min="0" max="24" value="2"></div>
        <div><label>Drying time &ndash; minutes</label><input type="number" id="f_min" step="5" min="0" max="59" value="0"></div>
        <div><label>Purge before power cut (s)</label><input type="number" id="f_cooldownSec" step="5" min="10" max="600"></div>
      </div></div>
    <div class="fsec"><h4>&#128267; Battery</h4>
      <div class="frow">
        <div><label>Battery type</label><select id="f_battType">
          <option value="0">3S Li-ion (12.6 V)</option><option value="1">4S Li-ion (16.8 V)</option>
          <option value="2">12 V SLA</option><option value="3">4S LiFePO4</option></select></div>
        <div><label>Safe shutdown below (%)</label><input type="number" id="f_cutoffPct" step="1" min="0" max="40"></div>
      </div></div>
    <div class="fsec"><h4>&#9878; Dry to weight (weigh scale)</h4>
      <div class="frow">
        <label class="chk"><input type="checkbox" id="f_requireWeight"><span>End the cycle when the batch stops losing weight (needs the HX711 scale fitted)</span></label>
      </div>
      <div class="frow">
        <div><label>Settled below (g/min)</label><input type="number" id="f_weightRateG" step="0.5" min="0.5" max="50"></div>
        <div><label>Stable for (min)</label><input type="number" id="f_weightMinY" step="1" min="2" max="120"></div>
      </div></div>
    <div class="fsec"><h4>&#9200; Clock &mdash; date &amp; time (cycle history stamps)</h4>
      <p class="note" style="margin:2px 0 10px">The dashboard automatically pushes your phone's
      clock to the ESP32 whenever it connects &mdash; usually nothing to do here. You can also set it
      manually below, tap the clock on the <b>/display</b> page, or use keypad menu rows <b>6/7</b>.</p>
      <div class="frow">
        <div><label>Set date &amp; time manually</label><input type="datetime-local" id="f_dt"></div>
        <div><label>&nbsp;</label><button class="act" onclick="setClockManual()"
          style="width:100%;background:linear-gradient(135deg,#0284c7,#0369a1);color:#e0f2fe">&#9201; Set clock</button></div>
        <div><label>Timezone offset (min)</label><input type="number" id="f_tz" step="15" min="-720" max="840"></div>
      </div></div>
    <div class="fsec"><h4>&#127987; Batch calculator &mdash; sticks &amp; paste (fills the target weight)</h4>
      <p class="note" style="margin:2px 0 10px">Moisture is a property of the <b>paste</b>:
      raw mix is typically <b>30&ndash;40&nbsp;% water</b>, finished agarbatti wants
      <b>8&ndash;10&nbsp;%</b>. Enter the batch below and the target weight is computed:
      <code>target&nbsp;=&nbsp;sticks&nbsp;&times;&nbsp;wet&nbsp;g&nbsp;&times;&nbsp;(1&nbsp;&minus;&nbsp;(paste&nbsp;%&nbsp;&minus;&nbsp;final&nbsp;%)/100)</code>.
      Example: 2.5&nbsp;g stick, 35&nbsp;%&nbsp;&rarr;&nbsp;10&nbsp;% = 1.875&nbsp;g dry
      (0.625&nbsp;g water per stick). Leave sticks at 0 to type the target by hand.</p>
      <div class="frow">
        <div><label>Sticks in batch (0 = off)</label><input type="number" id="f_stickCount" step="10" min="0" max="3000" oninput="stickCalc()"></div>
        <div><label>Avg wet stick (g)</label><input type="number" id="f_stickWetG" step="0.1" min="0.5" max="20" oninput="stickCalc()"></div>
        <div><label>Paste water %</label><input type="number" id="f_pasteWater" step="1" min="5" max="60" oninput="stickCalc()"></div>
        <div><label>Final moisture %</label><input type="number" id="f_targetMoist" step="1" min="3" max="20" oninput="stickCalc()"></div>
      </div>
      <p class="note" id="stickPrev" style="margin:2px 0 0">&mdash;</p></div>
    <div class="fsec"><h4>&#9878; Advanced &mdash; heater PID</h4>
      <div class="frow">
        <div><label>Kp (%/&deg;C)</label><input type="number" id="f_kp" step="0.5" min="0" max="100"></div>
        <div><label>Ki (%/&deg;C&middot;s)</label><input type="number" id="f_ki" step="0.01" min="0" max="10"></div>
        <div><label>Kd (%/&deg;C&middot;s&#8315;&sup1;)</label><input type="number" id="f_kd" step="0.5" min="0" max="100"></div>
      </div></div>
  </form>
  <div class="controls">
    <button class="act" id="btnSave" onclick="saveCfg(false)">&#128190; Save</button>
    <button class="act" id="btnGo" onclick="saveCfg(true)">&#9654; Save &amp; Start drying</button>
  </div>
  <details class="card" style="margin-top:14px;padding:12px 14px">
    <summary style="cursor:pointer;font-weight:700;color:var(--dim)">Factory defaults (reference)</summary>
    <table id="defTable" style="margin-top:10px"></table>
  </details>
</section>

<!-- v2.0.19: virtual keypad drawer - same keys as the pillar keypad -->
<div id="vpDrawer">
  <div id="vpHead"><b style="font-size:14px">&#9000; Virtual keypad</b>
    <button class="pill" onclick="vpClose()">&#10005;</button></div>
  <div id="vpText"></div>
  <div class="vpPad">
    <button onclick="vpKey('1')">1</button><button onclick="vpKey('2')">2<small>&#9650;</small></button><button onclick="vpKey('3')">3</button><button onclick="vpKey('A')">A<small>OK</small></button>
    <button onclick="vpKey('4')">4<small>&#9664;</small></button><button onclick="vpKey('5')">5</button><button onclick="vpKey('6')">6<small>&#9654;</small></button><button onclick="vpKey('B')">B<small>MENU</small></button>
    <button onclick="vpKey('7')">7</button><button onclick="vpKey('8')">8<small>&#9660;</small></button><button onclick="vpKey('9')">9</button><button onclick="vpKey('C')">C<small>MODE</small></button>
    <button onclick="vpKey('*')">*</button><button onclick="vpKey('0')">0</button><button onclick="vpKey('#')">#<small>BACK</small></button><button onclick="vpKey('D')">D<small>RUN</small></button>
  </div>
  <div class="note">digits type &middot; 2/4/6/8 move &middot; <b>A</b> OK &middot; <b>B</b> menu &middot; <b>C</b> mode &middot; <b>D</b> run/stop &middot; <b>*</b> home &middot; <b>#</b> back</div>
</div>
<div id="toast"></div>
<script>
'use strict';
/* espFetch: the online layer (injected by tools/online/sync-online.js into the
   GitHub-Pages build) provides espFetchBridge; standalone we use plain fetch. */
var espFetch=(typeof espFetchBridge!=='undefined')?espFetchBridge:function(u,o){return fetch(u,o)};
let S=null,DEF=null,T=[];           // status, defaults, chart series
const $=id=>document.getElementById(id);
function toast(m,e){const t=$('toast');t.textContent=m;t.className=e?'err':'';t.style.opacity=1;
  t.style.transform='translateX(-50%) translateY(0)';
  setTimeout(()=>{t.style.opacity=0;t.style.transform='translateX(-50%) translateY(8px)'},2200)}
function showTab(n){for(const s of['Dash','Set']){$('tab'+s).classList.toggle('on',s===n);
  $('sec'+s).classList.toggle('on',s===n)}if(n==='Set'){if(S)fillForm(S.set);if(DEF)fillDef()}}
const f2=(v,d=1)=>v==null||isNaN(v)?'--':(+v).toFixed(d);
const mmss=s=>{s=Math.max(0,s|0);const h=(s/3600)|0,m=((s%3600)/60)|0,x=s%60;
  return h>0?h+'h '+String(m).padStart(2,'0')+'m':String(m).padStart(2,'0')+':'+String(x).padStart(2,'0')};

async function api(url){try{const r=await espFetch(url,{method:'POST'});
  toast(r.ok?'OK':'Error '+(await r.text()),!r.ok)}catch(e){toast('Offline',1)}}

// one click: apply factory defaults on the ESP, then refresh this page
async function applyDefaults(){
  try{
    const r=await espFetch('/api/defaults',{method:'POST'});
    if(!r.ok){toast('Error '+(await r.text()),1);return}
    const j=await(await espFetch('/api/data')).json();
    S=j;DEF=j.defs||DEF;fillForm(S.set);fillDef();toast('Defaults applied')
  }catch(e){toast('Offline',1)}}

// ---------- background personalisation (persists on this phone) ---------
var bgCur={c:'#070b14',lines:true};
const BGP=[{n:'Navy',c:'#070b14'},{n:'Midnight',c:'#0b1026'},{n:'Deep Blue',c:'#0a1a33'},
           {n:'Graphite',c:'#14161a'},{n:'Warm Dark',c:'#1c1508'},{n:'Royal',c:'#101a3f'}];
function bgApply(c,lines){bgCur={c:c,lines:lines};
  document.body.style.setProperty('--bg',c);
  document.body.classList.toggle('nolines',!lines);
  try{localStorage.setItem('dryerBg',JSON.stringify(bgCur))}catch(e){}}
function bgSet(custom){var c=custom||bgCur.c;bgApply(c,$('bgLines').checked)}
function toggleBg(){var p=$('bgPanel');p.style.display=p.style.display==='none'?'block':'none'}
function initBg(){
  try{var p=JSON.parse(localStorage.getItem('dryerBg'));if(p&&p.c)bgCur=p}catch(e){}
  $('bgSwatches').innerHTML=BGP.map(b=>
    '<span class="bgsw" style="background:'+b.c+'" title="'+b.n+
    '" onclick="bgApply(\''+b.c+'\',bgCur.lines)"></span>').join('');
  $('bgLines').checked=bgCur.lines;$('bgColor').value=bgCur.c;
  bgApply(bgCur.c,bgCur.lines)}

// ---------- manual heat knob (override for 60 s, then auto again) --------
var knobTmr=0;
function knobDrag(){                      // while dragging: live readout,
  var v=+$('heatKnob').value;             // throttled send so the ESP is
  $('knobPct').textContent=v+'%';         // not flooded
  $('manBadge').textContent='MANUAL\u2026';
  $('manBadge').className='badge b-purge';
  if(knobTmr)clearTimeout(knobTmr);
  knobTmr=setTimeout(knobSend,400);
}
async function setMode(m){
  await api('/api/mode?m='+m);
  var names=['AGARBATTI DEFAULT','USER DEFINED','SILICAGEL DEFAULT'];
  toast('Mode: '+names[m]+(m!==1?' applied (60/80\u00B0C, 120 min)':''));
  loadCfg();
}
function scaleCal(){                       // calibrate with a known weight
  var g=parseFloat($('calG').value);
  if(!(g>0)){toast('type the known weight first',1);return}
  espFetch('/api/scale?cal='+g,{method:'POST'})
    .then(r=>toast(r.ok?'Calibrated':'Error '+(r.ok?'':r.status),!r.ok))
    .catch(function(){toast('Offline',1)});
}

function knobSend(){                      // release / pause -> commit now
  knobTmr=0;
  var v=+$('heatKnob').value;
  espFetch('/api/heat?d='+v,{method:'POST'}).catch(function(){});
}

// ---------- live date & time (browser pushes its clock automatically) ----
let baseSec=0,baseMs=0,lastSync=0,lastState='';
const p2=n=>String(n).padStart(2,'0');
function fmtClock(sec){const d=new Date(sec*1000);
  return d.getUTCFullYear()+'-'+p2(d.getUTCMonth()+1)+'-'+p2(d.getUTCDate())+' '
        +p2(d.getUTCHours())+':'+p2(d.getUTCMinutes())+':'+p2(d.getUTCSeconds())}
function tickClock(){if(!baseSec)return;
  $('clock').textContent='\u23F1 '+fmtClock(baseSec+(Date.now()-baseMs)/1000)}
function syncClock(){                       // "takes initiative" from the phone
  const d=new Date();if(d.getFullYear()<2021)return;
  espFetch('/api/settime?epoch='+Math.floor(d.getTime()/1000)
       +'&tz='+(-d.getTimezoneOffset()),{method:'POST'}).catch(()=>{});
  lastSync=Date.now()}
function stickCalc(){                     // sticks & paste -> target weight
  const n=+$('f_stickCount').value||0, w=+$('f_stickWetG').value||0,
        p=+$('f_pasteWater').value||35, f=+$('f_targetMoist').value||10;
  const el=$('stickPrev');
  if(!(n>0)||!(w>=0.5)){el.innerHTML='\u2014 calculator off: target weight is typed by hand';return}
  const loss=Math.min(Math.max((p-f)/100,0),0.8), dry=w*(1-loss),
        wet=n*w, tgt=wet*(1-loss), water=wet-tgt;
  $('f_targetG').value=Math.round(tgt);
  el.innerHTML='\u{1FA7A} per stick: <b>'+w.toFixed(1)+' g wet \u2192 '+dry.toFixed(2)+
   ' g dry</b> ('+(w-dry).toFixed(2)+' g water) &middot; batch: <b>'+wet.toFixed(0)+
   ' g wet \u2192 '+tgt.toFixed(0)+' g target</b> &middot; total water to remove: <b>'+
   water.toFixed(0)+' g</b> ('+(water/1000).toFixed(2)+' L)';}
function setClockManual(){const v=$('f_dt').value;if(!v){toast('Pick a date & time first',1);return}
  const ep=Math.floor(new Date(v).getTime()/1000);
  if(isNaN(ep)){toast('Bad date',1);return}
  espFetch('/api/settime?epoch='+ep+'&tz='+(parseInt($('f_tz').value||'330')),{method:'POST'})
    .then(()=>toast('Clock set')).catch(()=>toast('Offline',1))}

// ---------- chart engine (dual axis, toggles, hover tooltip) -------------
const SER={temp:{c:'#e3b341',lab:'Temp \u00B0C',ax:0,get:p=>p.t},
           hum:{c:'#4cc3ff',lab:'RH %',ax:0,get:p=>p.h},
           heat:{c:'#f6d365',lab:'Heater %',ax:1,get:p=>p.heat},
           fan:{c:'#5f8bff',lab:'Fans %',ax:1,get:p=>p.fan},
           bat:{c:'#aab6c8',lab:'Battery %',ax:1,get:p=>p.bat},
           wt:{c:'#e8ecf4',lab:'Weight g',ax:1,get:p=>p.wt}};
const VIS={temp:true,hum:true,heat:false,fan:false,bat:false};
const CIRC=2*Math.PI*52;
function setRing(id,frac){const e=$(id);if(!e)return;frac=frac<0?0:frac>1?1:frac;
  e.style.strokeDashoffset=(CIRC*(1-frac)).toFixed(1)}
function setMark(id,frac,show){const e=$(id);if(!e)return;
  if(!show){e.style.display='none';return}
  e.style.display='';e.setAttribute('transform','rotate('+((frac*360).toFixed(1))+' 60 60)')}
function buildLegend(box,vis,redraw){box.innerHTML='';
  for(const k in SER){const s=SER[k];
    const el=document.createElement('span');el.className='lg'+(vis[k]?' on':'');
    el.style.setProperty('--c',s.c);
    el.innerHTML='<i></i>'+s.lab;
    el.onclick=()=>{vis[k]=!vis[k];el.classList.toggle('on',vis[k]);redraw()};
    box.appendChild(el)}}
function paint(cnv,data,vis,xsec){
  const g=cnv.getContext('2d'),W=cnv.clientWidth,H=250,DPR=devicePixelRatio||1;
  cnv.width=W*DPR;cnv.height=H*DPR;g.scale(DPR,DPR);g.clearRect(0,0,W,H);
  const PL=42,PR=40,PT=12,PB=24,IW=W-PL-PR,IH=H-PT-PB;
  // scales
  let a0min=1e9,a0max=-1e9;const a1min=0,a1max=100;
  const act=Object.keys(SER).filter(k=>vis[k]&&data.some(p=>SER[k].get(p)!=null&&!isNaN(SER[k].get(p))));
  if(!act.length){g.fillStyle='#7c8aa3';g.font='12px system-ui';
    g.fillText('waiting for data\u2026',PL,PT+14);return}
  for(const k of act)for(const p of data){const v=SER[k].get(p);
    if(v!=null&&!isNaN(v)){if(v<a0min)a0min=v;if(v>a0max)a0max=v}}
  if(SER.temp.ax===0){}   // temp+hum share axis 0
  if(a0min>a0max){a0min=0;a0max=1}
  if(a0max-a0min<4){const m=(a0max+a0min)/2;a0min=m-2;a0max=m+2}
  const pad=(a0max-a0min)*.12;a0min-=pad;a0max+=pad;
  const n=data.length;
  const X=i=>n<2?PL+IW/2:PL+i/(n-1)*IW;
  const Y0=v=>PT+IH-(v-a0min)/(a0max-a0min)*IH;
  const Y1=v=>PT+IH-(v-a1min)/(a1max-a1min)*IH;
  // grid + axis labels
  g.font='10.5px system-ui';g.strokeStyle='#16223c';g.lineWidth=1;
  for(let i=0;i<=4;i++){const y=PT+IH*i/4;
    g.beginPath();g.moveTo(PL,y);g.lineTo(W-PR,y);g.stroke();
    g.fillStyle='#7c8aa3';g.textAlign='right';
    g.fillText((a0max-(a0max-a0min)*i/4).toFixed(0),PL-6,y+3.5);
    g.textAlign='left';
    g.fillText((a1max-(a1max-a1min)*i/4).toFixed(0),W-PR+6,y+3.5)}
  // x labels (elapsed)
  g.textAlign='center';g.fillStyle='#7c8aa3';
  const lastSec=xsec?data[n-1].sec:(data[n-1].ts-data[0].ts)/1000;
  for(let i=0;i<=4;i++){const idx=Math.round((n-1)*i/4);
    const sec=xsec?data[idx].sec:(data[idx].ts-data[0].ts)/1000;
    g.fillText(mmss(sec),X(idx),H-8)}
  // target temp guide on axis0 (live chart only)
  if(S&&!xsec&&VIS.temp){const y=Y0(S.set.setTemp);
    if(y>PT&&y<PT+IH){g.setLineDash([5,5]);g.strokeStyle='#d4af37';g.lineWidth=1.2;
      g.beginPath();g.moveTo(PL,y);g.lineTo(W-PR,y);g.stroke();g.setLineDash([])}}
  // series (linears + soft fill for axis0 series)
  for(const k of act){const s=SER[k],Y=s.ax?Y1:Y0;
    g.strokeStyle=s.c;g.lineWidth=2;g.lineJoin='round';g.beginPath();
    let started=false,last=null;
    data.forEach((p,i)=>{const v=s.get(p);
      if(v==null||isNaN(v)){if(started){g.stroke();started=false}return}
      const x=X(i),y=Y(v);
      if(started)g.lineTo(x,y);else{g.moveTo(x,y);started=true}last={x,y,v}});
    g.stroke();
    if(!s.ax&&n>2){                     // gradient fill under temp/RH
      g.lineTo(X(n-1),PT+IH);g.lineTo(PL,PT+IH);g.closePath();
      const gr=g.createLinearGradient(0,PT,0,PT+IH);
      gr.addColorStop(0,s.c+'26');gr.addColorStop(1,s.c+'00');g.fillStyle=gr;g.fill()}
    if(last){g.fillStyle=s.c;g.beginPath();
      g.arc(last.x,last.y,3.2,0,7);g.fill();
      g.fillStyle='#070b14';g.strokeStyle=s.c;g.lineWidth=1.4;
      g.beginPath();g.arc(last.x,last.y,5.4,0,7);g.stroke()}}
}
let hoverI=-1;
function draw(){paint($('chart'),T,VIS,false);if(hoverI>=0)showTip(hoverI)}
function showTip(i){const tip=$('chartTip'),cnv=$('chart');
  if(!T[i]){tip.style.opacity=0;return}
  const W=cnv.clientWidth,PL=42,PR=40,IW=W-PL-PR;
  const x=PL+(T.length<2?IW/2:i/(T.length-1)*IW);
  const sec=(T[i].ts-T[0].ts)/1000;
  let rows='';
  for(const k in SER){if(!VIS[k])continue;const v=SER[k].get(T[i]);
    if(v==null||isNaN(v))continue;
    rows+='<span style="color:'+SER[k].c+'">\u25CF</span> '+SER[k].lab+
          ' <b>'+f2(v)+'</b><br>'}
  tip.innerHTML='<b>'+mmss(sec)+'</b><br>'+rows;
  tip.style.opacity=1;
  tip.style.left=Math.min(W-150,Math.max(4,x+14))+'px';
  tip.style.top=(cnv.offsetTop+18)+'px';
  const g=cnv.getContext('2d');g.save();
  g.strokeStyle='#33415e';g.setLineDash([4,4]);
  g.beginPath();g.moveTo(x,12);g.lineTo(x,226);g.stroke();g.restore()}
function chartHover(ev){const cnv=$('chart'),W=cnv.clientWidth,PL=42,PR=40;
  if(T.length<2)return;
  const r=cnv.getBoundingClientRect();const x=ev.clientX-r.left;
  const i=Math.round((x-PL)/(W-PL-PR)*(T.length-1));
  hoverI=(i>=0&&i<T.length)?i:-1;
  if(hoverI>=0){draw();showTip(hoverI)}else{$('chartTip').style.opacity=0;draw()}}
function chartLeave(){hoverI=-1;$('chartTip').style.opacity=0;draw()}

// ---------- past-cycle graph viewer ---------------------------------------
let VT=null;const VVIS={temp:true,hum:true,heat:true,fan:true,bat:false};
function viewCycle(file,started){
  espFetch('/api/cycle?file='+encodeURIComponent(file))
   .then(r=>{if(!r.ok)throw 0;return r.text()})
   .then(csv=>{const pts=[];
     csv.split(/\r?\n/).forEach(l=>{if(!l||l[0]==='#')return;
       const c=l.split(',');if(c.length<8)return;
       pts.push({sec:+c[0],t:+c[1],h:+c[2],heat:+c[4],fan:+c[5],bat:+c[7],
                 wt:(c[8]!==''&&c[8]!=null)?+c[8]:null})});
     if(pts.length<2){toast('No data points',1);return}
     VT=pts;$('cycView').style.display='block';
     $('cycTitle').textContent='\u{1F4E6} '+started;
     buildLegend($('cycLegend'),VVIS,()=>paint($('cycChart'),VT,VVIS,true));
     paint($('cycChart'),VT,VVIS,true);
     $('cycView').scrollIntoView({behavior:'smooth'})})
   .catch(()=>toast('Could not load cycle',1))}
function closeCycle(){VT=null;$('cycView').style.display='none'}

// ---------- outdoor weather (phone relays internet -> ESP) ----------------
// typical-season dummy forecast: derived from the REAL date & time, so it
// tracks the seasons and the hour of the day without any internet at all
function dummyWx(){
  const now=Date.now(),d=Math.floor(now/864e5),days=[];
  for(let i=0;i<5;i++){
    const hi=32.5+6*Math.sin((d-30+i)/365*2*Math.PI);
    const rh=72-9*Math.sin((d-30+i)/365*2*Math.PI);
    days.push({lab:new Date(now+i*864e5).toLocaleDateString(undefined,{weekday:'short'}),
      icon:rh>78?'\u{1F327}':(rh>64?'\u26C5':'\u2600'),
      hi:Math.round(hi),lo:Math.round(hi-7.5)});
  }
  const dt=new Date(now),h=dt.getHours()+dt.getMinutes()/60;
  const f=Math.max(0,Math.min(1,0.5-0.5*Math.cos((h-9)/12*Math.PI)));
  const tNow=days[0].lo+(days[0].hi-days[0].lo)*(0.35+0.5*f);
  return {days,tNow,rhNow:Math.round(Math.max(35,88-days[0].hi-f*14))};
}
const WMO={0:['\u2600\uFE0F','clear'],1:['\uD83C\uDF24\uFE0F','mainly clear'],2:['\u26C5','partly cloudy'],
3:['\u2601\uFE0F','overcast'],45:['\uD83C\uDF2B\uFE0F','fog'],48:['\uD83C\uDF2B\uFE0F','rime fog'],
51:['\uD83C\uDF26\uFE0F','light drizzle'],53:['\uD83C\uDF26\uFE0F','drizzle'],55:['\uD83C\uDF26\uFE0F','heavy drizzle'],
61:['\uD83C\uDF27\uFE0F','light rain'],63:['\uD83C\uDF27\uFE0F','rain'],65:['\uD83C\uDF27\uFE0F','heavy rain'],
71:['\u2744\uFE0F','light snow'],73:['\u2744\uFE0F','snow'],75:['\u2744\uFE0F','heavy snow'],
80:['\uD83C\uDF26\uFE0F','showers'],81:['\uD83C\uDF27\uFE0F','showers'],82:['\u2614\uFE0F','heavy showers'],
95:['\u26C8\uFE0F','thunderstorm'],96:['\u26C8\uFE0F','storm + hail'],99:['\u26C8\uFE0F','severe storm']};
function wxName(c){return WMO[c]?WMO[c][1]:'--'}
async function pushWx(t,h,r,w,c,manual,loc){
  try{await espFetch('/api/weather',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({t:t,h:h,r:r||0,w:w||0,c:c||100,m:!!manual,
      loc:loc||'',ep:Math.floor(Date.now()/1000)})})}catch(e){}}
async function fetchWx(){                 // live: phone's mobile data -> ESP
  let loc=null;try{loc=JSON.parse(localStorage.getItem('dryerLoc')||'null')}catch(e){}
  if(!loc){$('wxWhen').textContent='set your town below for live weather';return}
  try{
    const u='https://api.open-meteo.com/v1/forecast?latitude='+loc.lat+'&longitude='+loc.lon+
      '&current=temperature_2m,relative_humidity_2m,precipitation_probability,weather_code,wind_speed_10m';
    const j=await(await fetch(u)).json();const c=j.current||{};
    await pushWx(c.temperature_2m,c.relative_humidity_2m,c.precipitation_probability,
                 c.wind_speed_10m,c.weather_code,false,loc.name);
    $('wxWhen').textContent='live from your phone \u2713';
  }catch(e){$('wxWhen').textContent='no internet on phone \u2014 use manual entry'}}
async function saveCity(){
  const v=$('wxCity').value.trim();if(!v){toast('Type a town name',1);return}
  try{
    const g=await(await fetch('https://geocoding-api.open-meteo.com/v1/search?count=1&name='
      +encodeURIComponent(v))).json();
    if(!g.results||!g.results.length){toast('Town not found',1);return}
    const r=g.results[0];
    localStorage.setItem('dryerLoc',JSON.stringify({lat:r.latitude,lon:r.longitude,name:r.name}));
    toast('Location: '+r.name);fetchWx();
  }catch(e){toast('No internet on phone',1)}}
function sendManualWx(){
  const t=parseFloat($('wxMt').value),h=parseFloat($('wxMh').value);
  if(isNaN(t)||isNaN(h)){toast('Enter temp and RH',1);return}
  pushWx(t,h,0,0,100,true,'manual').then(()=>toast('Sent'));}

// ---------- settings form / defaults table --------------------------------
function fillForm(c){
  $('f_setTemp').value=c.setTemp;$('f_tempHyst').value=c.tempHyst;$('f_maxTemp').value=c.maxTemp;
  $('f_humHigh').value=c.humHigh;$('f_humLow').value=c.humLow;$('f_humTarget').value=c.humTarget;
  $('f_requireHum').checked=!!c.requireHum;$('f_smartVent').checked=!!c.smartVent;
  $('f_boostHeat').checked=!!c.boostHeat;
  $('f_hrs').value=Math.floor(c.dryMinutes/60);$('f_min').value=c.dryMinutes%60;
  $('f_fanMin').value=c.fanMin;$('f_heaterMax').value=c.heaterMax;
  $('f_fanOut').value=c.fanOut;$('f_fanSlope').value=c.fanSlope;
  $('f_targetG').value=c.targetG||0;$('f_fanTrigRH').value=c.fanTrigRH||60;
  $('f_stickCount').value=c.stickCount||0;$('f_stickWetG').value=c.stickWetG||2.5;
  $('f_pasteWater').value=c.pasteWaterPct!=null?c.pasteWaterPct:35;
  $('f_targetMoist').value=c.targetMoistPct!=null?c.targetMoistPct:10;stickCalc();
  $('f_fanTrigMin').value=c.fanTrigMin||1;$('f_fanBurstS').value=c.fanBurstS||60;
  document.querySelectorAll('[data-mdbtn]').forEach(function(b){
    b.classList.toggle('on', +b.dataset.mdbtn===(c.mode||0));});
  $('f_battType').value=c.battType;$('f_cutoffPct').value=c.cutoffPct;
  $('f_requireWeight').checked=!!c.requireWeight;
  $('f_weightRateG').value=c.weightRateG;$('f_weightMinY').value=c.weightMinY;
  $('f_cooldownSec').value=c.cooldownSec;$('f_kp').value=c.kp;$('f_ki').value=c.ki;$('f_kd').value=c.kd;
  $('f_tz').value=c.tzMinutes;
}
function num(id,min,max){let v=parseFloat($(id).value);if(isNaN(v))v=min;
  return Math.min(max,Math.max(min,v))}
async function saveCfg(start){
  const o={setTemp:num('f_setTemp',25,90),tempHyst:num('f_tempHyst',0.2,5),
    maxTemp:num('f_maxTemp',35,110),humHigh:num('f_humHigh',20,95),humLow:num('f_humLow',10,80),
    humTarget:num('f_humTarget',5,70),requireHum:$('f_requireHum').checked,
    smartVent:$('f_smartVent').checked,boostHeat:$('f_boostHeat').checked,
    dryMinutes:Math.max(1,Math.round(num('f_hrs',0,24)*60+num('f_min',0,59))),
    fanMin:num('f_fanMin',0,60),heaterMax:num('f_heaterMax',10,100),
    fanOut:num('f_fanOut',10,100),fanSlope:num('f_fanSlope',1,12),
    targetG:num('f_targetG',0,9000),
    stickCount:num('f_stickCount',0,3000),stickWetG:num('f_stickWetG',0.5,20),
    pasteWaterPct:num('f_pasteWater',5,60),targetMoistPct:num('f_targetMoist',3,20),
    fanTrigRH:num('f_fanTrigRH',30,90),
    fanTrigMin:num('f_fanTrigMin',1,10),fanBurstS:num('f_fanBurstS',10,300),
    battType:num('f_battType',0,3),cutoffPct:num('f_cutoffPct',0,40),
    requireWeight:$('f_requireWeight').checked,weightRateG:num('f_weightRateG',0.5,50),
    weightMinY:num('f_weightMinY',2,120),
    cooldownSec:num('f_cooldownSec',10,600),kp:num('f_kp',0,100),ki:num('f_ki',0,10),kd:num('f_kd',0,100),
    tzMinutes:num('f_tz',-720,840)};
  if(o.maxTemp<o.setTemp+5){toast('Safety cutoff must be at least 5 \u00B0C above target',1);return}
  if(o.humLow>=o.humHigh-1){toast('Humidity band is upside down',1);return}
  try{const r=await espFetch('/api/settings',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify(o)});
    if(!r.ok){toast('Rejected: '+await r.text(),1);return}
    if(start){await espFetch('/api/start',{method:'POST'});showTab('Dash')}
    toast('Settings saved')}
  catch(e){toast('Offline',1)}
}
function fillDef(){
  const rows=[['Target temperature',f2(DEF.setTemp)+' \u00B0C','PID holds the chamber here'],
  ['Control band \u00B1',f2(DEF.tempHyst)+' \u00B0C','temperature smoothing window'],
  ['Safety cutoff',f2(DEF.maxTemp,0)+' \u00B0C','heater + fans cut above this'],
  ['Full-power heat-up',DEF.boostHeat?'ON':'OFF','BTS at max until target, then PID'],
  ['Fans ramp above',f2(DEF.humHigh,0)+' %RH','moist air is vented out'],
  ['Fans stop below',f2(DEF.humLow,0)+' %RH','heat retained, no over-drying'],
  ['Target RH',f2(DEF.humTarget,0)+' %RH',DEF.requireHum?'must be reached to finish':'informational'],
  ['Drying time',DEF.dryMinutes+' min','set manually in the form above'],
  ['Minimum fan speed',DEF.fanMin+' %','circulation inside the RH band'],
  ['Outlet fan burst','RH \u2265 '+DEF.fanTrigRH+'% for '+DEF.fanTrigMin+' min \u2192 100% for '+DEF.fanBurstS+' s','single exhaust fan'],
  ['Ramp per RH point',DEF.fanSlope+' %/RH','how fast fans spin up above humHigh'],
  ['Heater power cap',DEF.heaterMax+' %','soft limit on coil current'],
  ['Purge before cut',DEF.cooldownSec+' s','fans flush hot air, then all off'],
  ['Safe shutdown below',DEF.cutoffPct+' %','battery % that stops everything'],
  ['Heater PID (Kp/Ki/Kd)',DEF.kp+' / '+DEF.ki+' / '+DEF.kd,'duty control of coil current'],
  ['Dry to weight',DEF.requireWeight?'ON':'off','cycle ends when weight settles'],
  ['Settled below',DEF.weightRateG+' g/min','rate under this = dry enough'],
  ['Stable for',DEF.weightMinY+' min','how long the rate must stay low']];
  $('defTable').innerHTML='<tr><th>Parameter</th><th>Value</th></tr>'+
    rows.map(r=>'<tr><td>'+r[0]+'<div class="note">'+r[2]+'</div></td><td>'+r[1]+'</td></tr>').join('');
}

// ---------- main render ----------------------------------------------------
function render(){
  $('eeN').textContent = (S.eelog && S.eelog.ok) ? (S.eelog.used + '/' + S.eelog.slots) : 'not fitted';
  $('eeN2').textContent = (S.eelog && S.eelog.ok) ? S.eelog.used + ' cycles' : '--';
  if(!S)return;
  const st=S.state;
  const b=$('stBadge');b.innerHTML='<span class="dot"></span>'+st;
  b.className='pill '+({IDLE:'p-idle',DRYING:'p-run',PURGING:'p-purge',
    DONE:'p-done',FAULT:'p-fault'})[st];
  const fb=$('faultBox');fb.style.display=st==='FAULT'?'block':'none';
  var fr=S.faultRec||{};
  fb.innerHTML='\u26A0 <b>E'+String(fr.code||0).padStart(2,'0')+' '+(fr.name||'FAULT')+
    '</b> \u2014 '+S.fault+' \u00B7 power is cut. Fix the issue, then press Power On.'+
    ((fr.code&&fr.limit)?' \u00B7 value '+(fr.val||0).toFixed(1)+' / limit '+
      (fr.limit||0).toFixed(1):'')+' \u00B7 reset: MANUAL';
  var dc=$('doneCard');
  if(st==='DONE'){dc.style.display='block';
    var mm=Math.floor((S.cycleSecs||0)/60);
    dc.innerHTML='\u2705 <b>DRYING COMPLETE</b> \u00B7 initial '
     +Math.round(S.wtStart||0)+' g \u00B7 final '+Math.round(S.finalG||0)
     +' g \u00B7 target '+Math.round(S.set?(+S.set.targetG||0):0)+' g \u00B7 <b>moisture removed '
     +Math.round(S.moistureG||0)+' g</b> \u00B7 duration '+mm+' min'
     +' \u2014 batch ready for packaging';}
  else dc.style.display='none';
  var wb=$('warnBox'),wr=S.warnRec||{};
  if(wr&&wr.active&&wr.code){wb.style.display='block';
    wb.innerHTML='\u26A0 <b>E'+String(wr.code).padStart(2,'0')+' '+wr.name+
    '</b> \u2014 warning: the cycle continues, but check the machine';}
  else wb.style.display='none';
  // gauges
  $('tAvg').textContent=f2(S.tAvg);$('hAvg').textContent=f2(S.hAvg);
  setRing('g_t',S.tAvg!=null&&!isNaN(S.tAvg)?(S.tAvg-15)/60:0);
  setRing('g_h',S.hAvg!=null&&!isNaN(S.hAvg)?S.hAvg/100:0);
  setRing('g_b',S.bat.valid?S.bat.pct/100:0);
  const gb=$('g_b');if(S.bat.valid)
    gb.setAttribute('stroke',S.bat.pct>50?'#5fa8ff':S.bat.pct>20?'#d4af37':'#f87171');
  setMark('g_tmark',S.set?((S.set.setTemp-15)/60):0,S.set&&S.set.setTemp>15&&S.set.setTemp<75);
  setMark('g_hlo',S.set?S.set.humLow/100:0,true);setMark('g_hhi',S.set?S.set.humHigh/100:0,true);
  $('t1').textContent=f2(S.s1.t);$('t2').textContent=f2(S.s2.t);
  if(S.fw)$('fw').textContent=S.fw;
  var dr=S.door;
  if(dr){
    var dp=$('doorPill');
    if(dr.phase==='CALIBRATE'){dp.style.display='inline-flex';dp.className='pill p-fault';
      dp.textContent='\u{1F512} CALIBRATE THE SCALE - START LOCKED'}
    else if(dr.phase==='RUNNING'){dp.style.display='inline-flex';dp.className='pill p-run';
      dp.textContent='\u{1F513} OPEN DOOR = FAULT'}
    else if(dr.phase==='READY'){dp.style.display='inline-flex';dp.className='pill p-done';
      dp.textContent='\u2696 READY '+Math.round(dr.batch||0)+' g'}
    else{dp.style.display=dr.locked?'inline-flex':'none';dp.className='pill p-purge';
      dp.textContent='\u{1F513} LOAD THE TRAYS'}
  }
  $('kpLast').textContent=(S.kp&&S.kp.last)?S.kp.last:'\u2013';
  var dh=S.dht;
  if(dh&&dh.ok){$('wxOut').innerHTML='\u2600 <b>OUT (measured by the dryer)</b>: '
    +f2(dh.t,1)+'\u00B0C \u00B7 '+f2(dh.h,0)+'% RH \u2014 smart venting uses THIS';}
  else{$('wxOut').innerHTML='\u2600 no DHT22 fitted \u2014 outdoor values come from your phone';}
  var sc=S.scale||{ok:false,g:0,rate:0};
  if(sc.ok){
    $('wt').textContent=Math.round(sc.g);
    $('wtRate').textContent=(sc.rate!=null?sc.rate.toFixed(1):'--');
    $('wtMode').textContent=S.set&&S.set.requireWeight?'ON':'off';
    $('wtBadge').style.display=(S.set&&S.set.requireWeight&&Math.abs(sc.rate||1)<S.set.weightRateG)?'inline-block':'none';
    var tg=S.set?(+S.set.targetG||0):0;
    $('wtTgt').textContent=tg>0?('\u2192 '+Math.round(tg)+' g (diff '+Math.round(sc.g-tg)+' g)'):'no target set';
  }else{$('wt').textContent='--';$('wtRate').textContent='--';$('wtMode').textContent=S.set&&S.set.requireWeight?'ON (no scale!)':'off';$('wtBadge').style.display='none'}
  $('h1').textContent=f2(S.s1.h);$('h2').textContent=f2(S.s2.h);$('hMax').textContent=f2(S.hMax);
  $('d1').className='dot'+(S.s1.ok?'':' off');$('d2').className='dot'+(S.s2.ok?'':' off');
  $('heat').textContent=S.heat;$('fan').textContent=S.fan;
  $('boostBadge').style.display=S.boost?'inline-block':'none';
  // manual heat knob: MANUAL (countdown) while the override window is open,
  // otherwise the knob just mirrors the automatic duty
  var m=S.man||{pct:0,left:0};
  if(m.left>0){
    $('manBadge').textContent='MANUAL '+m.left+'s';
    $('manBadge').className='badge b-purge';
    $('heatKnob').value=m.pct;
    $('knobPct').textContent=m.pct+'%';
  }else{
    $('manBadge').textContent='AUTO';
    $('manBadge').className='badge b-run';
    if(document.activeElement!==$('heatKnob')&&!knobTmr){
      $('heatKnob').value=S.heat||0;
      $('knobPct').textContent=(S.heat||0)+'%';
    }
  }
  var sup=S.supply;
  if(sup){
    var sp=$('supPill');
    sp.textContent=sup.solar?'\u2600 SOLAR MODE':'\u26A1 BYPASS MODE';
    sp.className='pill '+(sup.live?'p-run':'p-fault');
    document.querySelectorAll('[data-mdbtn]').forEach(function(b){
      b.classList.toggle('on', +b.dataset.mdbtn===(sup.mode||0));});
  }
  $('fanOut').textContent=(S.fanOut!=null?S.fanOut:'--');
  $('heatBar').style.width=S.heat+'%';$('fanBar').style.width=S.fan+'%';
  $('setT').textContent=f2(S.set.setTemp);$('setH').textContent=f2(S.set.humLow,0)+'\u2013'+f2(S.set.humHigh,0);
  $('setD').textContent=S.set.dryMinutes;$('setPurge').textContent=S.set.cooldownSec;
  $('setCut').textContent=S.set.cutoffPct;
  $('setCap').textContent=S.set.heaterMax;
  $('bat').textContent=S.bat.valid?S.bat.pct:'--';$('bv').textContent=f2(S.bat.v,2);
  $('batType').textContent=S.bat.name;
  $('rem').textContent=st==='DRYING'?mmss(S.remaining):st==='PURGING'?'purging':st==='DONE'?'power cut':'--';
  $('el').textContent=mmss(S.elapsed);
  $('btnStart').disabled=st==='DRYING'||st==='PURGING';
  $('btnStop').disabled=st!=='DRYING';
  $('btnAdd').disabled=st!=='DRYING';
  $('btnPower').disabled=st!=='DONE'&&st!=='FAULT';
  $('logN').textContent=S.logCount;$('heap').textContent=S.heap;
  // live clock from the ESP (browser syncs it automatically on connect)
  if(S.clockSet){baseSec=S.now+(S.tz||0)*60;baseMs=Date.now();tickClock()}
  else{$('clock').textContent='\u23F1 clock not set \u2014 syncing from your phone\u2026';baseSec=0}
  if(!S.clockSet&&Date.now()-lastSync>60000)syncClock();
  else if(Date.now()-lastSync>1800000)syncClock();
  if(lastState==='DRYING'&&S.state==='DONE')loadCycles();   // fresh entry
  lastState=S.state;
  // outdoor weather card (source: live forecast > manual > stale)
  const w=S.wx;
  if(w&&w.outT!=null){
    $('wxBadge').textContent=(w.src||'none').toUpperCase();
    $('wxBadge').className='badge '+((w.src==='live')?'b-run':
      (w.src==='manual'?'b-purge':'b-fault'));
    $('wxT').textContent=f2(w.outT);$('wxH').textContent=f2(w.outH,0);
    $('wxR').textContent=w.ok?f2(w.r,0):'--';$('wxW').textContent=w.ok?f2(w.w,0):'--';
    $('wxIcon').textContent=(w.ok&&WMO[w.code])?WMO[w.code][0]:'';
    let when='';
    if(w.ok) when=(w.loc?w.loc+' \u00B7 ':'')+wxName(w.code)+' \u00B7 '+w.age+' min ago';
    else when='no outdoor data';
    if(S.set.smartVent&&w.outH!=null&&S.hMax!=null&&!isNaN(S.hMax)&&w.outH>=S.hMax)
      when+=' \u00B7 venting paused (outside wetter)';
    $('wxWhen').textContent=when;
  }else{const dwx=dummyWx();
    $('wxBadge').textContent='TYP.';$('wxBadge').className='badge b-idle';
    $('wxT').textContent=f2(dwx.tNow);$('wxH').textContent=f2(dwx.rhNow,0);
    $('wxR').textContent='--';$('wxW').textContent='--';$('wxIcon').textContent='';
    $('wxWhen').textContent='typical estimate from date & time (no live data)';
  }
  try{const dw=dummyWx();$('wxDays').innerHTML=dw.days.map(x=>
    '<span style="display:inline-block;text-align:center;padding:6px 9px;margin:2px;'+
    'border:1px solid var(--line2);border-radius:10px;background:var(--card2);font-size:11.5px">'+
    '<b style="color:var(--dim)">'+x.lab+'</b><br>'+x.icon+' '+x.hi+'\u00B0/'+x.lo+'\u00B0</span>').join('')}catch(e){}
  if(st==='DRYING'&&S.tAvg!=null&&!isNaN(S.tAvg))
    T.push({t:S.tAvg,h:S.hAvg,heat:S.heat,fan:S.fan,bat:S.bat.pct,ts:Date.now()});
  if(T.length>600)T.shift();
  try{draw()}catch(e){}          // never let a canvas glitch kill the poll loop
}
async function poll(){try{const r=await espFetch('/api/data');S=await r.json();
  if(DEF===null){DEF=S.defs;if($('secSet').classList.contains('on'))fillForm(S.set)}render();vpRenderIfOpen()}
  catch(e){$('stBadge').innerHTML='<span class="dot"></span>OFFLINE';
    $('stBadge').className='pill p-fault'}}

// ---------- virtual keypad (v2.0.19: touch-display entry) ----------------
function vesc(s){return String(s==null?'':s).replace(/&/g,'&amp;').replace(/</g,'&lt;')}
function vpOpen(){$('vpDrawer').classList.add('on');vpRender()}
function vpClose(){$('vpDrawer').classList.remove('on')}
function vpKey(k){espFetch('/api/key?k='+encodeURIComponent(k),{method:'POST'})
  .then(function(){poll()}).catch(function(){toast('Offline',1)})}
function vpRender(){
  var t=$('vpText'),m=S&&S.menu;
  if(!m){t.innerHTML=
    '<div class="vpRow vpSel"><span>'+vesc(S?S.state:'--')+'</span></div>'+
    '<div class="vpRow"><span>Press <b>B</b> (MENU) to open the on-device menu</span></div>'+
    '<div class="vpRow"><span><b>D</b> run/stop &middot; <b>C</b> mode &middot; <b>*</b> home &middot; <b>#</b> back &middot; digits type values</span></div>';}
  else if(m.edit){t.innerHTML=
    '<div class="vpRow"><span>ENTER VALUE</span></div>'+
    '<div class="vpRow vpSel" style="font-size:22px"><span>'+(m.buf||'--')+'</span>'+
    '<span style="font-size:12px;color:var(--dim)">'+vesc(m.hint)+'</span></div>'+
    '<div class="vpRow"><span>type digits &middot; <b>A</b> save &middot; <b>#</b> cancel</span></div>';}
  else{var h='';(m.items||[]).forEach(function(it,i){h+=
    '<div class="vpRow'+(i===m.cur?' vpSel':'')+'"><span>'+(i===m.cur?'▶ ':'')+
    vesc(it.n)+'</span><b>'+vesc(it.v)+'</b></div>'});
    t.innerHTML=h||'<div class="vpRow">menu empty</div>';}}
function vpRenderIfOpen(){if($('vpDrawer').classList.contains('on'))vpRender()}

// ---------- past cycles (saved in ESP flash) ------------------------------
async function loadCycles(){try{
  const r=await espFetch('/api/cycles'),list=await r.json();
  const t=$('cycTable');
  while(t.rows.length>1)t.deleteRow(-1);
  for(const c of list){
    const row=t.insertRow(-1);
    const dur=(+c.elapsedMin||0);
    const left=(c.reason==='completed')?'\u2714 done':
      ((+c.remainMin>0)?f2(c.remainMin,0)+' min left':'\u2714 done');
    row.innerHTML='<td>'+c.started+'<div class="note">target '+f2(c.setTemp,1)+
      ' \u00B0C \u00B7 set '+c.dryMin+' min'+(c.note?' \u00B7 '+c.note:'')+'</div></td>'+
      '<td>'+(dur>=60?f2(dur/60,1)+' h':f2(dur,0)+' min')+'</td>'+
      '<td><span class="badge '+(c.reason==='completed'?'b-run':c.reason==='stopped'?'b-purge':'b-fault')+
      '">'+c.reason+'</span></td><td>'+left+'</td>'+
      '<td><a class="dl" href="/api/cycle?file='+encodeURIComponent(c.file)+'" download>\u2B07 CSV</a></td>'+
      '<td><button class="vbtn" onclick="viewCycle(\''+c.file+'\',\''+c.started+'\')">\u{1F4C8} view</button></td>';
  }
 }catch(e){}}
async function clearCycles(){if(!confirm('Delete all saved cycles?'))return;
  try{await espFetch('/api/clearcycles',{method:'POST'});loadCycles();toast('History cleared')}
  catch(e){toast('Offline',1)}}

// ---------- boot -----------------------------------------------------------
(async()=>{
  buildLegend($('legend'),VIS,draw);
  $('chart').addEventListener('mousemove',chartHover);
  $('chart').addEventListener('mouseleave',chartLeave);
  try{const r=await espFetch('/api/history'),j=await r.json();
    const now=Date.now();
    T=j.t.map((t,i)=>({t:j.temp[i],h:j.hum[i],heat:null,fan:null,bat:null,
      ts:now-(j.t.length-1-i)*2000})).slice(-600)}catch(e){}
  try{const l=JSON.parse(localStorage.getItem('dryerLoc')||'null');
    if(l&&l.name)$('wxCity').value=l.name}catch(e){}
  loadCycles();fetchWx();setInterval(fetchWx,20*60*1000);
  initBg();setInterval(tickClock,1000);poll();setInterval(poll,2000)})();
addEventListener('resize',draw);
</script></body></html>)HTML";

// The OPTIONAL online-UI bridge page (served at /online). It frames the
// GitHub Pages dashboard and relays every /api call for it, because a
// browser will not let an https:// page call http://192.168.4.1 directly
// (mixed content). If the frame never says hello (no internet), it falls
// back to the built-in page at /.
static const char ONLINE_LOADER_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Dehumidifier - online interface</title>
<style>body{margin:0;background:#101512;color:#e8f0ea;font:15px system-ui;
height:100vh;display:flex;align-items:center;justify-content:center;text-align:center}
#m{opacity:.75;padding:20px}a{color:#38bdf8}iframe{border:0;width:100vw;height:100vh}</style>
</head><body>
<div id="m">loading the online interface&hellip;<br><br>
If nothing appears your phone has no internet -<br>
<a href="/">use the built-in interface</a></div>
<iframe id="f" src="__ONLINE_URL__"></iframe>
<script>
'use strict';
var seen=false;
addEventListener('message',function(e){
  if(e.source!==document.getElementById('f').contentWindow)return;
  seen=true;                                   // any msg = frame is alive
  var d=e.data; if(!d||!d.bridgeId)return;
  fetch(d.url,{method:d.method||'GET',body:d.body||null,
    headers:d.body?{'Content-Type':'application/json'}:{}})
  .then(function(r){return r.text().then(function(t){return{ok:r.ok,status:r.status,body:t}})})
  .catch(function(){return{ok:false,status:0,body:''}})
  .then(function(m){e.source.postMessage({bridgeId:d.bridgeId,ok:m.ok,status:m.status,body:m.body},'*')});
});
setTimeout(function(){if(!seen)location.replace('/')},7000);  // no internet -> built-in
</script></body></html>)HTML";


// ----------------------------------------------------------------------
//  GET /display - the KIOSK DISPLAY PAGE (v2.0.7). The hardware TFT was
//  removed from this build; a phone/tablet mounted on the pillar opens
//  this page and BECOMES the display: read-only, huge type, 1 Hz refresh,
//  wake-lock kept, zero controls. Same /api/data as the dashboard.
//  (The dashboard at / stays the full-control page.)
static const char DISPLAY_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SMART DEHUMIDIFIER - display</title>
<style>
:root{--navy:#0b1026;--navy2:#141b3d;--card:#10173a;--gold:#f4c25e;
--blue:#4e9ff4;--green:#3ddc84;--red:#ff5f6b;--dim:#8b93b8;--white:#eef2ff}
*{box-sizing:border-box}html,body{height:100%}
body{margin:0;background:var(--navy);color:var(--white);
font-family:'Segoe UI',system-ui,Arial,sans-serif;display:flex;
flex-direction:column;padding:12px;overflow:hidden}
.bar{display:flex;justify-content:space-between;align-items:center;
background:var(--navy2);border-radius:12px;padding:10px 18px;font-size:2.6vmin}
.bar b{color:var(--gold)}#sup{margin-left:12px}#sup.bad{color:var(--red)}
#sup.ok{color:var(--green)}#pmd{color:var(--dim)}
#stateRow{display:flex;align-items:baseline;gap:3vmin;padding:2vmin 2vmin 1vmin}
#st{font-size:9vmin;font-weight:800;letter-spacing:1px}
#st.DRYING{color:var(--green)}#st.FAULT{color:var(--red)}
#st.DONE,#st.PURGING{color:var(--blue)}#st.IDLE{color:var(--dim)}
#times{font-size:3.4vmin;color:var(--white)}
#grid{flex:1;display:grid;grid-template-columns:1fr 1fr 1fr 1fr;
grid-template-rows:1fr 1fr;gap:12px;min-height:0}
.tile{background:var(--card);border-radius:14px;padding:2vmin 2.4vmin;
display:flex;flex-direction:column;justify-content:center;min-height:0}
.tile .lbl{font-size:2.2vmin;color:var(--dim);letter-spacing:1px}
.tile .val{font-size:6.4vmin;font-weight:700;color:var(--gold);line-height:1.1}
.tile .sub{font-size:2.4vmin;color:var(--dim)}
.hbar{height:2.2vmin;border-radius:6px;background:#0a0f24;margin-top:1.2vmin;
overflow:hidden}.hbar i{display:block;height:100%;border-radius:6px}
#foot{font-size:2.6vmin;color:var(--dim);padding:1.4vmin 2vmin;text-align:center}
#fault{color:var(--red);font-weight:700;font-size:3vmin}
#clk{cursor:pointer;white-space:nowrap;margin-right:2vmin}
#clkPane{display:none;position:fixed;left:12px;right:12px;bottom:12px;
background:var(--navy2);border-radius:12px;padding:14px;z-index:9;font-size:3vmin}
#clkPane input{font-size:3vmin;padding:6px;border-radius:8px;border:1px solid #33407a;background:#0b1026;color:var(--white)}
#clkPane button{font-size:3vmin;padding:8px 14px;margin:8px 6px 0 0;border:0;
border-radius:8px;background:var(--blue);color:#fff;cursor:pointer}
/* v2.0.19: virtual keypad - landscape text page + 4x4 pad */
#vpWrap{display:none;flex:1;gap:12px;min-height:0}
body.vp #stateRow,body.vp #grid,body.vp #foot{display:none}
body.vp #vpWrap{display:flex}
#vpText{flex:1;background:var(--card);border-radius:14px;padding:2vmin 3vmin;
overflow:auto;font-size:3.2vmin;line-height:1.9;min-height:0}
#vpText .sel{color:var(--gold);font-weight:800}
#vpPad{width:min(46vmin,340px);display:grid;grid-template-columns:repeat(4,1fr);
grid-auto-rows:1fr;gap:1.2vmin}
#vpPad button{font-size:4.4vmin;font-weight:700;border-radius:12px;border:0;
background:var(--navy2);color:var(--white);cursor:pointer;font-family:inherit}
#vpPad button:active{transform:scale(.93);background:var(--card)}
#vpPad button small{display:block;font-size:1.9vmin;font-weight:600;color:var(--dim)}
@media(max-width:720px){#vpWrap{flex-direction:column}#vpPad{width:100%}}
</style></head><body>
<div class="bar"><span><b>SMART DEHUMIDIFIER</b> <span id="fw"></span></span>
<span><a id="vpBtn" onclick="vpToggle()" title="Virtual keypad - landscape text + 4x4 pad"
  style="color:var(--dim);text-decoration:none;margin-right:2vmin;cursor:pointer">&#9000; keypad</a><a href="/eelog.csv" download title="Download all cycle data (CSV)"
  style="color:var(--dim);text-decoration:none;margin-right:2vmin">&#11015; data</a><span id="clk" onclick="clkPane()">&#9201; --</span><span id="sup" class="ok">&#9728; SOLAR MODE</span>
<span id="pmd">AGARBATTI</span></span></div>
<div id="stateRow"><span id="st">--</span><span id="times">--</span></div>
<div id="grid">
 <div class="tile"><div class="lbl">T1 TOP</div><div class="val" id="t1">--</div>
   <div class="sub">T2 <span id="t2">--</span> &middot; AVG <span id="ta">--</span></div></div>
 <div class="tile"><div class="lbl">HUMIDITY</div><div class="val" id="rh">--</div>
   <div class="sub">OUT <span id="out">--</span> (<span id="outsrc">--</span>)</div></div>
 <div class="tile"><div class="lbl">BATTERY</div><div class="val" id="bat">--</div>
   <div class="sub"><span id="batv">--</span> V &middot; <span id="batn">--</span></div>
   <div class="hbar"><i id="batBar" style="background:var(--green);width:0%"></i></div></div>
 <div class="tile"><div class="lbl">WEIGHT</div><div class="val" id="wt">--</div>
   <div class="sub" id="wt2">no target</div></div>
 <div class="tile"><div class="lbl">HEAT</div><div class="val" id="heat">--</div>
   <div class="hbar"><i id="heatBar" style="background:var(--gold);width:0%"></i></div></div>
 <div class="tile"><div class="lbl">FAN</div><div class="val" id="fan">--</div>
   <div class="hbar"><i id="fanBar" style="background:var(--blue);width:0%"></i></div></div>
 <div class="tile"><div class="lbl">DOOR</div><div class="val" id="door">--</div>
   <div class="sub" id="door2">--</div></div>
 <div class="tile"><div class="lbl">TARGET</div><div class="val" id="set">--</div>
   <div class="sub">rate <span id="rate">--</span> g/min</div></div>
</div>
<div id="vpWrap"><div id="vpText"></div><div id="vpPad">
 <button onclick="vpKey('1')">1</button><button onclick="vpKey('2')">2<small>&#9650;</small></button><button onclick="vpKey('3')">3</button><button onclick="vpKey('A')">A<small>OK</small></button>
 <button onclick="vpKey('4')">4<small>&#9664;</small></button><button onclick="vpKey('5')">5</button><button onclick="vpKey('6')">6<small>&#9654;</small></button><button onclick="vpKey('B')">B<small>MENU</small></button>
 <button onclick="vpKey('7')">7</button><button onclick="vpKey('8')">8<small>&#9660;</small></button><button onclick="vpKey('9')">9</button><button onclick="vpKey('C')">C<small>MODE</small></button>
 <button onclick="vpKey('*')">*</button><button onclick="vpKey('0')">0</button><button onclick="vpKey('#')">#<small>BACK</small></button><button onclick="vpKey('D')">D<small>RUN</small></button>
</div></div>
<div id="foot"><span id="fault"></span><span id="hint">AgarbattiDryer &middot; full control page: 192.168.4.1</span></div>
<div id="doneCard" style="display:none;position:fixed;left:12px;right:12px;bottom:12px;
background:var(--card);border:2px solid var(--green);border-radius:14px;padding:2vmin 3vmin;
z-index:8;text-align:center">
<div style="font-size:6vmin;font-weight:800;color:var(--green)">DRYING COMPLETE</div>
<div style="font-size:3vmin;margin-top:1vmin">INITIAL <b id="dWi">--</b> g &middot; FINAL <b id="dWf">--</b> g
&middot; TARGET <b id="dWt">--</b> g</div>
<div style="font-size:4vmin;margin-top:0.6vmin;color:var(--gold)">MOISTURE REMOVED <b id="dWm">--</b> g
&middot; DURATION <b id="dDu">--</b></div>
<div style="font-size:2.6vmin;color:var(--dim);margin-top:0.6vmin">batch ready for packaging &middot; the green light means GO</div></div>
<div id="clkPane"><b>&#9201; SET CLOCK</b><br>
<input type="datetime-local" id="kdt">
<button onclick="kSet()">Set</button>
<button onclick="kDev()">Use this device</button>
<button onclick="clkPane()" style="background:#4a5568">Close</button>
<div class="sml" style="color:var(--dim);margin-top:6px">full control: 192.168.4.1 &middot; keypad: menu 6/7 &middot; auto: open / on a phone</div></div>
<script>
var wake=null;
async function keepAwake(){try{wake=await navigator.wakeLock.request('screen')}catch(e){}}
keepAwake();document.addEventListener('visibilitychange',()=>{if(!document.hidden)keepAwake()});
function f1(v){return v==null?'--':(Math.round(v*10)/10)}
const p2=n=>String(n).padStart(2,'0');
var kb=0,kbm=0,kTz=330;
function kFmt(s){var d=new Date(s*1000);
var W=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
var M=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
return W[d.getUTCDay()]+' '+d.getUTCDate()+' '+M[d.getUTCMonth()]+' '
+p2(d.getUTCHours())+':'+p2(d.getUTCMinutes())+':'+p2(d.getUTCSeconds())}
function clkPane(){var p=$('clkPane');
p.style.display=(p.style.display==='block')?'none':'block'}
function kSet(){var v=$('kdt').value;if(!v)return;
fetch('/api/settime?epoch='+Math.floor(new Date(v).getTime()/1000)+'&tz='+kTz,{method:'POST'}).catch(function(){});clkPane()}
function kDev(){var d=new Date();if(d.getFullYear()<2021)return;
fetch('/api/settime?epoch='+Math.floor(d.getTime()/1000)+'&tz='+(-d.getTimezoneOffset()),{method:'POST'}).catch(function(){});clkPane()}
function mmss(s){s=Math.max(0,s|0);var h=(s/3600)|0,m=((s%3600)/60)|0;
return h?h+'h '+String(m).padStart(2,'0')+'m':m+'m '+String(s%60).padStart(2,'0')+'s'}
// ---- v2.0.19: virtual keypad - same keys as the pillar keypad ----------
var vpS=null;
function vesc(s){return String(s==null?'':s).replace(/&/g,'&amp;').replace(/</g,'&lt;')}
function vpToggle(){document.body.classList.toggle('vp');vpDraw()}
function vpKey(k){fetch('/api/key?k='+encodeURIComponent(k),{method:'POST'})
  .then(function(){tick()}).catch(function(){})}
function vpDraw(){
  var t=$('vpText');if(!t||!vpS)return;var m=vpS.menu;
  if(!m){t.innerHTML=
   '<div class="sel" style="font-size:5vmin">'+vesc(vpS.state||'--')+'</div>'+
   '<div>'+f1(vpS.tAvg)+'\u00B0C \u00B7 '+Math.round(vpS.hAvg||0)+'%RH \u00B7 '+
   (vpS.bat&&vpS.bat.valid?vpS.bat.pct+'% battery':'--')+'</div>'+
   '<div style="margin-top:2vmin;color:var(--dim)">Press <b>B</b> (MENU) to open the on-device menu \u00B7 <b>D</b> run/stop \u00B7 <b>C</b> mode \u00B7 <b>*</b> home \u00B7 <b>#</b> back \u00B7 digits type \u00B7 2/4/6/8 move</div>';}
  else if(m.edit){t.innerHTML=
   '<div class="sel">ENTER VALUE</div>'+
   '<div style="font-size:9vmin;font-weight:800;color:var(--gold)">'+vesc(m.buf||'--')+'</div>'+
   '<div style="color:var(--dim)">'+vesc(m.hint)+' \u00B7 A save \u00B7 # cancel</div>';}
  else{var h='';(m.items||[]).forEach(function(it,i){h+=
   '<div style="display:flex;justify-content:space-between;gap:2vmin'+(i===m.cur?';color:var(--gold);font-weight:800':'')+'">'+
   '<span>'+(i===m.cur?'▶ ':'')+vesc(it.n)+'</span><b>'+vesc(it.v)+'</b></div>'});
   t.innerHTML=h||'menu empty';}}
var miss=0;
async function tick(){
 try{
  const r=await fetch('/api/data',{cache:'no-store'});const S=await r.json();miss=0;
  vpS=S;vpDraw();
  $('fw').textContent='v'+(S.fw||'');
  var sup=S.supply||{};$('sup').textContent=sup.solar?'\u2600 SOLAR MODE':'\u26A1 BYPASS MODE';
  $('sup').className=sup.live?'ok':'bad';
  $('pmd').textContent=(sup.mode===2)?'SILICAGEL':(sup.mode===1)?'USER':'AGARBATTI';
  var st=S.state||'--';$('st').textContent=st;$('st').className=st;
  $('times').textContent=(st==='DRYING'||st==='PURGING')
   ?'ELAPSED '+mmss(S.elapsed)+' \u00B7 LEFT '+mmss(S.remaining)
   :'READY - open 192.168.4.1 to start';
  var s1=S.s1||{},s2=S.s2||{};
  $('t1').textContent=s1.ok?f1(s1.t)+'\u00B0':'--';
  $('t2').textContent=s2.ok?f1(s2.t)+'\u00B0':'--';
  $('ta').textContent=f1(S.tAvg)+'\u00B0';
  $('rh').textContent=f1(S.hAvg)+'%';
  var dh=S.dht||{};
  if(dh.ok){$('out').textContent=f1(dh.t)+'\u00B0 '+Math.round(dh.h)+'%';$('outsrc').textContent='measured'}
  else{$('out').textContent='--';$('outsrc').textContent='no sensor'}
  var b=S.bat||{};
  $('bat').textContent=b.valid?b.pct+'%':'--';
  $('batv').textContent=b.valid?f1(b.v):'--';$('batn').textContent=b.name||'';
  $('batBar').style.width=(b.pct||0)+'%';
  var sc=S.scale||{};
  $('wt').textContent=sc.ok?Math.round(sc.g)+'g':'--';
  var tg=S.set?(+S.set.targetG||0):0;
  $('wt2').textContent=tg>0?('\u2192 '+Math.round(tg)+'g \u00B7 diff '+Math.round((sc.g||0)-tg)+'g'):'no target';
  $('heat').textContent=(S.heat||0)+'%';$('heatBar').style.width=(S.heat||0)+'%';
  var fo=S.fanOut!=null?S.fanOut:0;
  $('fan').textContent=(fo?'ON ':'OFF ')+fo+'%';$('fanBar').style.width=fo+'%';
  var d=S.door||{};
  $('door').textContent=d.closed?'CLOSED':'OPEN';
  $('door2').textContent=d.phase||'--';
  $('set').textContent=S.set?f1(S.set.setTemp)+'\u00B0':'--';
  $('rate').textContent=sc.ok?f1(sc.rate):'--';
  if(S.clockSet){kb=S.now+(S.tz||0)*60;kbm=Date.now();kTz=S.tz||330}
  $('clk').textContent=kb?('⏱ '+kFmt(kb+(Date.now()-kbm)/1000)):'⏱ set clock';
  var dn=(st==='DONE');
  $('doneCard').style.display=dn?'block':'none';
  if(dn){$('dWi').textContent=Math.round(S.wtStart||0);
    $('dWf').textContent=Math.round(S.finalG||0);
    $('dWt').textContent=Math.round(S.set?(+S.set.targetG||0):0);
    $('dWm').textContent=Math.round(S.moistureG||0);
    $('dDu').textContent=mmss(S.cycleSecs||0);}
  $('fault').textContent=S.fault||'';
  $('hint').style.display=S.fault?'none':'inline';
 }catch(e){if(++miss>5){$('st').textContent='WAITING';$('st').className='';
  $('hint').textContent='waiting for the dryer - check WiFi AgarbattiDryer'}}
}
function $(i){return document.getElementById(i)}
setInterval(tick,1000);tick();
</script></body></html>)HTML";

/* ==========================  src/web.h  ========================== */
/**
 * @file web.h
 * @brief ESP32 access-point web server: dashboard + JSON API + captive DNS.
 */
#include <WebServer.h>
#include <DNSServer.h>
namespace web {
  void begin();
  void handle();
  void clockBoot();     // restore last-saved wall clock after full power-down
  void clockSetManual(time_t ep);   // keypad menu / manual set: set + persist
  void clockTick();     // persist the clock every TIME_SAVE_MS (call in loop)
}
  void clockBoot();     // restore last-saved wall clock after full power-down
  void clockSetManual(time_t ep);   // keypad menu / manual set: set + persist
  void clockTick();     // persist the clock every TIME_SAVE_MS (call in loop)
}
/* ==========================  src/pwm.cpp  ========================== */

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
// ---------- Arduino-ESP32 core 3.x : channel handled internally ----------
void pwmInitPin(int pin, uint32_t freqHz, uint8_t resBits) {
  ledcAttach(pin, freqHz, resBits);
  ledcWrite(pin, 0);
}
void pwmWritePin(int pin, uint32_t duty) { ledcWrite(pin, duty); }

#else
// ---------- Arduino-ESP32 core 2.x / 1.x : manual channel bookkeeping ----
static const int  kMaxCh = 8;
static int        chPin[kMaxCh];
static int        chCount = 0;

static int chOf(int pin) {
  for (int i = 0; i < chCount; i++) if (chPin[i] == pin) return i;
  return -1;
}

void pwmInitPin(int pin, uint32_t freqHz, uint8_t resBits) {
  if (chOf(pin) >= 0) return;
  if (chCount >= kMaxCh) return;
  int ch = chCount++;
  chPin[ch] = pin;
  ledcSetup(ch, freqHz, resBits);
  ledcAttachPin(pin, ch);
  ledcWrite(ch, 0);
}

void pwmWritePin(int pin, uint32_t duty) {
  int ch = chOf(pin);
  if (ch >= 0) ledcWrite(ch, duty);
}
#endif
/* ==========================  src/aht10.cpp  ========================== */

// AHT10 command set (see Aosong datasheet)
static const uint8_t AHT_CMD_SOFTRESET  = 0xBA;
static const uint8_t AHT_CMD_CALIBRATE  = 0xE1; // AHT10 init/calibrate
static const uint8_t AHT_CMD_TRIGGER    = 0xAC;
static const uint8_t AHT_STATUS_BUSY    = 0x80;
static const uint8_t AHT_STATUS_CALIB   = 0x08;

void AHT10::begin(TwoWire &bus, uint8_t addr, uint32_t periodMs) {
  _bus = &bus;
  _addr = addr;
  _period = periodMs;
  _stage = Stage::IDLE;
  _ok = false;
  _fails = 0;
  _t = NAN; _h = NAN;

  // Soft reset, then send the calibration command (blocking, runs in setup())
  _bus->beginTransmission(_addr);
  _bus->write(AHT_CMD_SOFTRESET);
  _bus->endTransmission();
  delay(25);
  cmd3(AHT_CMD_CALIBRATE, 0x08, 0x00);
  uint8_t st = 0;
  if (readStatus(st) && (st & AHT_STATUS_CALIB)) {
    _ok = true;
    _lastOk = millis();
  }
}

bool AHT10::cmd3(uint8_t a, uint8_t b, uint8_t c) {
  _bus->beginTransmission(_addr);
  _bus->write(a); _bus->write(b); _bus->write(c);
  return (_bus->endTransmission() == 0);
}

bool AHT10::readStatus(uint8_t &st) {
  _bus->requestFrom(_addr, (uint8_t)1);
  if (!_bus->available()) return false;
  st = (uint8_t)_bus->read();
  return true;
}

bool AHT10::update(uint32_t now) {
  bool newData = false;

  switch (_stage) {
    case Stage::IDLE:
      if (now - _lastReq >= _period) {
        if (cmd3(AHT_CMD_TRIGGER, 0x33, 0x00)) {
          _lastReq = now;
          _stage = Stage::MEASURING;
        } else {                              // bus error -> 2 s backoff
          _lastReq = now;
          if (++_fails > 5) _ok = false;
        }
      }
      break;

    case Stage::MEASURING:                    // measurement takes ~80 ms
      if (now - _lastReq >= 90) {
        uint8_t st = 0;
        if (!readStatus(st)) {                // bus error -> bounded retry
          if (++_readTries >= 3) {            // then give up on this cycle
            _readTries = 0;
            _stage = Stage::IDLE;
            _lastReq = now;
            if (++_fails > 5) _ok = false;
          }
          break;
        }
        _readTries = 0;
        if (st & AHT_STATUS_BUSY) break;      // still busy, come back later
        if (readResult()) {                   // fresh numbers
          _fails = 0;
          _ok = true;
          _lastOk = now;
          newData = true;
        } else if (++_fails > 5) {
          _ok = false;
        }
        _stage = Stage::IDLE;
        _lastReq = now;                       // schedule next cycle from now
      }
      break;
  }

  // stale readings count as a failure too
  if (_ok && (now - _lastOk > _period * 10 + 5000)) _ok = false;
  return newData;
}

bool AHT10::readResult() {
  uint8_t b[6];
  _bus->requestFrom(_addr, (uint8_t)6);
  if (_bus->available() < 6) return false;
  for (auto &x : b) x = (uint8_t)_bus->read();

  if (b[0] & AHT_STATUS_BUSY) return false;

  // 20-bit humidity / 20-bit temperature, datasheet formulas
  uint32_t rawH = ((uint32_t)b[1] << 12) | ((uint32_t)b[2] << 4) | (b[3] >> 4);
  uint32_t rawT = (((uint32_t)b[3] & 0x0F) << 16) | ((uint32_t)b[4] << 8) | b[5];

  float h = rawH * 100.0f / 1048576.0f;
  float t = rawT * 200.0f / 1048576.0f - 50.0f;

  if (isnan(h) || isnan(t) || h < 0 || h > 105 || t < -40 || t > 90) return false;

  _h = h;
  _t = t;
  return true;
}
/* ==========================  src/sensors.cpp  ========================== */

bool SensorModule::begin() {
  Wire.begin(PIN_I2C0_SDA, PIN_I2C0_SCL);
  _s1.begin(Wire, 0x38, SENSOR_PERIOD_MS);   // top of chamber
#if SENS2_DHT
  // chamber source #2 = DHT22 on the cool-return path;
  // dht::update() (main loop) drives its frames.
#else
  Wire1.begin(PIN_I2C1_SDA, PIN_I2C1_SCL);
  _s2.begin(Wire1, 0x38, SENSOR_PERIOD_MS);  // bottom of chamber
#endif
  recompute();
  return anyOk();
}

void SensorModule::update() {
  bool n1 = _s1.update(millis());
#if SENS2_DHT
  static uint32_t lastGen = 0;
  bool n2 = (dht::chamber.gen != lastGen);   // a new DHT22 frame arrived
  if (n2) lastGen = dht::chamber.gen;
#else
  bool n2 = _s2.update(millis());
#endif
  if (n1 || n2) recompute();
}

void SensorModule::recompute() {
  float tSum = 0, hSum = 0; int n = 0;
  float tM = -300, hM = -300;
  if (s1ok()) {
    tSum += t1(); hSum += h1(); n++;
    tM = max(tM, t1()); hM = max(hM, h1());
  }
  if (s2ok()) {
    tSum += t2(); hSum += h2(); n++;
    tM = max(tM, t2()); hM = max(hM, h2());
  }
  if (n > 0) {
    _tAvg = tSum / n; _hAvg = hSum / n;
  float tM = -300, hM = -300;
  if (s1ok()) {
    tSum += t1(); hSum += h1(); n++;
    tM = max(tM, t1()); hM = max(hM, h1());
  }
  if (s2ok()) {
    tSum += t2(); hSum += h2(); n++;
    tM = max(tM, t2()); hM = max(hM, h2());
  }
  if (n > 0) {
    _tAvg = tSum / n; _hAvg = hSum / n;
    _tMax = tM;       _hMax = hM;
  } else {
    _tAvg = NAN; _hAvg = NAN; _tMax = NAN; _hMax = NAN;
  }
}
/* ==========================  src/battery.cpp  ========================== */

// Resting-voltage approximations - good enough for a dryer dashboard.
static const BatteryProfile kProfiles[] = {
  { "3S Li-ion",  12.6f,  9.90f },   // 0
  { "4S Li-ion",  16.8f, 13.20f },   // 1
  { "12V SLA",    12.9f, 11.70f },   // 2
  { "4S LiFePO4", 14.2f, 10.00f },   // 3
};

const BatteryProfile &BatteryMonitor::profile(uint8_t id) {
  if (id > 3) id = 0;
  return kProfiles[id];
}

uint8_t BatteryMonitor::percentFor(float v, uint8_t type) {
  const BatteryProfile &p = profile(type);
  float pct = (v - p.v0) / (p.v100 - p.v0) * 100.0f;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return (uint8_t)(pct + 0.5f);
}

void BatteryMonitor::begin() {
  pinMode(PIN_VBAT_ADC, INPUT);
  analogSetPinAttenuation(PIN_VBAT_ADC, ADC_11db);   // full 0-2.45 V usable window
  if (PIN_VBAT_ENABLE >= 0) pinMode(PIN_VBAT_ENABLE, OUTPUT);
#if RELAYS_ENABLED
  pinMode(PIN_BYPASS_CTRL, OUTPUT);
  digitalWrite(PIN_BYPASS_CTRL, LOW);
#endif
  update();                                        // first reading right away
}

void BatteryMonitor::setBypass(bool on) {
  _bypass = on;
#if RELAYS_ENABLED
  digitalWrite(PIN_BYPASS_CTRL, on ? HIGH : LOW);
#endif
}

float BatteryMonitor::readVoltsOnce() {
  // 16x oversample to tame the ESP32 ADC noise
  uint32_t acc = 0;
  for (int i = 0; i < 16; i++) acc += analogRead(PIN_VBAT_ADC);
  float raw = (acc / 16.0f) / 4095.0f;
  return raw * VBAT_ADC_REF * VBAT_DIV_RATIO + VBAT_CAL_OFFSET;
}

void BatteryMonitor::update() {
  uint32_t now = millis();
  if (_last != 0 && (now - _last) < BATT_PERIOD_MS) return;
  _last = now ? now : 1;

  if (PIN_VBAT_ENABLE >= 0) {
    digitalWrite(PIN_VBAT_ENABLE, HIGH);   // pull the divider down to GND
    delay(8);                              // let the ADC node settle
  }
  float v1 = readVoltsOnce();
  float v2 = readVoltsOnce();
  if (PIN_VBAT_ENABLE >= 0) digitalWrite(PIN_VBAT_ENABLE, LOW);

  _v = (v1 + v2) / 2.0f;
  _valid = _v > 1.0f;                      // below 1 V -> nothing connected
  _pct = percentFor(_v, _type);
}

/* ==========================  src/buzzer.cpp  ========================== */

// ---- pattern table: segments of (on ms, off ms); off==0 = last segment
struct Pat {
  uint8_t  n;                 // segment count
  uint16_t on[6];
  uint16_t off[6];
  uint8_t  prio;              // higher preempts
};

static const Pat P[] = {
  /*NONE*/        {0, {0,0,0,0,0,0}, {0,0,0,0,0,0}, 0},
  /*KEY*/         {1, {50,0,0,0,0,0},          {0,0,0,0,0,0},            1}, // #1
  /*INVALID*/     {2, {80,80,0,0,0,0},         {100,0,0,0,0,0},          2}, // #2
  /*TICK*/        {1, {50,0,0,0,0,0},          {0,0,0,0,0,0},            4}, // #3
  /*MODE*/        {1, {100,0,0,0,0,0},         {0,0,0,0,0,0},            2}, // #4
  /*SAVED*/       {2, {80,80,0,0,0,0},         {80,0,0,0,0,0},           3}, // #5
  /*BACK*/        {1, {100,0,0,0,0,0},         {0,0,0,0,0,0},            1}, // #6
  /*DOOR_AJAR*/   {3, {150,150,150,0,0,0},     {100,100,0,0,0,0},        7}, // #7 /2s
  /*NO_TRAYS*/    {3, {100,100,300,0,0,0},     {150,150,0,0,0,0},        6}, // #8 /3s
  /*UNSTABLE*/    {2, {100,100,0,0,0,0},       {150,0,0,0,0,0},          5}, // #9 /3s
  /*READY*/       {1, {500,0,0,0,0,0},         {0,0,0,0,0,0},            3}, // #11
  /*BATT_LOW*/    {3, {150,150,150,0,0,0},     {100,100,0,0,0,0},        5}, // #12
  /*BATT_CRIT*/   {4, {100,100,100,100,0,0},   {100,100,100,0,0,0},      8}, // #13 /30s
  /*SETPOINT*/    {2, {100,100,0,0,0,0},       {100,0,0,0,0,0},          3}, // #14
  /*FAN_ON*/      {1, {80,0,0,0,0,0},          {0,0,0,0,0,0},            2}, // #15
  /*DOOR_OPEN_RUN*/{1,{100,0,0,0,0,0},         {0,0,0,0,0,0},            9}, // #16 /150ms
  /*MIDWAY*/      {2, {100,100,0,0,0,0},       {150,0,0,0,0,0},          3}, // #17
  /*APPROACH*/    {2, {80,80,0,0,0,0},         {100,0,0,0,0,0},          4}, // #18 /60s
  /*TIMEOUT5*/    {3, {100,100,100,0,0,0},     {100,100,0,0,0,0},        5}, // #19 /30s
  /*WARN*/        {2, {150,150,0,0,0,0},       {150,0,0,0,0,0},          6}, // #23 /15s
  /*CRITICAL*/    {1, {5000,0,0,0,0,0},        {0,0,0,0,0,0},           10}, // #24a
  /*CRITICAL_R*/  {2, {250,250,0,0,0,0},       {250,0,0,0,0,0},          6}, // #24b /10s
  /*RECOVERED*/   {2, {50,50,0,0,0,0},         {60,0,0,0,0,0},           3}, // #25
  /*E05_LOAD*/    {1, {2000,0,0,0,0,0},        {0,0,0,0,0,0},            8}, // #26
  /*AP_UP*/       {1, {80,0,0,0,0,0},          {0,0,0,0,0,0},            2}, // #27
  /*CLIENT*/      {1, {120,0,0,0,0,0},         {0,0,0,0,0,0},            2}, // #28
  /*LOG_SAVED*/   {2, {80,80,0,0,0,0},         {80,0,0,0,0,0},           3}, // #29
  /*STORE_FULL*/  {3, {100,100,100,0,0,0},     {100,100,0,0,0,0},        4}, // #30
  /*BROWNOUT_RET*/{3, {120,120,120,0,0,0},     {120,120,0,0,0,0},        4}, // #21
  /*ANOMALY*/     {3, {80,80,80,0,0,0},        {80,80,0,0,0,0},          5}, // #22
  /*MAINT*/       {3, {120,120,120,0,0,0},     {150,150,0,0,0,0},        4}, // #33
  /*INIT_FAIL*/   {1, {2000,0,0,0,0,0},        {0,0,0,0,0,0},            8}, // #34
  /*COOL_DONE*/   {1, {200,0,0,0,0,0},         {0,0,0,0,0,0},            2}, // #35
  /*SHUTDOWN*/    {1, {1000,0,0,0,0,0},        {0,0,0,0,0,0},            9}, // #36
  /*FACT_RESET*/  {4, {1500,100,100,100,0,0},  {200,100,100,0,0,0},      9}, // #37
  /*POWER_ON*/    {6, {250,250,250,250,250,250},{250,250,250,250,250,0}, 4},
  /*CYCLE_START*/ {1, {3000,0,0,0,0,0},        {0,0,0,0,0,0},            9},
  /*CYCLE_DONE*/  {1, {5000,0,0,0,0,0},        {0,0,0,0,0,0},            9},
  /*ERROR*/       {1, {5000,0,0,0,0,0},        {0,0,0,0,0,0},           10},
  /*DOOR*/        {1, {1000,0,0,0,0,0},        {0,0,0,0,0,0},            5},
  /*MODE_CHANGE*/ {1, {3000,0,0,0,0,0},        {0,0,0,0,0,0},            8},
};

#if BUZZER_ENABLED

Buzzer buzzer;
struct Pat {
  uint8_t  n;                 // segment count
  uint16_t on[6];
  uint16_t off[6];
  uint8_t  prio;              // higher preempts
};

static const Pat P[] = {
  /*NONE*/        {0, {0,0,0,0,0,0}, {0,0,0,0,0,0}, 0},
  /*KEY*/         {1, {50,0,0,0,0,0},          {0,0,0,0,0,0},            1}, // #1
  /*INVALID*/     {2, {80,80,0,0,0,0},         {100,0,0,0,0,0},          2}, // #2
  /*TICK*/        {1, {50,0,0,0,0,0},          {0,0,0,0,0,0},            4}, // #3
  /*MODE*/        {1, {100,0,0,0,0,0},         {0,0,0,0,0,0},            2}, // #4

void Buzzer::beep(uint8_t n, uint16_t onMs, uint16_t offMs) {
  if (n == 0) return;
  _p = BP::NONE;                     // legacy beep owns the driver
  _bTot = n; _bDone = 0;
  _bOnMs = onMs; _bOffMs = offMs;
  _bOn = true;
  _tEdge = millis();
  drive(true);
}

void Buzzer::playPat(BP p) {
  const Pat &pat = P[(uint8_t)p];
  if (pat.n == 0) return;
  _bTot = 0;                         // pattern preempts any legacy beep
  if (_p != BP::NONE && P[(uint8_t)_p].prio > pat.prio) return; // quieter wins
  _p = p; _seg = 0; _on = true; _tEdge = millis();
  drive(true);
}

void Buzzer::addRepeat(BP p, uint32_t periodMs) {
  for (auto &r : _r)
    if (r.p == p) { r.period = periodMs; return; }        // already on
  for (auto &r : _r)
    if (r.p == BP::NONE) { r.p = p; r.period = periodMs; r.last = millis(); return; }
  uint8_t low = 0;                                        // table full:
  for (uint8_t i = 1; i < 4; i++)                         // drop the quietest
    if (P[(uint8_t)_r[i].p].prio < P[(uint8_t)_r[low].p].prio) low = i;
  _r[low] = {p, periodMs, millis()};
}

void Buzzer::delRepeat(BP p) {
  for (auto &r : _r) if (r.p == p) r.p = BP::NONE;
}

void Buzzer::stopAll() {
  for (auto &r : _r) r.p = BP::NONE;
}

bool Buzzer::repeating(BP p) {
  for (auto &r : _r) if (r.p == p) return true;
  return false;
}

void Buzzer::pump() {
  if (_p != BP::NONE) return;                             // busy
  R *best = nullptr;
  uint32_t now = millis();
  for (auto &r : _r) {
    if (r.p == BP::NONE || now - r.last < r.period) continue;
    if (!best || P[(uint8_t)r.p].prio > P[(uint8_t)best->p].prio) best = &r;
  }
  if (best) { best->last = now; playPat(best->p); }
}

void Buzzer::update() {
  if (_bTot) {                                  // legacy beep() cadence
    uint32_t nowB = millis();
    uint16_t phase = _bOn ? _bOnMs : _bOffMs;
    if (nowB - _tEdge >= phase) {
      if (_bOn) {                               // on-phase over
        drive(false); _bOn = false; _tEdge = nowB;
        if (++_bDone >= _bTot) _bTot = 0;       // cadence finished
      } else {                                  // gap over -> next beep
        _bOn = true; _tEdge = nowB; drive(true);
      }
    }
    return;
  }
  pump();
  if (_p == BP::NONE) return;
  const Pat &pat = P[(uint8_t)_p];
  uint32_t now = millis();
  uint16_t phase = _on ? pat.on[_seg] : pat.off[_seg];
  if (now - _tEdge >= phase) {
    if (_on) {
      drive(false); _on = false; _tEdge = now;
      if (pat.off[_seg] == 0) _p = BP::NONE;   // last segment ends pattern
    } else {
      if (_seg + 1 < pat.n) { _seg++; _on = true; _tEdge = now; drive(true); }
      else _p = BP::NONE;
    }
  }
}

#else
Buzzer buzzer;                                  // stub class from header
#endif

// ---- public API --------------------------------------------------------
namespace bz {
  void play(BP p)                { buzzer.playPat(p); }
  void startRepeat(BP p, uint32_t periodMs) { buzzer.addRepeat(p, periodMs); }
  void stopRepeat(BP p)          { buzzer.delRepeat(p); }
  void stopAllRepeats()          { buzzer.stopAll(); }
  bool repeating(BP p)           { return buzzer.repeating(p); }
  // classic law
  void powerOn()    { play(BP::POWER_ON); }
  void cycleStart() { play(BP::CYCLE_START); }
  void cycleDone()  { play(BP::CYCLE_DONE); }
  void error()      { play(BP::ERROR); }
  void door()       { play(BP::DOOR); }
  void modeChange() { play(BP::MODE_CHANGE); }
}
  /*COOL_DONE*/   {1, {200,0,0,0,0,0},         {0,0,0,0,0,0},            2}, // #35
  /*SHUTDOWN*/    {1, {1000,0,0,0,0,0},        {0,0,0,0,0,0},            9}, // #36
  /*FACT_RESET*/  {4, {1500,100,100,100,0,0},  {200,100,100,0,0,0},      9}, // #37
  /*POWER_ON*/    {6, {250,250,250,250,250,250},{250,250,250,250,250,0}, 4},
  /*CYCLE_START*/ {1, {3000,0,0,0,0,0},        {0,0,0,0,0,0},            9},
  /*CYCLE_DONE*/  {1, {5000,0,0,0,0,0},        {0,0,0,0,0,0},            9},
  /*ERROR*/       {1, {5000,0,0,0,0,0},        {0,0,0,0,0,0},           10},
  /*DOOR*/        {1, {1000,0,0,0,0,0},        {0,0,0,0,0,0},            5},
  /*MODE_CHANGE*/ {1, {3000,0,0,0,0,0},        {0,0,0,0,0,0},            8},
};

#if BUZZER_ENABLED

Buzzer buzzer;

void Buzzer::drive(bool on) {
#if BUZZER_ACTIVE_HIGH
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
#else
  digitalWrite(PIN_BUZZER, on ? LOW : HIGH);
#endif
}

void Buzzer::begin() {
  pinMode(PIN_BUZZER, OUTPUT);
  drive(false);
}

void Buzzer::playPat(BP p) {
  const Pat &pat = P[(uint8_t)p];
  if (pat.n == 0) return;
  if (_p != BP::NONE && P[(uint8_t)_p].prio > pat.prio) return; // quieter wins
  _p = p; _seg = 0; _on = true; _tEdge = millis();
  drive(true);
}

void Buzzer::addRepeat(BP p, uint32_t periodMs) {
  for (auto &r : _r)
    if (r.p == p) { r.period = periodMs; return; }        // already on
  for (auto &r : _r)
    if (r.p == BP::NONE) { r.p = p; r.period = periodMs; r.last = millis(); return; }
  uint8_t low = 0;                                        // table full:
  for (uint8_t i = 1; i < 4; i++)                         // drop the quietest
    if (P[(uint8_t)_r[i].p].prio < P[(uint8_t)_r[low].p].prio) low = i;
  _r[low] = {p, periodMs, millis()};
}

void Buzzer::delRepeat(BP p) {
  for (auto &r : _r) if (r.p == p) r.p = BP::NONE;
}

void Buzzer::stopAll() {
  for (auto &r : _r) r.p = BP::NONE;
}

bool Buzzer::repeating(BP p) {
  for (auto &r : _r) if (r.p == p) return true;
  return false;
}

void Buzzer::pump() {
  if (_p != BP::NONE) return;                             // busy
  R *best = nullptr;
  uint32_t now = millis();
  for (auto &r : _r) {
    if (r.p == BP::NONE || now - r.last < r.period) continue;
    if (!best || P[(uint8_t)r.p].prio > P[(uint8_t)best->p].prio) best = &r;
  }
  if (best) { best->last = now; playPat(best->p); }
}

void Buzzer::update() {
  pump();
  if (_p == BP::NONE) return;
  const Pat &pat = P[(uint8_t)_p];
  uint32_t now = millis();
  uint16_t phase = _on ? pat.on[_seg] : pat.off[_seg];
  if (now - _tEdge >= phase) {
    if (_on) {
      drive(false); _on = false; _tEdge = now;
      if (pat.off[_seg] == 0) _p = BP::NONE;   // last segment ends pattern
    } else {
      if (_seg + 1 < pat.n) { _seg++; _on = true; _tEdge = now; drive(true); }
      else _p = BP::NONE;
    }
  }
}

#else
Buzzer buzzer;                                  // stub class from header
#endif

// ---- public API --------------------------------------------------------
namespace bz {
  void play(BP p)                { buzzer.playPat(p); }
  void startRepeat(BP p, uint32_t periodMs) { buzzer.addRepeat(p, periodMs); }
  void stopRepeat(BP p)          { buzzer.delRepeat(p); }
  void stopAllRepeats()          { buzzer.stopAll(); }
  bool repeating(BP p)           { return buzzer.repeating(p); }
  // classic law
  void powerOn()    { play(BP::POWER_ON); }
  void cycleStart() { play(BP::CYCLE_START); }
  void cycleDone()  { play(BP::CYCLE_DONE); }
  void error()      { play(BP::ERROR); }
  void door()       { play(BP::DOOR); }
  void modeChange() { play(BP::MODE_CHANGE); }
}
/* ==========================  src/eelog.cpp  ========================== */

#if ELOG_ENABLED
#include <Wire.h>

namespace eelog {

// ---- chip geometry ------------------------------------------------------
static const uint16_t CHIP_B  = 32768;      // AT24C256
static const uint16_t PAGE    = 64;
static const uint16_t R_SIZE  = 40;
static const uint16_t R_BASE  = 64;         // page 0 = header
static const uint16_t R_MAX   = (CHIP_B - R_BASE) / R_SIZE;   // 817
static const uint32_t MAGIC   = 0x454C4F47; // "GOLE" LE = "ELOG"

struct Hdr {
  uint32_t magic;
  uint16_t ver;
  uint16_t slots;
  uint16_t count;
  uint16_t head;       // next write index
  uint32_t seq;        // last sequence number handed out
};

static Hdr   s_h = {};
static bool  s_ok = false;

// ---- low-level: random/sequential reads + page-safe writes -------------
static bool rd(uint16_t a, uint8_t *b, uint16_t n) {
  while (n) {
    uint8_t chunk = n > 28 ? 28 : n;
    Wire.beginTransmission(ELOG_ADDR);
    Wire.write((uint8_t)(a >> 8));  Wire.write((uint8_t)a);
    if (Wire.endTransmission(false) != 0) return false;   // rep start
    if (Wire.requestFrom((int)ELOG_ADDR, (int)chunk) != chunk) return false;
    for (uint8_t i = 0; i < chunk; i++) b[i] = (uint8_t)Wire.read();
    a += chunk;  b += chunk;  n -= chunk;
  }
  return true;
}

static bool wrPage(uint16_t a, const uint8_t *b, uint16_t n) {
  // n never crosses a page boundary (callers guarantee it)
  Wire.beginTransmission(ELOG_ADDR);
  Wire.write((uint8_t)(a >> 8));  Wire.write((uint8_t)a);
  for (uint16_t i = 0; i < n; i++) Wire.write(b[i]);
  if (Wire.endTransmission() != 0) return false;
  delay(5);                                // AT24 twr write cycle
  return true;
}

static bool wr(uint16_t a, const uint8_t *b, uint16_t n) {
  while (n) {
    uint16_t inPage = PAGE - (a % PAGE);
    uint16_t chunk = n < inPage ? n : inPage;
    if (!wrPage(a, b, chunk)) return false;
    a += chunk;  b += chunk;  n -= chunk;
  }
  return true;
}

// ---- record pack/unpack (explicit, endian/padding-safe) ----------------
static uint8_t crc8(const uint8_t *b, uint16_t n) {
  uint8_t c = 0x5A;
  while (n--) { c ^= *b++; for (uint8_t i = 0; i < 8; i++) c = (c & 0x80) ? (c << 1) ^ 0x07 : (c << 1); }
  return c;
}

static void putU32(uint8_t *p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
static uint32_t getU32(const uint8_t *p)   { return p[0] | (p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static void putU16(uint8_t *p, uint16_t v) { p[0]=v; p[1]=v>>8; }
static uint16_t getU16(const uint8_t *p)   { return p[0] | (p[1]<<8); }

static void pack(const EeRec &r, uint8_t *b) {
  memset(b, 0, R_SIZE);
  putU32(b + 0,  r.seq);        putU32(b + 4,  r.startEpoch);
  putU32(b + 8,  r.durS);
  b[12]=r.mode; b[13]=r.endR; b[14]=r.ecode; b[15]=r.flags;
  putU16(b + 16, (uint16_t)r.setT10);  putU16(b + 18, (uint16_t)r.tAvg10);
  putU16(b + 20, (uint16_t)r.tMax10);  putU16(b + 22, (uint16_t)r.hMax10);
  putU16(b + 24, (uint16_t)r.outT10);  putU16(b + 26, (uint16_t)r.wtS10);
  putU16(b + 28, (uint16_t)r.wtE10);   putU16(b + 30, (uint16_t)r.wtT10);
  putU16(b + 32, (uint16_t)r.vbS10);   putU16(b + 34, (uint16_t)r.vbE10);
  b[38] = crc8(b, 38);
}

static bool unpack(EeRec &r, const uint8_t *b) {
  if (b[38] != crc8(b, 38)) return false;   // torn/garbage record
  r.seq = getU32(b + 0);   r.startEpoch = getU32(b + 4);
  r.durS = getU32(b + 8);
  r.mode=b[12]; r.endR=b[13]; r.ecode=b[14]; r.flags=b[15];
  r.setT10=(int16_t)getU16(b+16); r.tAvg10=(int16_t)getU16(b+18);
  r.tMax10=(int16_t)getU16(b+20); r.hMax10=(int16_t)getU16(b+22);
  r.outT10=(int16_t)getU16(b+24); r.wtS10=(int16_t)getU16(b+26);
  r.wtE10=(int16_t)getU16(b+28);  r.wtT10=(int16_t)getU16(b+30);
  r.vbS10=(int16_t)getU16(b+32);  r.vbE10=(int16_t)getU16(b+34);
  return true;
}

static bool writeHdr() {
  uint8_t b[16];
  putU32(b + 0, s_h.magic);  putU16(b + 4, s_h.ver);  putU16(b + 6, s_h.slots);
  putU16(b + 8, s_h.count);  putU16(b + 10, s_h.head);
  putU32(b + 12, s_h.seq);
  return wr(0, b, 16);
}

// ---- public API ---------------------------------------------------------
void begin() {
  Wire.beginTransmission(ELOG_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println(F("[eelog] AT24C256 not found - long-term registry off"));
    return;
  }
  uint8_t b[16];
  if (!rd(0, b, 16)) return;
  bool fresh = (getU32(b + 0) != MAGIC);
  if (!fresh) {
    s_h.magic = getU32(b + 0);  s_h.ver   = getU16(b + 4);
    s_h.slots = getU16(b + 6);  s_h.count = getU16(b + 8);
    s_h.head  = getU16(b + 10); s_h.seq   = getU32(b + 12);
    if (s_h.slots != R_MAX || s_h.count > R_MAX || s_h.head >= R_MAX)
      fresh = true;                              // junk header -> reformat
  }
  if (fresh) {
    s_h = { MAGIC, 1, R_MAX, 0, 0, 0 };
    if (!writeHdr()) return;
    Serial.println(F("[eelog] AT24C256 32 kB: registry formatted (817 slots)"));
  } else {
    // torn-write rollback: validate the record HEAD points at
    uint8_t rb[R_SIZE];
    EeRec tmp;
    if (s_h.count > 0) {
      uint16_t last = (uint16_t)((s_h.head + R_MAX - 1) % R_MAX);
      if (rd(R_BASE + (uint16_t)last * R_SIZE, rb, R_SIZE) &&
          !unpack(tmp, rb)) {
        // last record is corrupt -> drop it
        s_h.head = last;
        if (s_h.count > 0) s_h.count--;
        if (s_h.seq > 0) s_h.seq--;
        writeHdr();
      }
    }
    Serial.printf("[eelog] AT24C256 32 kB cycle registry: %u/%u summaries "
                  "(seq %u)\n", s_h.count, R_MAX, (unsigned)s_h.seq);
  }
  s_ok = true;
}

bool ok()                { return s_ok; }
uint16_t count()         { return s_h.count; }
uint16_t slots()         { return R_MAX; }

void append(EeRec &r) {
  if (!s_ok) return;
  r.seq = ++s_h.seq;
  uint8_t b[R_SIZE];
  pack(r, b);
  if (!wr(R_BASE + s_h.head * R_SIZE, b, R_SIZE)) return;
  s_h.head = (s_h.head + 1) % R_MAX;
  if (s_h.count < R_MAX) s_h.count++;
  writeHdr();
}

bool get(uint16_t i, EeRec &r) {
  if (!s_ok || i >= s_h.count) return false;
  uint16_t idx = (uint16_t)((s_h.head + R_MAX - s_h.count + i) % R_MAX);
  uint8_t b[R_SIZE];
  if (!rd(R_BASE + idx * R_SIZE, b, R_SIZE)) return false;
  return unpack(r, b);
}

void clear() {
  if (!s_ok) return;
  s_h.count = 0;  s_h.head = 0;  s_h.seq = 0;
  writeHdr();
}

}  // namespace eelog

#endif
/* ==========================  src/rtc.cpp  ========================== */
/**
 * @file rtc.cpp
 * @brief DS1302 real-time clock driver - 3-wire bit-bang, library-free.
 * See rtc.h for wiring + behaviour. Pins come from the variant config
 * (S3: RST 40 / SCLK 42 / I-O 47; classic: -1 = not fitted).
 */

#if RTC_ENABLED

namespace rtc {

// ---- calendar math (proleptic Gregorian, no TZ env involved) ------------
// Days between 1970-01-01 and the civil date (Howard Hinnant's algorithm).
static long daysSinceEpoch(int y, int mo, int d) {
  int yy = y - (mo <= 2 ? 1 : 0);
  int era = (yy >= 0 ? yy : yy - 399) / 400;
  int yoe = yy - era * 400;                                   // [0,399]
  int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0,365]
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;            // [0,146096]
  return (long)era * 146097L + doe - 719468;
}

static time_t civilToEpoch(int y, int mo, int d, int h, int mi, int s) {
  return daysSinceEpoch(y, mo, d) * 86400L + h * 3600L + mi * 60L + s;
}

// DS1302 day-of-week register value for a civil date: 1 = Sunday .. 7.
// 1970-01-01 was a Thursday (=5), hence the +4.
static uint8_t dowFromCivil(int y, int mo, int d) {
  long days = daysSinceEpoch(y, mo, d);
  return (uint8_t)(((days % 7) + 7 + 4) % 7 + 1);
}

static void epochToCivil(time_t ep, int *y, int *mo, int *d,
                         int *h, int *mi, int *s) {
  long days = ep / 86400;                       // ep is 2016+ here
  int rem = (int)(ep - days * 86400);
  *h = rem / 3600;  rem %= 3600;
  *mi = rem / 60;   *s = rem % 60;
  days += 719468;
  int era = (days >= 0 ? days : days - 146096) / 146097;
  int doe = (int)(days - era * 146097);                    // [0,146096]
  int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0,399]
  int y_ = yoe + era * 400;
  int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);       // [0,365]
  int mp = (5 * doy + 2) / 153;                            // [0,11]
  *d = doy - (153 * mp + 2) / 5 + 1;                       // [1,31]
  *mo = mp + (mp < 10 ? 3 : -9);                           // [1,12]
  *y = y_ + (*mo <= 2 ? 1 : 0);                            // Jan/Feb belong to the next yy
}

static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

// ---- DS1302 bit-bang ------------------------------------------------------
// Frame: RST low -> high (chip select), START bit 0, 8-bit command byte
// LSB-first, data bytes (in read mode the chip drives I/O, updating on
// each SCLK falling edge), STOP bit 1, RST low.
// Command byte: bit7=1, bit6=0 (clock data), bits5-1 = register,
// bit0 = R/W (0 write / 1 read). Time: 0x80 write / 0x81 read; control
// register: 0x8E write / 0x8F read; scratch RAM byte 0: 0xC0 (write) /
// 0xC1 (read); registers auto-increment in burst.
// Time registers 0-6 BCD: sec min hr dow date month year. CH (halt)
// flag = bit7 of the SECONDS register; WP (write-protect) = bit7 of the
// control register, power-on state UNDEFINED - always cleared before a
// write (datasheet requirement).

static inline void ceHiLo(bool hi) { digitalWrite(PIN_RTC_RST, hi ? HIGH : LOW); }
static inline void clkHiLo(bool hi) { digitalWrite(PIN_RTC_SCLK, hi ? HIGH : LOW); }

static void rtcBitWrite(bool b) {
  digitalWrite(PIN_RTC_IO, b ? HIGH : LOW);
  clkHiLo(true);
  clkHiLo(false);
}

static bool rtcBitRead() {
  clkHiLo(true);
  clkHiLo(false);                              // falling edge: chip updates
  return (digitalRead(PIN_RTC_IO) == HIGH);    // bit, valid during the low
}                                              // phase until the next fall

static void byteWrite(uint8_t v) {
  pinMode(PIN_RTC_IO, OUTPUT);
  for (int i = 0; i < 8; i++) rtcBitWrite((v >> i) & 1);
}

static uint8_t byteRead() {
  pinMode(PIN_RTC_IO, INPUT);
  uint8_t v = 0;
  for (int i = 0; i < 8; i++) if (rtcBitRead()) v |= (uint8_t)(1u << i);
  return v;
}

static void xfer(uint8_t addr, const uint8_t *data, uint8_t n, uint8_t *out) {
  ceHiLo(false);
  ceHiLo(true);                                   // chip select (CE = RST)
  rtcBitWrite(false);                                // START
  byteWrite(addr);
  if (addr & 0x01) {                              // R/W bit (bit0): 1 = read
    for (uint8_t i = 0; i < n; i++) out[i] = byteRead();
  } else {
    for (uint8_t i = 0; i < n; i++) byteWrite(data[i]);
  }
  rtcBitWrite(true);                                 // STOP
  ceHiLo(false);
}

// sane = a time a real module would hold (year 2019-2099 keeps this
// forgiving for modules that arrive with a seller-set date)
static bool sane(const uint8_t *t) {
  return bcd2bin(t[0]) < 60 && bcd2bin(t[1]) < 60
      && bcd2bin(t[2] & 0x1F) <= 23                      // bits7-5 = mode/PM
      && bcd2bin(t[4]) >= 1 && bcd2bin(t[4]) <= 31
      && bcd2bin(t[5]) >= 1 && bcd2bin(t[5]) <= 12
      && bcd2bin(t[6]) >= 19 && bcd2bin(t[6]) <= 99;
}

static bool sPresent = false;
static bool sTrusted = false;

bool begin() {
  sPresent = false;
  sTrusted = false;
  if (PIN_RTC_RST < 0 || PIN_RTC_SCLK < 0 || PIN_RTC_IO < 0) return false;
  pinMode(PIN_RTC_RST, OUTPUT);  ceHiLo(false);
  pinMode(PIN_RTC_SCLK, OUTPUT); clkHiLo(false);
  pinMode(PIN_RTC_IO, OUTPUT);   digitalWrite(PIN_RTC_IO, LOW);

  {
    uint8_t zero = 0x00;
    xfer(0x8E, &zero, 1, nullptr);               // clear WP (power-on state
  }                                              // is undefined per datasheet)
  // Deterministic presence probe on scratch RAM byte 0: read it, write a
  // magic value, read it back, restore it. A dangling bus echoes nothing,
  // so only a real chip can return the magic.
  uint8_t orig = 0, magic = 0xA5, back = 0;
  xfer(0xC1, nullptr, 1, &orig);
  xfer(0xC0, &magic, 1, nullptr);
  xfer(0xC1, nullptr, 1, &back);
  xfer(0xC0, &orig, 1, nullptr);                 // give the byte back
  sPresent = (back == magic);
  if (!sPresent) return false;
  {
    uint8_t a[7];
    xfer(0x81, nullptr, 7, a);
    // CH flag (bit7 of seconds) set = clock halted: factory-fresh modules
    // ship this way with garbage registers, so the time is untrusted until
    // the first real set (site sync or 'rtcset').
    sTrusted = sane(a) && !(a[0] & 0x80);
  }
  return true;
}

bool present() { return sPresent; }

bool readTime(time_t *outEp) {
  if (!sPresent) return false;
  uint8_t t[7];
  xfer(0x81, nullptr, 7, t);
  if (!sane(t)) return false;
  int year = 2000 + bcd2bin(t[6]);
  // Hours register: bit7 = 12 h mode select, bit5 = AM/PM (12 h mode),
  // bits4-0 = hour BCD (0-23 in 24 h mode, 1-12 in 12 h mode).
  int hour;
  if (t[2] & 0x80) {                              // 12-hour mode
    hour = bcd2bin(t[2] & 0x1F);                  // 1-12
    if (t[2] & 0x20) hour = (hour == 12) ? 12 : hour + 12;   // PM
    else            hour = (hour == 12) ? 0  : hour;         // AM
  } else                                          // 24-hour mode
    hour = bcd2bin(t[2] & 0x1F);
  time_t ep = civilToEpoch(year, bcd2bin(t[5]), bcd2bin(t[4]),
                           hour, bcd2bin(t[1]), bcd2bin(t[0]));
  ep -= (time_t)cfg.tzMinutes * 60;              // chip holds LOCAL time
  *outEp = ep;
  return true;
}

void writeNow() {
  if (!sPresent) return;
  time_t ep = time(nullptr);
  if (ep <= (time_t)1700000000) return;          // don't push an unset clock
  int y, mo, d, h, mi, s;
  epochToCivil(ep + (time_t)cfg.tzMinutes * 60, &y, &mo, &d, &h, &mi, &s);
  uint8_t dow = dowFromCivil(y, mo, d);    // from the LOCAL civil date
  uint8_t t[7] = {
    bin2bcd((uint8_t)s), bin2bcd((uint8_t)mi),
    bin2bcd((uint8_t)(h & 0x1F)),        // 24 h (bit5 = 12-h flag = 0)
    bin2bcd(dow), bin2bcd((uint8_t)d), bin2bcd((uint8_t)mo),
    bin2bcd((uint8_t)((y >= 2000 ? y - 2000 : y) % 100)),
  };
  {
    uint8_t zero = 0x00;
    xfer(0x8E, &zero, 1, nullptr);               // clear WP before the write
    xfer(0x80, t, 7, nullptr);                   // burst; seconds bit7=0
  }                                              // clears CH: now ticking
  sTrusted = true;
}

const char *statusText() {
  static char b[64];
  if (!sPresent)
    snprintf(b, sizeof(b), "no DS1302 (clock = phone sync + NVS)");
  else if (sTrusted) {
    time_t ep;
    if (readTime(&ep)) {
      time_t local = ep + (time_t)cfg.tzMinutes * 60;
      int y, mo, d, h, mi, s;
      epochToCivil(local, &y, &mo, &d, &h, &mi, &s);
      snprintf(b, sizeof(b), "DS1302 OK %04d-%02d-%02d %02d:%02d:%02d (coin-cell)",
               y, mo, d, h, mi, s);
    } else snprintf(b, sizeof(b), "DS1302 OK but registers read garbage");
  } else
    snprintf(b, sizeof(b), "DS1302 present, no valid time yet (open site or 'rtcset')");
  return b;
}

}  // namespace rtc

#endif  // RTC_ENABLED
/* ==========================  src/keypad.cpp  ========================== */

#if KEYPAD_ENABLED

I2CKeypad keypad;

// row on P0-P3 (drive one LOW at a time), column read back on P4-P7.
// Keys:  rows = digits blocks, cols = 4th column is A/B/C/D.
const char I2CKeypad::kMap[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'},
};

void I2CKeypad::begin(TwoWire &bus, uint8_t addr) {
  _bus = &bus;
  _addr = addr;
  _bus->beginTransmission(_addr);
  _bus->write(0xFF);                       // all pins high (idle)
  _ok = (_bus->endTransmission() == 0);
}

// one sweep: drive each row low in turn, read the columns
char I2CKeypad::scan() {
  for (uint8_t r = 0; r < 4; r++) {
    uint8_t out = 0xF0 | (uint8_t)~(1u << r);   // rows: only r low; cols high
    _bus->beginTransmission(_addr);
    _bus->write(out);
    if (_bus->endTransmission() != 0) { _ok = false; return 0; }
    _bus->requestFrom(_addr, (uint8_t)1);
    if (!_bus->available()) { _ok = false; return 0; }
    uint8_t in = _bus->read();
    _ok = true;
    uint8_t cols = (in >> 4) ^ 0x0F;       // pressed column reads LOW -> 1
    if (cols) {
      for (uint8_t c = 0; c < 4; c++)
        if (cols & (1u << c)) return kMap[r][c];
    }
  }
  return 0;
}

char I2CKeypad::update() {
  uint32_t now = millis();
  if (_tScan != 0 && now - _tScan < 15) return 0;   // scan every 15 ms
  _tScan = now ? now : 1;

  char k = scan();

  if (k != _prev) {                        // raw state changed: restart timer
    _prev = k;
    _tEdge = now;
  } else if (k != 0 && k == _prev && !_fired &&
             now - _tEdge >= 15) {         // stable for 15 ms -> accept
    _fired = true;
    _lastKey = k;
    return k;
  }
  if (k == 0) _fired = false;              // released: re-arm
  return 0;
}

#else

I2CKeypad keypad;                          // stub instance (methods inline no-ops)

#endif

/* ==========================  src/display.cpp  ========================== */

#if DISPLAY_ENABLED
#include <SPI.h>

// ---------------------------------------------------------------------
//  ILI9488 480x320 SPI driver - no libraries, direct register commands
// ---------------------------------------------------------------------
namespace display {

static SPIClass spi(HSPI);                 // remapped pins, see config.h

// RGB565 palette (matches the website theme)
#define C_NAVY   0x0042
#define C_NAVY2  0x0145    // title bar
#define C_CARD   0x0142
#define C_GOLD   0xD566
#define C_BLUE   0x4E1F
#define C_GREEN  0x3693
#define C_RED    0xFB8E
#define C_WHITE  0xFFFF
#define C_DIM    0xADB9
#define C_BLACK  0x0000

static inline void cs(bool low) { digitalWrite(PIN_TFT_CS, low ? LOW : HIGH); }

static void cmd(uint8_t c) {
  cs(true); digitalWrite(PIN_TFT_DC, LOW);
  spi.transfer(c);
  cs(false);
}
static void data8(uint8_t d) {
  cs(true); digitalWrite(PIN_TFT_DC, HIGH);
  spi.transfer(d);
  cs(false);
}
static void cmdData(uint8_t c, uint8_t d) { cmd(c); data8(d); }

// 5x7 font (printable ASCII 0x20-0x7E), column bits, MSB = top row
static const uint8_t FONT5x7[95][5] PROGMEM = {
  {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
  {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
  {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
  {0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
  {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
  {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
  {0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
  {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
  {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
  {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
  {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
  {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},
  {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
  {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
  {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
  {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
  {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
  {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0x7C,0x14,0x14,0x14,0x08},
  {0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
  {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
  {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
  {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
  {0x00,0x41,0x36,0x08,0x00},{0x08,0x08,0x2A,0x1C,0x08}
};

static void setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  cmd(0x2A);                               // CASET
  data8(x >> 8); data8(x & 0xFF);
  data8((x + w - 1) >> 8); data8((x + w - 1) & 0xFF);
  cmd(0x2B);                               // PASET
  data8(y >> 8); data8(y & 0xFF);
  data8((y + h - 1) >> 8); data8((y + h - 1) & 0xFF);
  cmd(0x2C);                               // RAMWR
}

static void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c) {
  setWindow(x, y, w, h);
  cs(true); digitalWrite(PIN_TFT_DC, HIGH);
  uint32_t n = (uint32_t)w * h;
  while (n--) spi.transfer16(c);
  cs(false);
}

static void drawChar(uint16_t x, uint16_t y, char ch, uint8_t s, uint16_t fg, uint16_t bg) {
  if (ch < 0x20 || ch > 0x7E) ch = '?';
  const uint8_t *g = FONT5x7[(uint8_t)ch - 0x20];
  setWindow(x, y, (uint16_t)(5 * s), (uint16_t)(7 * s));
  cs(true); digitalWrite(PIN_TFT_DC, HIGH);
  for (uint8_t r = 0; r < 7; r++) {
    uint8_t bits = 0;
    for (uint8_t c = 0; c < 5; c++) if (pgm_read_byte(&g[c]) & (0x40 >> r)) bits |= 1 << c;
    for (uint8_t vs = 0; vs < s; vs++)
      for (uint8_t c = 0; c < 5; c++)
        for (uint8_t hs = 0; hs < s; hs++) spi.transfer16((bits & (1 << c)) ? fg : bg);
  }
  cs(false);
}

static void drawText(uint16_t x, uint16_t y, const char *str, uint8_t s, uint16_t fg, uint16_t bg) {
  while (*str) { drawChar(x, y, *str++, s, fg, bg); x += (uint16_t)(6 * s); }
}

static void bar(uint16_t x, uint16_t y, uint16_t w, uint8_t h, uint8_t pct, uint16_t c) {
  fillRect(x, y, w, h, C_CARD);
  fillRect(x + 1, y + 1, (uint16_t)((w - 2) * pct / 100), (uint8_t)(h - 2), c);
}

void begin() {
  pinMode(PIN_TFT_CS, OUTPUT);  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_TFT_DC, OUTPUT);
  pinMode(PIN_TFT_RST, OUTPUT); digitalWrite(PIN_TFT_RST, HIGH);
  delay(50);
  spi.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, -1);
  spi.beginTransaction(SPISettings(TFT_SPI_HZ, MSBFIRST, SPI_MODE0));

  digitalWrite(PIN_TFT_RST, LOW); delay(20);   // hardware reset
  digitalWrite(PIN_TFT_RST, HIGH); delay(150);
  cmd(0x01); delay(150);                       // SWRESET
  cmd(0x11); delay(50);                        // SLPOUT
  cmdData(0x3A, 0x55); delay(10);              // 16-bit colour
  cmdData(0x36, 0x48);                         // landscape + BGR
  cmd(0x21);                                   // INVON (typical 3.5" boards)
  cmd(0x29); delay(20);                        // DISPON
  drawSplash();                     // v2.0: boot = ARCHITECTS OF SOLUTIONS
  Serial.printf("[disp] ILI9488 480x320 up (SCK%d MOSI%d CS%d DC%d RST%d)\n",
                PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
}

// ---------------------------------------------------------------------
//  v2.0 screens: splash / menu / status - 1 Hz
// ---------------------------------------------------------------------
static bool sSplash = true;            // boot starts on the splash

void splash(bool on) { sSplash = on; }

static void fmtC(char *dst, size_t n, float v) {   // "--.-C" when NaN
  if (isnan(v)) snprintf(dst, n, "--.-C");
  else          snprintf(dst, n, "%.1fC", v);
}

// ---- boot splash: the brand, held for the 10 s initialisation window ----
static void drawSplash() {
  fillRect(0, 0, 480, 320, C_NAVY);
  drawText(8, 84, BRAND_NAME, 3, C_GOLD, C_NAVY);      // ARCHITECTS OF
  drawText(64, 140, "S O L U T I O N S", 3, C_GOLD, C_NAVY);
  fillRect(60, 184, 360, 2, C_DIM);
  drawText(96, 204, PRODUCT_NAME, 4, C_WHITE, C_NAVY); // SMART DEHUMIDIFIER
  char b[48];
  snprintf(b, sizeof(b), "v%s  -  initialising, outputs LOW", FW_VERSION);
  drawText(84, 262, b, 2, C_DIM, C_NAVY);
}

// ---- on-device menu (keypad-driven, see menu.cpp) -----------------------
static void drawMenu() {
  fillRect(0, 0, 480, 320, C_NAVY);
  fillRect(0, 0, 480, 34, C_NAVY2);
  drawText(8, 10, menu::editing() ? "ENTER VALUE" : "MENU", 2, C_GOLD, C_NAVY2);
  drawText(300, 10, "2/8 move  A ok  # back", 2, C_DIM, C_NAVY2);

  if (menu::info()) {                       // the OTA firmware-update screen
    drawText(16, 56, "FIRMWARE UPDATE", 4, C_GOLD, C_NAVY);
    drawText(16, 108, "phone/PC -> WiFi:", 2, C_WHITE, C_NAVY);
    drawText(16, 130, "  AgarbattiDryer / dryer1234", 2, C_WHITE, C_NAVY);
    drawText(16, 162, "browser (phone):", 2, C_WHITE, C_NAVY);
    drawText(16, 184, "  http://192.168.4.1", 2, C_GOLD, C_NAVY);
    drawText(16, 206, "  menu: Firmware update,", 2, C_WHITE, C_NAVY);
    drawText(16, 228, "  pick the .bin, Update", 2, C_WHITE, C_NAVY);
    drawText(16, 260, "Arduino IDE (PC):", 2, C_WHITE, C_NAVY);
    drawText(16, 282, "  Port -> smart-dehumidifier", 2, C_GOLD, C_NAVY);
    drawText(16, 302, "  at 192.168.4.1", 2, C_GOLD, C_NAVY);
    drawText(400, 302, "# back", 1, C_DIM, C_NAVY);
    return;
  }
  if (menu::editing()) {
    drawText(16, 80, menu::itemName(menu::cursor()), 3, C_WHITE, C_NAVY);
    char b[32];
    snprintf(b, sizeof(b), "now: %s", menu::itemValue(menu::cursor()));
    drawText(16, 130, b, 3, C_GOLD, C_NAVY);
    snprintf(b, sizeof(b), "new: %s_", menu::editBuffer());
    drawText(16, 180, b, 4, C_GREEN, C_NAVY);
    drawText(16, 250, menu::editHint(), 2, C_DIM, C_NAVY);
    drawText(16, 276, "type digits, A = save, # = cancel", 2, C_DIM, C_NAVY);
    return;
  }
  uint16_t y = 60;
  for (uint8_t i = 0; i < menu::itemCount(); i++) {
    bool sel = (i == menu::cursor());
    if (sel) fillRect(0, y - 6, 480, 30, C_CARD);
    drawText(24, y, menu::itemName(i), 3, sel ? C_GOLD : C_WHITE,
             sel ? C_CARD : C_NAVY);
    drawText(280, y, menu::itemValue(i), 3, sel ? C_GOLD : C_DIM,
             sel ? C_CARD : C_NAVY);
    y += 40;
  }
}

// ---- compact temperature spark-line (cycle log) ---------------------------
static void drawGraph(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  fillRect(x, y, w, h, C_CARD);
  uint16_t n = dryer.logCount();
  if (n < 2) { drawText(x + 4, y + h / 2 - 7, "graph: start a cycle", 1, C_DIM, C_CARD); return; }
  int16_t lo = 3000, hi = -3000;
  for (uint16_t i = 0; i < n; i++) {          // scale to the observed range
    int16_t v = dryer.logAt(i).tAvg10;
    if (v < lo) lo = v;  if (v > hi) hi = v;
  }
  if (hi - lo < 20) { lo -= 10; hi += 10; }   // avoid a flat zero-range
  uint16_t step = n > w ? n / w : 1;
  for (uint16_t px = 0; px < w && px * step < n; px++) {
    int16_t v = dryer.logAt(px * step).tAvg10;
    uint16_t yy = y + h - 2 - (uint16_t)((long)(v - lo) * (h - 4) / (hi - lo));
    if (yy < y + 1) yy = y + 1;
    if (yy > y + h - 2) yy = y + h - 2;
    fillRect(x + px, yy, 1, 2, C_GOLD);       // 2 px thick point
  }
}

void update() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (last != 0 && now - last < 1000) return;
  last = now ? now : 1;

  if (sSplash)  { drawSplash(); return; }
  if (menu::active()) { drawMenu(); return; }

  char buf[52], b1[10], b2[10];
  float t1 = sensors.t1(), t2 = sensors.t2();
  float tA = sensors.tAvg(), hA = sensors.hAvg();
  DState st = dryer.state();

  fillRect(0, 0, 480, 320, C_NAVY);

  // title bar: product + supply mode + parameter mode
  fillRect(0, 0, 480, 34, C_NAVY2);
  snprintf(buf, sizeof(buf), "%s v%s", PRODUCT_NAME, FW_VERSION);
  drawText(8, 10, buf, 2, C_GOLD, C_NAVY2);
  drawText(230, 10, supply::modeName(), 2,
           supply::optoLive() ? C_GREEN : C_RED, C_NAVY2);
  snprintf(buf, sizeof(buf), "%s",
           cfg.mode == 0 ? "AGARBATTI" : cfg.mode == 2 ? "SILICAGEL" : "USER");
  drawText(400, 10, buf, 2, C_WHITE, C_NAVY2);

  // state + times (elapsed since start, remaining of the set time)
  uint16_t stCol = (st == DState::FAULT) ? C_RED :
                   (st == DState::RUNNING) ? C_GREEN :
                   (st == DState::DONE) ? C_BLUE : C_DIM;
  drawText(16, 48, stateName(st), 4, stCol, C_NAVY);
  if (st == DState::RUNNING || st == DState::COOLDOWN) {
    uint32_t el = dryer.elapsedS(), rem = dryer.remainingS();
    snprintf(buf, sizeof(buf), "ELAPSED %lu:%02lu:%02lu  LEFT %lu:%02lu",
             (unsigned long)(el / 3600), (unsigned long)((el / 60) % 60),
             (unsigned long)(el % 60),
             (unsigned long)(rem / 60), (unsigned long)(rem % 60));
    drawText(16, 92, buf, 2, C_WHITE, C_NAVY);
    if (dryer.boosting()) drawText(400, 48, "MAX", 3, C_GOLD, C_NAVY);
  }

  // left column: climate + external temperature
  uint16_t y = 124;
  drawText(16, y, "T1", 2, C_DIM, C_NAVY);
  fmtC(b1, sizeof(b1), t1);  drawText(52, y, b1, 2, C_GOLD, C_NAVY);
  drawText(16, y + 26, "T2", 2, C_DIM, C_NAVY);
  fmtC(b2, sizeof(b2), t2);  drawText(52, y + 26, b2, 2, C_GOLD, C_NAVY);
  if (isnan(tA)) snprintf(buf, sizeof(buf), "AVG --.-C  RH --%%");
  else           snprintf(buf, sizeof(buf), "AVG %.1fC  RH %.0f%%",
                         tA, isnan(hA) ? 0.0f : hA);
  drawText(16, y + 52, buf, 2, C_WHITE, C_NAVY);
  snprintf(buf, sizeof(buf), "SET %.0fC  RH>%.0f%% FAN 1min", cfg.setTemp, cfg.humHigh);
  drawText(16, y + 78, buf, 2, C_DIM, C_NAVY);
  { // external temperature (phone-relayed weather / forecast)
    float oT, oH;
    if (getOutdoor(oT, oH))
      snprintf(buf, sizeof(buf), "OUT %.0fC %.0f%%", oT, oH);
    else
      snprintf(buf, sizeof(buf), "OUT -- (no DHT / phone)");
    drawText(16, y + 104, buf, 2, C_BLUE, C_NAVY);
  }

  // right column: power
  uint16_t x2 = 260;
  snprintf(buf, sizeof(buf), "BAT %.2fV %u%%", battery.volts(), battery.percent());
  drawText(x2, y, buf, 2, battery.valid() ? C_WHITE : C_DIM, C_NAVY);
  bar(x2, y + 24, 210, 12, battery.percent(), C_GREEN);
  drawText(x2, y + 44, "HEAT", 2, C_DIM, C_NAVY);
  snprintf(buf, sizeof(buf), "%u%%", dryer.heatDuty());
  drawText(x2 + 60, y + 44, buf, 2, C_GOLD, C_NAVY);
  bar(x2, y + 68, 210, 12, dryer.heatDuty(), C_GOLD);
  drawText(x2, y + 88, "FAN", 2, C_DIM, C_NAVY);
  const char *fanSt = dryer.fanOutDuty() ? "ON " : "OFF";   // no tach: ON/OFF
  snprintf(buf, sizeof(buf), "%s %u%%", fanSt, dryer.fanOutDuty());
  drawText(x2 + 60, y + 88, buf, 2, C_BLUE, C_NAVY);
  bar(x2, y + 112, 210, 12, dryer.fanOutDuty(), C_BLUE);

  // weight line: current / target / difference (spec v2.0)
  {
    char wb[52];
    if (scale.ok() && !isnan(scale.grams())) {
      float cur = scale.grams();
      if (cfg.targetG > 0)
        snprintf(wb, sizeof(wb), "WT %.0fg  TGT %.0fg  DIFF %+.0fg",
                 (double)cur, (double)cfg.targetG, (double)(cur - cfg.targetG));
      else
        snprintf(wb, sizeof(wb), "WT %.0fg (rate %.1f g/min)",
                 (double)cur, (double)(scale.ok() ? scale.rate() : 0.0f));
    } else {
      snprintf(wb, sizeof(wb), "WT -- (no scale)");
    }
    drawText(16, 232, wb, 2, C_WHITE, C_NAVY);
    if (st == DState::DONE && dryer.suggestMin() > 0) {  // unreachable target
      snprintf(buf, sizeof(buf), "target was +%.0f min away", (double)dryer.suggestMin());
      drawText(16, 254, buf, 2, C_RED, C_NAVY);
    }
  }

  // door / workflow line
  {
    const char *ph = door::phase();
    char db[52];
    if (!door::calibrated())
      snprintf(db, sizeof(db), "START LOCKED - CALIBRATE THE SCALE");
    else if (strcmp(ph, "READY") == 0)
      snprintf(db, sizeof(db), "DOOR %s - BATCH %.0f g - %s START",
               door::closed() ? "CLOSED" : "OPEN", (double)door::batchG(),
               door::locked() ? "UNLOCK," : "PRESS");
    else
      snprintf(db, sizeof(db), "DOOR %s - %s",
               door::closed() ? "CLOSED" : "OPEN", ph);
    drawText(16, 210, db, 2, C_GOLD, C_NAVY);
  }

  // cycle graph (spark-line) + footer
  drawGraph(16, 258, 224, 54);
  const char *why = dryer.faultWhy();
  if (why && why[0]) {
    drawText(252, 266, why, 2, C_RED, C_NAVY);
  } else {
    drawText(252, 266, "WiFi AgarbattiDryer", 2, C_DIM, C_NAVY);
    drawText(252, 288, "192.168.4.1", 2, C_DIM, C_NAVY);
  }
}

}  // namespace display
#else
// DISPLAY_ENABLED 0: nothing to build
#endif
/* ==========================  src/scale.cpp  ========================== */

#if SCALE_ENABLED

LoadScale scale;

void LoadScale::begin() {
  pinMode(PIN_SCALE_DOUT, INPUT);
  pinMode(PIN_SCALE_CLK,  OUTPUT);
  digitalWrite(PIN_SCALE_CLK, LOW);       // HX711 active (high > 60 us = power down)
  delay(1);
  _ok = (digitalRead(PIN_SCALE_DOUT) == LOW);   // data already pending = chip present
  Serial.printf("[scale] HX711 %s (CLK%d DOUT%d)\n",
                _ok ? "found" : "not responding (optional)",
                PIN_SCALE_CLK, PIN_SCALE_DOUT);
}

// one conversion: DOUT must be LOW (data ready), then 25 clock pulses.
// 24 data bits MSB-first on the falling edges, 25th sets gain 128 again.
bool LoadScale::readRaw(int32_t &v) {
  uint32_t t0 = millis();
  while (digitalRead(PIN_SCALE_DOUT) == HIGH) {
    if (millis() - t0 > 40) return false;      // no chip / not ready
  }
  int32_t d = 0;
  for (int i = 0; i < 24; i++) {
    digitalWrite(PIN_SCALE_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_SCALE_CLK, LOW);
    delayMicroseconds(1);
    d = (d << 1) | (digitalRead(PIN_SCALE_DOUT) ? 1 : 0);
  }
  digitalWrite(PIN_SCALE_CLK, HIGH);           // 25th pulse: next read ch A / x128
  delayMicroseconds(1);
  digitalWrite(PIN_SCALE_CLK, LOW);
  if (d & 0x800000) d |= (int32_t)0xFF000000;  // 24-bit two's complement
  v = d;
  return true;
}

void LoadScale::update() {
  uint32_t now = millis();
  if (_lastTry != 0 && now - _lastTry < 500) return;    // 2 Hz
  _lastTry = now ? now : 1;

  int32_t v;
  if (!readRaw(v)) {                                    // miss
    if (_ok && now - _lastOk > 10000) {                 // silent > 10 s
      _ok = false;
      Serial.println("[warn] weight scale stopped responding (optional)");
    }
    return;
  }
  bool wasOk = _ok;
  _ok = true; _lastOk = now; _raw = v;
  if (!wasOk) Serial.println("[warn] weight scale OK again");

  _g = ((float)(_raw - _offset)) / _factor;

  // rate over the rolling 5-minute window (sample every 30 s)
  static uint32_t lastSample = 0;
  if (lastSample == 0 || now - lastSample >= 30000) {
    lastSample = now;
    _hG[_hIdx] = _g; _hT[_hIdx] = now;
    _hIdx = (_hIdx + 1) % 10;
    if (_hN < 10) _hN++;
    if (_hN >= 3) {                                     // need >= 1 min span
      uint8_t newest = (_hIdx + 10 - 1) % 10;
      uint8_t oldest = _hIdx;                           // next write slot = oldest
      float dtMin = (_hT[newest] - _hT[oldest]) / 60000.0f;
      if (dtMin >= 0.9f) _rate = (_hG[newest] - _hG[oldest]) / dtMin;
    }
  }
}

void LoadScale::tare() {
  if (!_ok) return;
  _offset = _raw;                        // zero at the current raw reading
  _g = 0.0f;
  _hN = 0; _hIdx = 0; _rate = NAN;       // restart the rate window
  Serial.printf("[scale] tared (raw offset %ld)\n", (long)_offset);
}

void LoadScale::calibrate(float knownGrams) {
  if (!_ok || knownGrams <= 0) return;
  float f = (float)(_raw - _offset) / knownGrams;
  if (f > 0.05f && f < 200000.0f) {
    _factor = f;
    Serial.printf("[scale] calibrated: %.1f units/g (known %.0f g)\n",
                  (double)f, (double)knownGrams);
  } else {
    Serial.println("[scale] calibration rejected - put the weight on first");
  }
}

#else

LoadScale scale;      // stub instance (methods are inline no-ops)

#endif
/* ==========================  src/door.cpp  ========================== */
#include <Preferences.h>

// Phases: CALIBRATE (locked) -> LOAD (unlocked) -> READY (batch weighed)
//         -> RUNNING (locked) -> back to LOAD after DONE/FAULT.
enum class DPhase { CALIBRATE, LOAD, READY, RUNNING };
static DPhase sPhase = DPhase::CALIBRATE;
static bool  sCalOK  = false;          // scale calibrated at least once
static bool  sLocked = false;          // lock output state
static float sBatch  = 0.0f;           // measured batch grams
static bool  sWasClosed = true;
static uint32_t sStableSince = 0;      // scale-stable window for weighing
static float   sStableG = 0.0f;
static bool  sUnlockLogged = false;

namespace door {

bool fitted()     { return DOOR_ENABLED != 0; }
bool locked()     { return sLocked; }
bool calibrated() { return sCalOK; }
float batchG()    { return sBatch; }

bool closed() {
#if DOOR_ENABLED
  return digitalRead(PIN_DOOR_REED) == DOOR_CLOSED_LEVEL;
#else
  return true;                         // no sensor: assume closed
#endif
}

const char *phase() {
  switch (sPhase) {
    case DPhase::CALIBRATE: return "CALIBRATE";
    case DPhase::LOAD:      return "LOAD";
    case DPhase::READY:     return "READY";
    case DPhase::RUNNING:   return "RUNNING";
  }
  return "?";
}

static void driveLock(bool on) {
  if (on == sLocked) return;
  sLocked = on;
#if DOOR_ENABLED && DOOR_LOCK_ENABLED
  digitalWrite(PIN_DOOR_LOCK, on ? DOOR_LOCK_ACTIVE : !DOOR_LOCK_ACTIVE);
#endif
  // without a physical lock this is the SOFTWARE gate: start refused
  Serial.printf("[door] %s\n", on ? "START LOCKED" : "start unlocked");
}

void engage() { driveLock(!sCalOK); }   // called after the init window

void markCalibrated() {
  if (sCalOK) return;
  sCalOK = true;
  Preferences p;
  p.begin("dryer", false);
  p.putBool("scalOK", true);
  p.end();
  Serial.println(F("[door] scale calibrated - workflow unlocked"));
}

void serviceUnlock() {                 // serial service override
  sCalOK = true;
  driveLock(false);
  Serial.println(F("[door] SERVICE unlock"));
}

void begin(bool engageLock) {
#if DOOR_ENABLED
#if DOOR_LOCK_ENABLED
  pinMode(PIN_DOOR_LOCK, OUTPUT);
#endif
  pinMode(PIN_DOOR_REED, INPUT_PULLUP);
#endif
  Preferences p;
  p.begin("dryer", true);
  sCalOK = p.getBool("scalOK", false);
  p.end();
  sWasClosed = closed();
  if (sCalOK) sPhase = DPhase::LOAD;   // calibrated on an earlier boot
  // spec v2.0: during the 10 s power-on window every pin stays LOW -
  // the lock engages only when the window ends (engage())
  driveLock(engageLock && !sCalOK);
  Serial.printf("[door] %s%s - %s\n",
                fitted() ? "lock+reed fitted, " : "software workflow (no lock pins), ",
                sCalOK ? "scale calibrated" : "scale NOT calibrated",
                sCalOK ? "load the trays" : "door LOCKED until calibration");
}

void update() {
  DState st = dryer.state();

  // ---- phase machine --------------------------------------------------
  if (st == DState::RUNNING) {
    if (sPhase != DPhase::RUNNING) {
      sPhase = DPhase::RUNNING;
      driveLock(true);                 // keep it shut while hot
    }
  } else if (st == DState::COOLDOWN) {
    driveLock(true);                   // still hot - stay locked
  } else {                             // IDLE / DONE / FAULT
    if (sPhase == DPhase::RUNNING) {   // cycle just ended
      sPhase = DPhase::LOAD;
      sBatch = 0.0f;
      driveLock(sCalOK ? false : true);
      if (sCalOK) Serial.println(F("[door] unlocked - unload / load the next batch"));
    }
    if (!sCalOK) {                     // the whole point: no cal, no door
      sPhase = DPhase::CALIBRATE;
      driveLock(true);
      return;
    }
    if (sPhase == DPhase::CALIBRATE) {
      sPhase = DPhase::LOAD;
      driveLock(false);
      if (!sUnlockLogged) {
        Serial.println(F("[door] unlocked - load the trays, then close the door"));
        sUnlockLogged = true;
      }
    }
  }

  // ---- door closed + stable scale = batch weight (the "diff") ---------
  bool nowClosed = closed();
  if (nowClosed && !sWasClosed) {         // a close ends these nags
    bz::stopRepeat(BP::DOOR_AJAR);
    bz::stopRepeat(BP::DOOR_OPEN_RUN);
    bz::stopRepeat(BP::UNSTABLE);
  }
  if (nowClosed && !sWasClosed && sPhase == DPhase::LOAD &&
      scale.ok() && !isnan(scale.grams())) {
    if (sStableSince == 0) { sStableSince = millis(); sStableG = scale.grams(); }
    // weigh when the reading sits within 20 g for ~4 s
    if (millis() - sStableSince >= 4000) {
      if (fabsf(scale.grams() - sStableG) <= 20.0f) {
        sBatch = scale.grams();        // tare was captured at calibration
        sPhase = DPhase::READY;
        sStableSince = 0;
        bz::play(BP::READY);            // #11: weighed - ready to start
        if (sBatch < 50.0f)
          Serial.printf("[load] door closed - trays look EMPTY (%.0f g)\n", (double)sBatch);
        else if (sBatch > 9000.0f)
          Serial.printf("[load] door closed - OVERLOAD %.0f g (cells are 10 kg) - remove some\n", (double)sBatch);
        else
          Serial.printf("[load] door closed - batch %.0f g loaded - press Start (or knob/serial)\n", (double)sBatch);
      } else {
        sStableSince = millis(); sStableG = scale.grams();  // still settling
      }
    }
  } else if (nowClosed && sPhase == DPhase::LOAD && sStableSince != 0 &&
             millis() - sStableSince > 6000UL) {
    // #9: weight still moving ~6 s after the close - keep nudging
    bz::startRepeat(BP::UNSTABLE, 3000);
    sStableSince = millis();            // re-arm the window
  } else if (!nowClosed && sPhase == DPhase::READY) {
    sPhase = DPhase::LOAD;             // reopened: allow the next weighing
    sStableSince = 0;
  } else if (nowClosed != sWasClosed) {
    sStableSince = 0;
  }
  if (nowClosed != sWasClosed) bz::door();     // 1 s beep: door moved (spec)
  sWasClosed = nowClosed;

  // ---- door opened mid-cycle = FAULT ----------------------------------
#if DOOR_ENABLED
  if ((st == DState::RUNNING || st == DState::COOLDOWN) && !nowClosed) {
    dryer.faultNowE(7, "door opened during the cycle");   // E07 (spec)
    bz::startRepeat(BP::DOOR_OPEN_RUN, 150);  // #16: rapid until closed
  }
#endif
}

}  // namespace door
/* ==========================  src/supply.cpp  ========================== */

namespace supply {

// ---- state -------------------------------------------------------------
static bool  sHoldUp    = false;      // our latch keep-alive asserted
static bool  sSolar     = true;       // requested mode (toggle)
static bool  sRelayAuto = SUPPLY_RELAYS_ENABLED != 0;
static bool  sBtn1Was   = false;      // BUTTON-1 edge tracking
static uint32_t sBtn1T  = 0;          // press start
static bool  sBtn2Was   = false;
static bool  sErrLatched = false;     // one error beep per event

bool latched()    { return PIN_POWER_HOLD >= 0; }
bool relayAuto()  { return sRelayAuto; }
bool solarRequested() { return sSolar; }

bool optoLive() {
  if (PIN_SUPPLY_OPTO < 0) return true;         // no detector: assume ok
  return digitalRead(PIN_SUPPLY_OPTO) == (OPTO_ACTIVE_HIGH ? HIGH : LOW);
}

const char *modeName() { return sSolar ? "SOLAR MODE" : "BYPASS MODE"; }

// ---- 2-channel feed relay ----------------------------------------------
static void driveRelay(bool solar) {
#if SUPPLY_RELAYS_ENABLED
  // one channel ON at a time, the other always OFF (never both feeds)
  digitalWrite(PIN_SUPPLY_CH1, solar ? HIGH : LOW);
  digitalWrite(PIN_SUPPLY_CH2, solar ? LOW  : HIGH);
#else
  (void)solar;
#endif
}

static void driveAllOff() {                 // both feed channels LOW
#if SUPPLY_RELAYS_ENABLED
  digitalWrite(PIN_SUPPLY_CH1, LOW);
  digitalWrite(PIN_SUPPLY_CH2, LOW);
#endif
}

void engage() { driveRelay(sSolar); }        // after the 10 s init window

bool loadsQuiet() {
  DState st = dryer.state();
  return st != DState::RUNNING && st != DState::COOLDOWN &&
         dryer.heatDuty() == 0 && dryer.fanOutDuty() == 0;
}

// ---- hard power-off ------------------------------------------------------
void powerOff() {
  Serial.println(F("[power] BUTTON-1 3 s -> HARD POWER OFF"));
  dryer.stop();                                  // loads off, purge skipped:
  bz::error();                                   // the pillar is dying anyway
  delay(300);                                    // let the beep + serial flush
  if (PIN_POWER_HOLD >= 0) digitalWrite(PIN_POWER_HOLD, LOW);
  sHoldUp = false;
  delay(2000);                                   // latch falls, MCU starves
  // still alive? the latch failed - say so and carry on
  Serial.println(F("[power] latch did not release - check the P-MOSFET circuit"));
  if (PIN_POWER_HOLD >= 0) digitalWrite(PIN_POWER_HOLD, HIGH);
}

// ---- mode switch ----------------------------------------------------------
void requestSwitch() {                          // follow the toggle if safe
  bool wantSolar = solarRequested();
  if (wantSolar == sSolar) return;
  if (!loadsQuiet()) {
    if (!sErrLatched) {
      Serial.println(F("[supply] REFUSED: mode change while a supply is "
                       "under load - stop the cycle first"));
      dryer.warnE(2, "change-over refused under load");      // E02 (spec)
      bz::error();                              // 5 s error beep (spec)
      sErrLatched = true;
    }
    return;                                     // toggle stays pending
  }
  sSolar = wantSolar;
  sErrLatched = false;
  driveRelay(sSolar);
  Serial.printf("[supply] switched to %s\n", modeName());
  bz::modeChange();                             // 3 s continuous (spec)
}

// ---- begin -----------------------------------------------------------------
void begin() {
  // 1. keep ourselves alive - the soft-latch button may already be released
  if (PIN_POWER_HOLD >= 0) {
    pinMode(PIN_POWER_HOLD, OUTPUT);
    digitalWrite(PIN_POWER_HOLD, HIGH);         // assert within ms of boot
    sHoldUp = true;
  }
  // 2. inputs
  if (PIN_BTN1 >= 0) pinMode(PIN_BTN1, INPUT_PULLUP);
  if (PIN_BTN2 >= 0) pinMode(PIN_BTN2, INPUT_PULLUP);
  if (PIN_SOLAR_TOGGLE >= 0) pinMode(PIN_SOLAR_TOGGLE, INPUT_PULLUP);
  if (PIN_SUPPLY_OPTO >= 0) pinMode(PIN_SUPPLY_OPTO, INPUT);
  // 3. relay follows the toggle immediately (loads are off at boot)
  if (PIN_SOLAR_TOGGLE >= 0) sSolar = digitalRead(PIN_SOLAR_TOGGLE) == HIGH;
#if SUPPLY_RELAYS_ENABLED
  pinMode(PIN_SUPPLY_CH1, OUTPUT);
  pinMode(PIN_SUPPLY_CH2, OUTPUT);
#endif
  driveAllOff();                 // spec: every pin LOW for the init window
  Serial.printf("[supply] %s (toggle), opto %s, relay %s\n",
                modeName(),
                PIN_SUPPLY_OPTO >= 0 ? "fitted" : "absent",
                sRelayAuto ? "auto" : "manual");
}

// ---- update ------------------------------------------------------------------
void update() {
  uint32_t now = millis();

  // BUTTON-1: 3 s = hard power off, 10 s = reboot
  if (PIN_BTN1 >= 0) {
    static uint32_t sLastTick = 0;
    bool p = digitalRead(PIN_BTN1) == LOW;      // active low (to GND)
    if (p && !sBtn1Was) { sBtn1T = now; sLastTick = 0; }
    if (p && sBtn1Was) {
      // #3: live tick every 500 ms so the operator holds long enough
      if (now - sBtn1T > 500 && now - sLastTick >= 500) {
        bz::play(BP::TICK);
        sLastTick = now;
      }
      if (now - sBtn1T >= BTN1_RESET_MS) {      // 10 s: reboot
        Serial.println(F("[power] BUTTON-1 10 s -> REBOOT"));
        bz::play(BP::FACT_RESET);               // #37: sweep + 3 beeps
        delay(2100);                            // let it sound before dying
        ESP.restart();
      } else if (now - sBtn1T >= BTN1_OFF_MS) { // 3 s: hard off
        bz::play(BP::SHUTDOWN);                 // #36: descending confirm
        delay(1200);
        powerOff();                             // may not return
        sBtn1T = now;                           // re-arm if latch failed
      }
    }
    sBtn1Was = p;
  }

  // BUTTON-2: default automation - agarbatti preset + start (door-gated)
  if (PIN_BTN2 >= 0) {
    bool p = digitalRead(PIN_BTN2) == LOW;
    if (p && !sBtn2Was) {
      Serial.println(F("[btn2] default automation: AGARBATTI preset + start"));
      dryer.applyMode(0);
      dryer.start();
    }
    sBtn2Was = p;
  }

  // toggle / opto / relay
  if (PIN_SOLAR_TOGGLE >= 0) {
    bool wantSolar = digitalRead(PIN_SOLAR_TOGGLE) == HIGH;
    if (wantSolar != sSolar) requestSwitch();
  }

  // opto verification: selected feed should be live
  static uint32_t optoWarnT = 0;
  if (PIN_SUPPLY_OPTO >= 0 && !optoLive() && optoWarnT != 0 &&
      now - optoWarnT > 60000UL) {
    Serial.printf("[warn] %s but the feed reads DEAD - check the supply\n",
                  modeName());
    dryer.warnE(11, "selected feed reads DEAD");            // E11 (spec)
    bz::error();
    optoWarnT = now ? now : 1;
  } else if (PIN_SUPPLY_OPTO >= 0 && optoLive()) {
    optoWarnT = now ? now : 1;                  // feed healthy: re-arm
  } else if (PIN_SUPPLY_OPTO >= 0 && optoWarnT == 0) {
    optoWarnT = now ? now : 1;
  }
}

}  // namespace supply
/* ==========================  src/menu.cpp  ========================== */
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
/* ==========================  src/txdisp.cpp  ========================== */

#if TXDISP_ENABLED


// TX-only UART to the companion Arduino (RX pin -1 = transmit only)
#define BRIDGE Serial1

namespace txdisp {

void begin() {
  BRIDGE.begin(TXDISP_BAUD, SERIAL_8N1, -1, PIN_TXDISP_TX);
  Serial.printf("[txdisp] display bridge up: TX GPIO%d @ %u baud -> Arduino\n",
                PIN_TXDISP_TX, (unsigned)TXDISP_BAUD);
}

void update() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (last != 0 && now - last < 1000) return;   // one burst per second
  last = now ? now : 1;

  const char *st  = stateName(dryer.state());
  const char *why = dryer.faultWhy();
  float g = (scale.ok() && !isnan(scale.grams())) ? scale.grams() : 0.0f;

  BRIDGE.printf("$ST,%s\n", st);
  BRIDGE.printf("$T,%.1f,%.1f\n", (double)sensors.t1(), (double)sensors.t2());
  BRIDGE.printf("$H,%.0f,%.0f\n", (double)sensors.h1(), (double)sensors.h2());
  BRIDGE.printf("$SET,%.0f,%u\n", (double)cfg.setTemp, (unsigned)cfg.dryMinutes);
  BRIDGE.printf("$E,%lu,%lu\n",
                (unsigned long)dryer.elapsedS(), (unsigned long)dryer.remainingS());
  BRIDGE.printf("$P,%u,%u,%.2f,%u\n", dryer.heatDuty(), dryer.fanOutDuty(),
                (double)battery.volts(), battery.percent());
  BRIDGE.printf("$W,%.0f,%.0f,%.0f\n", (double)g, (double)cfg.targetG,
                (double)(g - cfg.targetG));
  BRIDGE.printf("$D,%s,%d,%.0f\n", door::phase(), door::closed() ? 1 : 0,
                (double)door::batchG());
  BRIDGE.printf("$S,%s,%d,%u\n", supply::modeName(),
                supply::optoLive() ? 1 : 0, (unsigned)cfg.mode);
  BRIDGE.printf("$F,%s\n", (why && why[0]) ? why : "");
  BRIDGE.printf("$V,%s\n", FW_VERSION);
}

}  // namespace txdisp

#else

namespace txdisp {
void begin() {}
void update() {}
}

#endif
/* ==========================  src/dht.cpp  ========================== */

namespace dht {

// ---- one 40-bit frame (blocking ~5 ms, timeout-bounded) ----------------
static bool frame(int8_t pin, bool is11, float &t, float &h) {
  uint8_t d[5] = {0, 0, 0, 0, 0};

  // start: pull low 2 ms, release, wait for the sensor's 80/80 us reply
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(2);
  digitalWrite(pin, HIGH);
  pinMode(pin, INPUT);              // external 10k pulls the line up
  delayMicroseconds(30);

  uint32_t t0 = micros();
  while (digitalRead(pin) == LOW)  if (micros() - t0 > 100) return false;
  t0 = micros();
  while (digitalRead(pin) == HIGH) if (micros() - t0 > 100) return false;

  // 40 bits: 50 us low then a 26-70 us high; long high = 1
  for (uint8_t i = 0; i < 40; i++) {
    t0 = micros();
    while (digitalRead(pin) == LOW)  if (micros() - t0 > 80)  return false;
    t0 = micros();
    while (digitalRead(pin) == HIGH) if (micros() - t0 > 100) return false;
    d[i / 8] <<= 1;
    if (micros() - t0 > 48) d[i / 8] |= 1;   // high was long -> bit 1
  }

  if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return false;  // checksum

  if (is11) {                       // DHT11: integer bytes + decimal byte
    h = (float)d[0] + d[1] * 0.1f;
    t = (float)d[2] + d[3] * 0.1f;
  } else {                          // DHT22/AM2302: tenths, sign bit
    h = (float)(((uint16_t)d[0] << 8) | d[1]) * 0.1f;
    float raw = (float)(((uint16_t)(d[2] & 0x7F) << 8) | d[3]) * 0.1f;
    t = (d[2] & 0x80) ? -raw : raw;
  }
  return true;
}

void Dev::update() {
  if (pin < 0) return;              // not fitted on this variant
  uint32_t now = millis();
  if (last != 0 && now - last < everyMs) return;
  last = now ? now : 1;

  float t, h;
  if (frame(pin, is11, t, h)) {
    this->t = t;  this->h = h;  miss = 0;  ok = true;  gen++;
  } else if (++miss >= 3) {         // absent/unplugged after 3 tries
    if (ok) Serial.printf("[warn] DHT on GPIO%d stopped responding\n", pin);
    ok = false;                     // keep the last values for display
  }
}

static Dev mk(int8_t pin, bool is11, uint32_t every) {
  Dev d;  d.pin = pin;  d.is11 = is11;  d.everyMs = every;  return d;
}

#if DHT_CHAMBER_ENABLED
  Dev chamber = mk(PIN_DHT22_CHAMBER, false, DHT_CHAMBER_MS);  // DHT22
#else
  Dev chamber;                                                  // off
#endif
#if DHT_OUT_ENABLED
  Dev outdoor = mk(PIN_DHT11_OUT, true, DHT_READ_MS);          // DHT11
#else
  Dev outdoor;                                                  // off
#endif

void begin() {
  if (chamber.pin >= 0) {
    pinMode(chamber.pin, INPUT);
    Serial.printf("[dht] DHT22 chamber sensor on GPIO%d (cool return)\n",
                  chamber.pin);
  }
  if (outdoor.pin >= 0) {
    pinMode(outdoor.pin, INPUT);
    Serial.printf("[dht] DHT11 outdoor sensor on GPIO%d (shade!)\n",
                  outdoor.pin);
  }
}

void update() { chamber.update(); outdoor.update(); }

}  // namespace dht
/* ==========================  src/pixel.cpp  ========================== */

#if PIXEL_ENABLED
#include <SPI.h>

namespace pixel {

static SPIClass spi(FSPI);          // the display is gone - SPI is free
static int      sPin = PIN_PIXEL;   // v2.0.21: re-pinnable at runtime (48/38)
static uint8_t  lastG = 255, lastR = 255, lastB = 255;

static void write(uint8_t g, uint8_t r, uint8_t b) {   // GRB order
  if (g == lastG && r == lastR && b == lastB) return;  // unchanged
  lastG = g;  lastR = r;  lastB = b;
  uint8_t buf[24];
  uint8_t grb[3] = {g, r, b};
  for (uint8_t i = 0; i < 24; i++)
    buf[i] = (grb[i / 8] >> (7 - (i % 8))) & 1 ? 0b11111000 : 0b11000000;
  spi.beginTransaction(SPISettings(6400000, MSBFIRST, SPI_MODE0));
  for (uint8_t n = 0; n < PIXEL_COUNT; n++) spi.writeBytes(buf, 24);
  spi.endTransaction();
  delayMicroseconds(60);                               // reset latch
}

void begin() {
  spi.begin(-1, -1, sPin, -1);                         // MOSI only
  write(0, 0, 0);
  Serial.printf("[pixel] RGB status pixel on GPIO%d (x%u) - "
                "v1.0 devkits: 48, v1.1: 38 (serial 'pixel 48|38' to switch)\n",
                sPin, (unsigned)PIXEL_COUNT);
}

// v2.0.21: the on-board pixel is GPIO48 on DevKitC v1.0 boards and
// GPIO38 on v1.1 boards (check the silkscreen). This moves the driver
// to the other candidate at runtime - the 300 ms white flash shows
// which physical LED just lit, so an external strip wired to either
// pin is driven identically. Only 48/38 are allowed (both safe as
// plain outputs; nothing else in the build uses them).
bool rePin(int pin) {
  if (pin != 48 && pin != 38) return false;
  sPin = pin;
  spi.end();
  spi.begin(-1, -1, sPin, -1);
  write(0, 0, 0);
  write(0, 255, 255);                                  // white = "that's the one"
  delay(300);
  write(0, 0, 0);
  Serial.printf("[pixel] moved to GPIO%d (x%u)\n", sPin, (unsigned)PIXEL_COUNT);
  return true;
}

void update() {
  uint32_t ms = millis();
  bool blink2 = (ms / 250) & 1;          // 2 Hz
  bool blink1 = (ms / 500) & 1;          // 1 Hz
  uint32_t ph = ms % 3000;
  uint8_t breathe = 20 + (uint8_t)(80 *
      (ph < 1500 ? ph : 3000 - ph) / 1500);            // 0..100 triangle

  if (ms < 3000)                 { write(0, 0, 60); return; }   // boot blue

  DState st = dryer.state();
  switch (st) {
    case DState::FAULT:   if (blink2) write(0, 120, 0);  else write(0, 0, 0);   return;
    case DState::DONE:    write(120, 0, 0);             return;   // batch ready
    case DState::COOLDOWN:if (blink1) write(60, 0, 60);  else write(0, 0, 0);   return;
    case DState::RUNNING:
      if (battery.percent() < 20) {                     // low battery
        if (blink1) write(60, 120, 0); else write(0, 0, 0);
      } else if (dryer.warnActive()) {                  // DEGRADED (v2.0.17):
        if (blink1) write(80, 80, 0); else write(0, 0, 0);  // yellow blink
      } else if (dryer.boosting()) {                    // max heat-up
        if (blink1) write(40, 100, 0); else write(0, 0, 0);
      } else {
        write(breathe, 0, 0);                           // green breathe
      }
      return;
    default:                                             // IDLE
      if (door::fitted() && !door::closed()) {          // load the trays
        write(60, 120, 0);
      } else {
        write((uint8_t)(breathe / 4), 0, (uint8_t)(breathe / 4)); // dim cyan
      }
      return;
  }
}

}  // namespace pixel

#else

namespace pixel {
void begin() {}
void update() {}
bool rePin(int) { return false; }
}

#endif
/* ==========================  src/control.cpp  ========================== */

// ---------------------------------------------------------------------
//  Module instances (defined here, used everywhere)
// ---------------------------------------------------------------------
SensorModule   sensors;
BatteryMonitor battery;
Dryer          dryer;
Settings       cfg;
Weather        weather;

bool weatherFresh() {
  return weather.rxMs != 0 && (millis() - weather.rxMs) < WX_STALE_MS;
}

// Effective outdoor conditions. Priority:
//   1. fresh forecast relayed by the phone (or manual entry)
//   2. stale forecast (better than nothing, flagged as such)
const char *outdoorSrc() {
  if (weatherFresh())            return weather.manual ? "manual" : "live";
  if (weather.rxMs)              return "stale";
  return "none";
}

bool getOutdoor(float &t, float &h) {
  // priority: MEASURED DHT11 (live) > phone-relayed weather > none
  if (dht::outdoor.ok && !isnan(dht::outdoor.t) && !isnan(dht::outdoor.h)) {
    t = dht::outdoor.t;  h = dht::outdoor.h;  return true;
  }
  if (weatherFresh() || weather.rxMs) {
    if (!isnan(weather.humRH)) { t = weather.tempC;  h = weather.humRH;  return true; }
  }
  return false;
}

LogRec Dryer::_log[LOG_MAX];

static const uint32_t kMagic = 0x53445259;   // "SDRY"
static const uint16_t kVer  = 8;   // v8: +stick/paste calculator fields
static const char *kPrefs = "dryer";

// ---------------------------------------------------------------------
//  Settings: defaults / NVS load / save
// ---------------------------------------------------------------------
Settings defaultSettings() {
  Settings s{};
  s.magic = kMagic; s.ver = kVer;
  s.setTemp   = DEF_SET_TEMP;
  s.tempHyst  = DEF_TEMP_HYST;
  s.maxTemp   = DEF_MAX_TEMP;
  s.humLow    = DEF_HUM_LOW;
  s.humHigh   = DEF_HUM_HIGH;
  s.humTarget = DEF_HUM_TARGET;
  s.requireHum= DEF_REQUIRE_HUM;
  s.dryMinutes= DEF_DRY_MINUTES;
  s.fanMin    = DEF_FAN_MIN;
  s.fanIn     = DEF_FAN_IN;
  s.fanOut    = DEF_FAN_OUT;
  s.fanSlope  = DEF_FAN_SLOPE;
  s.heaterMax = DEF_HEATER_MAX;
  s.cooldownSec = DEF_COOLDOWN_S;
  s.bypassPct = DEF_BYPASS_PCT;
  s.cutoffPct = DEF_CUTOFF_PCT;
  s.battType  = DEF_BATT_TYPE;
  s.tzMinutes = DEF_TZ_MINUTES;
  s.smartVent = DEF_SMART_VENT;
  s.boostHeat = DEF_BOOST_HEAT;
  s.kp = DEF_KP; s.ki = DEF_KI; s.kd = DEF_KD;
  s.requireWeight = false;         // dry-to-weight gate (opt-in)
  s.weightRateG   = 2.0f;          // g/min below this = "settled"
  s.weightMinY    = 10;            // minutes stable before ending
  s.scaleCal      = 1.0f;          // recalibrate via the website
  s.scaleOffset   = 0;
  s.targetG       = DEF_TARGET_G;  // v2.0 target weight (0 = off)
  s.stickCount    = 0;             // v2.0.16 calculator off by default
  s.stickWetG     = 2.5f;          // avg wet stick (g)
  s.pasteWaterPct = 35.0f;         // water in the paste (30-40 typical)
  s.targetMoistPct= 10.0f;         // finished residual moisture (8-10)
  s.fanTrigRH     = DEF_FAN_TRIG_RH;
  s.fanTrigMin    = DEF_FAN_TRIG_MIN;
  s.fanBurstS     = DEF_FAN_BURST_S;
  s.mode          = 0;             // AGARBATTI
  return s;
}

Settings loadSettings() {
  Preferences p;
  p.begin(kPrefs, true);
  Settings s{};
  size_t n = p.getBytes("cfg", &s, sizeof(s));
  p.end();
  if (n == sizeof(s) && s.magic == kMagic && s.ver == kVer) return s;
  return defaultSettings();
}

bool saveSettings(const Settings &s) {
  Preferences p;
  p.begin(kPrefs, false);
  size_t n = p.putBytes("cfg", &s, sizeof(s));
  p.end();
  return n == sizeof(s);
}

const char *stateName(DState s) {
  switch (s) {
    case DState::IDLE:     return "IDLE";
    case DState::RUNNING:  return "DRYING";
    case DState::COOLDOWN: return "PURGING";
    case DState::DONE:     return "DONE";
    case DState::FAULT:    return "FAULT";
  }
  return "?";
}

// ---------------------------------------------------------------------
//  Hardware outputs
// ---------------------------------------------------------------------
static inline int dutyOf(uint8_t pct) {           // % -> 0..1023
  return (int)pct * ((1 << PWM_RES_BITS) - 1) / 100;
}

void Dryer::setRelay(bool on) {
  _relayOn = on;                      // state the website shows
#if RELAYS_ENABLED
#if RELAY_ACTIVE_LOW
  digitalWrite(PIN_LOAD_RELAY, on ? LOW : HIGH);
#else
  digitalWrite(PIN_LOAD_RELAY, on ? HIGH : LOW);
#endif
#else
  (void)0;                            // no relay fitted: state only
#endif
}

void Dryer::outputsAllOff() {
  _heatDuty = 0; _fanDuty = 0; _fanInD = 0; _fanOutD = 0;
  _manUntil = 0;                        // knob window dies with the outputs
  pwmWritePin(PIN_BTS_RPWM, 0);
#if PIN_BTS_LPWM >= 0
  pwmWritePin(PIN_BTS_LPWM, 0);
#endif
  digitalWrite(PIN_BTS_EN, LOW);
  pwmWritePin(PIN_L298_ENA, 0);
  pwmWritePin(PIN_L298_ENB, 0);
}

// ---------------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------------
void Dryer::begin(const Settings &s) {
  _cfg = s;

  // heater pins
  pinMode(PIN_BTS_EN, OUTPUT);
#if PIN_BTS_LPWM >= 0
  pinMode(PIN_BTS_LPWM, OUTPUT);
  digitalWrite(PIN_BTS_LPWM, LOW);
#endif
  pwmInitPin(PIN_BTS_RPWM, HEATER_PWM_FREQ, PWM_RES_BITS);

  // fan pin - speed on ENB only. Direction is HARD-WIRED on the module
  // (v2.0.10: IN1/IN3 -> 5V, IN2/IN4 -> GND); the IN pins are -1 and
  // ENB LOW is a real off (both bridge switches open, fan coasts).
  pwmInitPin(PIN_L298_ENA, FAN_PWM_FREQ, PWM_RES_BITS);   // no-op at -1
  pwmInitPin(PIN_L298_ENB, FAN_PWM_FREQ, PWM_RES_BITS);

#if RELAYS_ENABLED
  pinMode(PIN_LOAD_RELAY, OUTPUT);
#endif
  setRelay(false);                 // (no-op without the relay fitted)

  battery.setType(_cfg.battType);
  _st = DState::IDLE;
}

void Dryer::applySettings(const Settings &s) {
  _cfg = s;                              // LIVE: takes effect on the next
  battery.setType(_cfg.battType);        // 1 s tick - even mid-cycle
  // -- v2.0.16: stick/paste calculator overrides the raw target -------
  // moisture belongs to the paste: N x wet x (1 - (water% - final%)/100)
  if (_cfg.stickCount > 0 && _cfg.stickWetG >= 0.5f) {
    float loss = (_cfg.pasteWaterPct - _cfg.targetMoistPct) / 100.0f;
    if (loss < 0) loss = 0;
    if (loss > 0.8f) loss = 0.8f;
    float t = (float)_cfg.stickCount * _cfg.stickWetG * (1.0f - loss);
    if (t >= 50.0f && t <= 9000.0f) {
      _cfg.targetG = roundf(t);
      Serial.printf("[cfg] target %.0f g COMPUTED: %u sticks x %.2f g wet,"
                    " paste %.0f%% water -> %.0f%% final\n",
                    (double)t, (unsigned)_cfg.stickCount,
                    (double)_cfg.stickWetG,
                    (double)_cfg.pasteWaterPct, (double)_cfg.targetMoistPct);
    }
  }
  Serial.printf("[cfg] LIVE settings applied: setTemp=%.1fC time=%umin fans=%u/%u%%\n",
                _cfg.setTemp, (unsigned)_cfg.dryMinutes, _cfg.fanIn, _cfg.fanOut);
}

void Dryer::start() {
  if (_st == DState::RUNNING) return;
  if (!door::calibrated() && scale.ok()) {   // gate only when the scale WORKS
    Serial.println(F("[start] REFUSED: calibrate the weigh scale first "
                     "(known weight on the trays) - start stays locked"));
    return;
  }
  if (!scale.ok()) {                   // v2.0.17: a dead scale != a dead dryer
    _scaleLost = true;
    warnE(15, "scale absent/dead - DEGRADED: time + RH termination only");
  }
  // v2.0: remember the batch weight at start (target-weight tracking)
  _wtStart = (scale.ok() && !isnan(scale.grams())) ? scale.grams()
             : (door::batchG() > 50.0f ? door::batchG() : NAN);
  // -- #7 door ajar at START (owner spec) --------------------------------
  if (door::fitted() && !door::closed()) {
    bz::startRepeat(BP::DOOR_AJAR, 2000);        // nag until it shuts
    Serial.println(F("[start] REFUSED: door is OPEN - close it first"));
    return;
  }
  // -- #8 nothing on the trays -------------------------------------------
  if (scale.ok() && !isnan(scale.grams()) && scale.grams() < 50.0f) {
    bz::startRepeat(BP::NO_TRAYS, 3000);
    Serial.println(F("[start] REFUSED: trays look EMPTY - load the batch"));
    return;
  }
  // -- E-code start gates (owner spec v2.0.14) --------------------------
  if (!isnan(_wtStart)) {
    if (_wtStart > 9500.0f) {                       // E16 overload
      warnE(16, "batch exceeds the 2x5 kg cells", _wtStart, 9500.0f);
      bz::play(BP::INVALID);
      Serial.println(F("[start] REFUSED: E16 - remove the excess load"));
      return;
    }
    if (_wtStart < -50.0f) {                        // E05 scale implausible
      warnE(5, "scale reads negative - check the load cells", _wtStart, 0.0f);
      bz::play(BP::E05_LOAD);                      // #26: 2 s held tone
      Serial.println(F("[start] REFUSED: E05 - load-cell error"));
      return;
    }
    if (_cfg.targetG > 0.0f) {                      // E17 invalid target
      if (_cfg.targetG >= _wtStart) {
        bz::play(BP::INVALID);
        warnE(17, "target must be BELOW the batch weight",
              _cfg.targetG, _wtStart);
        Serial.println(F("[start] REFUSED: E17 - target weight >= batch"));
        return;
      }
      if (_cfg.targetG < 50.0f) {
        bz::play(BP::INVALID);
        warnE(17, "target below the 50 g minimum", _cfg.targetG, 50.0f);
        Serial.println(F("[start] REFUSED: E17 - target below minimum"));
        return;
      }
    }
  }
  buzzer.stopAll();                                // setup nags end here
  if (battery.valid() && battery.percent() < 20)
    bz::play(BP::BATT_LOW);                         // #12: low battery at start
  _spReached = false; _midway = false; _anomWarned = false; _lastG = NAN;
  _fr.active = false;  _wr.active = false;          // fresh cycle, fresh E-records
  // -- E-code start gates (owner spec v2.0.14) --------------------------
  if (!isnan(_wtStart)) {
    if (_wtStart > 9500.0f) {                       // E16 overload
      warnE(16, "batch exceeds the 2x5 kg cells", _wtStart, 9500.0f);
      Serial.println(F("[start] REFUSED: E16 - remove the excess load"));
      return;
    }
    if (_wtStart < -50.0f) {                        // E05 scale implausible
      warnE(5, "scale reads negative - check the load cells", _wtStart, 0.0f);
      Serial.println(F("[start] REFUSED: E05 - load-cell error"));
      return;
    }
    if (_cfg.targetG > 0.0f) {                      // E17 invalid target
      if (_cfg.targetG >= _wtStart) {
        warnE(17, "target must be BELOW the batch weight",
              _cfg.targetG, _wtStart);
        Serial.println(F("[start] REFUSED: E17 - target weight >= batch"));
        return;
      }
      if (_cfg.targetG < 50.0f) {
        warnE(17, "target below the 50 g minimum", _cfg.targetG, 50.0f);
        Serial.println(F("[start] REFUSED: E17 - target below minimum"));
        return;
      }
    }
  }
  _fr.active = false;  _wr.active = false;          // fresh cycle, fresh E-records
  _tWarned = false; _suggestMin = -1;
  _trigSince = 0; _fanBurstUntil = 0; _rhErrSince = 0; _flatSince = 0;
  _scaleLostSince = 0; _voltLowSince = 0; _voltHighSince = 0;
  _tHighWarnSince = 0;  _scaleWasOk = scale.ok();
  _scaleLostSince = 0; _voltLowSince = 0; _voltHighSince = 0;
  _tHighWarnSince = 0;  _scaleWasOk = scale.ok();
  _integ = 0; _lastE = 0;
  clearLog();
  _lastLog = 0;
  _tStart = millis();
  _elapsed = 0;
  _why[0] = 0;
  setRelay(true);
  battery.setBypass(false);
  cyclelog::start();                  // history entry opens
  bz::cycleStart();                   // start: 3 s continuous (spec)
  _st = DState::RUNNING;
  Serial.printf("[dryer] START  %.1f C / RH %0.f-%0.f%% / %u min\n",
                _cfg.setTemp, _cfg.humLow, _cfg.humHigh,
                (unsigned)_cfg.dryMinutes);
}

void Dryer::stop() {               // user pressed STOP -> purge, then cut
  if (_st == DState::RUNNING) {
    strncpy(_endReason, "stopped", sizeof(_endReason));
    buzzer.beep(1, 500);              // stop: one long beep
    _manUntil = 0;                    // knob override ends with the cycle
    _wtGoodSince = 0;                 // weight-settle timer resets too
    _st = DState::COOLDOWN;
    _cdStart = millis();
    Serial.println("[dryer] user STOP -> purge");
  }
}

void Dryer::setManualHeat(uint8_t pct) {  // web knob turned
  if (pct > 100) pct = 100;
  _manPct  = pct;
  _manUntil = millis() + MANUAL_HEAT_MS;  // every turn re-arms the window
  Serial.printf("[man] manual heat %u%% - automatic resumes in %u s\n",
                (unsigned)pct, (unsigned)(MANUAL_HEAT_MS / 1000UL));
}

bool Dryer::manualOn() const {
  return _manUntil != 0 && millis() < _manUntil;
}

uint16_t Dryer::manualLeftS() const {
  if (_manUntil == 0 || millis() >= _manUntil) return 0;
  return (uint16_t)((_manUntil - millis() + 999UL) / 1000UL);
}

void Dryer::powerOn() {            // re-energise after DONE / FAULT
  _why[0] = 0;
  _fr.active = false;  _wr.active = false;   // E-records -> CLEARED view
  _fr.active = false;  _wr.active = false;   // E-records -> CLEARED view
  _wr.active = false;
  buzzer.stopAll();                   // silence every nagging pattern
  bz::play(BP::RECOVERED);            // #25: relief tone (owner spec)
  _st = DState::IDLE;
  setRelay(true);
  Serial.println("[dryer] power ON -> IDLE");
}

void Dryer::addMinutes(int m) {
  if (_st != DState::RUNNING) return;
  // +m minutes => pretend the cycle started m minutes later in the past:
  //   newElapsed = elapsed - m*60  =>  tStart = now - newElapsed*1000
  int32_t newElapsed = (int32_t)_elapsed - (int32_t)m * 60;
  if (newElapsed < 0) newElapsed = 0;
  _tStart = millis() - (uint32_t)newElapsed * 1000UL;   // shift the epoch
  _elapsed = (uint32_t)newElapsed;
}

void Dryer::faultNow(const char *why) { raiseFault(why); }   // door & co.

// ---- E-code fault system (owner spec, v2.0.14) --------------------------
const char *eName(uint8_t c) {
  switch (c) {
    case 1:  return "HEATER OVERHEATING";
    case 2:  return "POWER SOURCE ERROR";
    case 3:  return "HEATER FAILURE";
    case 4:  return "TEMP SENSOR FAILURE";
    case 5:  return "LOAD CELL ERROR";
    case 6:  return "FAN / RH ERROR";
    case 7:  return "DOOR OPEN";
    case 8:  return "DOOR SENSOR FAULT";
    case 9:  return "BLOWER FAILURE";
    case 10: return "HEATER CURRENT FAULT";
    case 11: return "RELAY / FEED FAULT";
    case 12: return "LOW BATTERY";
    case 13: return "LOW SUPPLY VOLTAGE";
    case 14: return "HIGH SUPPLY VOLTAGE";
    case 15: return "LOAD CELL COMM ERROR";
    case 16: return "LOAD OVERLOAD";
    case 17: return "INVALID TARGET";
    case 18: return "INVALID PARAMETERS";
    case 19: return "CYCLE TIMEOUT";
    case 20: return "TEMP OUT OF RANGE";
  }
  return "";
}

void Dryer::warnE(uint8_t code, const char *why, float val, float limit) {
  if (_wr.active && _wr.code == code) return;         // dedupe repeats
  _wr = FaultRec();
  _wr.code = code;  _wr.sev = 0;  _wr.active = true;
  _wr.sinceMs = millis();  _wr.val = val;  _wr.limit = limit;
  Serial.printf("[warn] E%02u %s - %s\n", code, eName(code), why);
  bz::startRepeat(BP::WARN, 15000);      // #23: 2x150 every 15 s while active
}

void Dryer::clearWarn(uint8_t code) {
  if (_wr.active && _wr.code == code) {
    _wr.active = false;                  // banner -> CLEARED
    bz::stopRepeat(BP::WARN);
    bz::play(BP::RECOVERED);             // #25: self-recovered
    Serial.printf("[dryer] E%02u cleared - conditions back to normal\n", code);
  }
}

void Dryer::faultNowE(uint8_t code, const char *why, float val, float limit) {
  _fr = FaultRec();
  _fr.code = code;  _fr.sev = 1;  _fr.active = true;
  _fr.sinceMs = millis();  _fr.val = val;  _fr.limit = limit;
  char buf[40];
  snprintf(buf, sizeof(buf), "E%02u %s", code, why);
  raiseFault(buf);                    // halt + power cut + 5 s beep
}

void Dryer::applyMode(uint8_t m) {       // 0 agarbatti / 1 user / 2 silica
  bz::play(BP::MODE);                    // #4: mode rotated (owner spec)
  if (m > 2) m = 0;
  if (m == 0) { _cfg.setTemp = 60.0f; _cfg.dryMinutes = 120; }   // AGARBATTI
  else if (m == 2) { _cfg.setTemp = 80.0f; _cfg.dryMinutes = 120; } // SILICAGEL
  // m == 1 (USER DEFINED): keep the current values untouched
  _cfg.mode = m;
  saveSettings(_cfg);
  applySettings(_cfg);
  Serial.printf("[mode] %s (target %.0fC, %u min)%s\n",
                m == 0 ? "AGARBATTI DEFAULT" :
                m == 2 ? "SILICAGEL DEFAULT" : "USER DEFINED",
                (double)_cfg.setTemp, (unsigned)_cfg.dryMinutes,
                (_st == DState::RUNNING || _st == DState::COOLDOWN)
                  ? " - applies to the next cycle" : "");
}

void Dryer::raiseFault(const char *why) {
  bool cycleWasOn = (_st == DState::RUNNING || _st == DState::COOLDOWN);
  strncpy(_why, why, sizeof(_why) - 1);
  _why[sizeof(_why) - 1] = 0;
  if (cycleWasOn) cyclelog::finish("fault", why);
  _st = DState::FAULT;
  bz::play(BP::CRITICAL);               // #24: 5 s, then nag every 10 s
  bz::startRepeat(BP::CRITICAL_R, 10000);
  outputsAllOff();
  setRelay(false);                 // hard power cut
  Serial.printf("[dryer] FAULT: %s -> power cut\n", why);
}

uint32_t Dryer::remainingS() const {
  if (_st != DState::RUNNING) return 0;
  uint32_t total = _cfg.dryMinutes * 60UL;
  return (total > _elapsed) ? (total - _elapsed) : 0;
}

const LogRec &Dryer::logAt(uint16_t i) const {
  uint16_t idx = (_logHead + i) % LOG_MAX;
  return _log[idx];
}

// ---------------------------------------------------------------------
//  Control laws
// ---------------------------------------------------------------------
void Dryer::pidStep() {            // heater: hold setTemp via BTS7960 duty
  float t = sensors.tAvg();
  if (isnan(t)) { _heatDuty = 0; _boosting = false; pwmWritePin(PIN_BTS_RPWM, 0); digitalWrite(PIN_BTS_EN, LOW); return; }

  // MANUAL KNOB from the website: the user's exact duty for up to
  // MANUAL_HEAT_MS; the MCU returns to automatic (boost / PID) by itself.
  // Over-temperature and sensor-fault safety still apply (checked in tick
  // before we get here), so the knob cannot override a safety cut.
  if (_manUntil != 0 && millis() < _manUntil) {
    _boosting = false;
    _integ = 0;                          // bumpless hand-back to automatic
    _lastE = _cfg.setTemp - t;
    uint8_t cap = _cfg.heaterMax;
    _heatDuty = (_manPct < cap) ? _manPct : cap;
    digitalWrite(PIN_BTS_EN, _heatDuty > 0 ? HIGH : LOW);
    pwmWritePin(PIN_BTS_RPWM, dutyOf(_heatDuty));
    return;
  }
  _manUntil = 0;                         // window over -> automatic again

  // FULL-POWER HEAT-UP: below (setTemp - band) the BTS runs at max output
  // (heaterMax cap); once the chamber is close to target the PID takes
  // over and holds the temperature there.
  if (_cfg.boostHeat && t < _cfg.setTemp - _cfg.tempHyst) {
    _boosting = true;
    _integ = 0;                          // no wind-up from the boost phase
    _lastE = _cfg.setTemp - t;
    _heatDuty = _cfg.heaterMax;
    digitalWrite(PIN_BTS_EN, HIGH);
    pwmWritePin(PIN_BTS_RPWM, dutyOf(_heatDuty));
    return;
  }
  _boosting = false;

  float e = _cfg.setTemp - t;
  _integ += e * _cfg.ki;                          // 1 s loop
  if (_integ > 100.0f) _integ = 100.0f;
  if (_integ < -20.0f) _integ = -20.0f;           // anti-windup
  float d = e - _lastE;
  _lastE = e;

  float out = _cfg.kp * e + _integ + _cfg.kd * d;
  float cap = (float)_cfg.heaterMax;
  if (out < 0)   out = 0;
  if (out > cap) out = cap;
  _heatDuty = (uint8_t)(out + 0.5f);

  digitalWrite(PIN_BTS_EN, _heatDuty > 0 ? HIGH : LOW);
  pwmWritePin(PIN_BTS_RPWM, dutyOf(_heatDuty));
}

void Dryer::fanStep() {            // v2.0: burst venting, ONE outlet fan
  // Spec: RH above fanTrigRH (60 %) for fanTrigMin (1) minute -> the fan
  // runs at 100 % for fanBurstS (60) seconds, then re-arms. Purge = 100 %
  // continuous. fanMin is an optional continuous floor (default 0 = off).
  float h = sensors.hMax();       // control on the wettest sensor
  uint8_t d = 0;
  uint32_t now = millis();

  if (_st == DState::COOLDOWN) {
    d = 100;                      // purge heat + moist air before power cut
  } else if (_st == DState::RUNNING && !isnan(h)) {
    if (h >= _cfg.fanTrigRH) {
      if (_trigSince == 0) _trigSince = now ? now : 1;
      bz::play(BP::FAN_ON);                       // #15: fan kicks in
      if (_fanBurstUntil == 0 &&
          now - _trigSince >= (uint32_t)_cfg.fanTrigMin * 60000UL) {
        _fanBurstUntil = (now ? now : 1) + (uint32_t)_cfg.fanBurstS * 1000UL;
        _trigSince = 0;           // next burst needs a fresh trigger window
        Serial.printf("[fan] RH %.0f%% -> %u s burst at 100%%\n",
                      (double)h, (unsigned)_cfg.fanBurstS);
      }
    } else _trigSince = 0;
    if (_fanBurstUntil != 0) {
      if (now >= _fanBurstUntil) _fanBurstUntil = 0;   // burst finished
      else d = 100;
    }
    if (d == 0) d = _cfg.fanMin;  // optional continuous floor
  } else {
    _trigSince = 0; _fanBurstUntil = 0;
  }

  uint8_t dOut = (uint8_t)((uint16_t)d * _cfg.fanOut / 100);
  // kick-start: a resting fan rotor can stall at very low PWM - when the
  // channel wakes from 0, one 1 s tick at 60 % gets it spinning
  if (_fanOutD == 0 && dOut > 0 && dOut < 60) dOut = 60;
#if FAN_FIXED_DIR
  // enable-PWM: below ~40 % duty a 12 V fan just hums - clamp the floor
  if (dOut > 0 && dOut < FAN_PWM_FLOOR) dOut = FAN_PWM_FLOOR;
#endif

  _fanDuty = d;                   // demand (shown as fan %)
  _fanInD  = 0;                   // intake channel removed in v2.0
  _fanOutD = dOut;
  if (PIN_L298_ENA >= 0) pwmWritePin(PIN_L298_ENA, 0);
  pwmWritePin(PIN_L298_ENB, dutyOf(dOut));
}

// ---------------------------------------------------------------------
//  1-second tick
// ---------------------------------------------------------------------
void Dryer::tick() {
  uint32_t now = millis();
  if (_lastTick != 0 && now - _lastTick < CONTROL_PERIOD_MS) return;
  _lastTick = now ? now : 1;

  // -- battery housekeeping (safe shutdown; bypass only with relays) ---
  uint8_t pct = battery.percent();
  bool lowCut = battery.valid() && pct <= _cfg.cutoffPct;
#if RELAYS_ENABLED
  bool bypOn  = battery.valid() && pct <= _cfg.bypassPct && !lowCut;
  static bool prevByp = false;
  if (bypOn != prevByp) {
    Serial.printf("[batt] %s @ %.2fV (%u%%) - bypass threshold %u%%\n",
                  bypOn ? "BYPASS ON (battery low)" : "BYPASS OFF (battery recovered)",
                  battery.volts(), pct, _cfg.bypassPct);
    prevByp = bypOn;
  }
  battery.setBypass(bypOn);
#else
  battery.setBypass(false);           // no bypass relay fitted
#endif

  // -- safety: over-temperature ----------------------------------------
  float tMax = sensors.tMax();
  if (!isnan(tMax) && tMax >= _cfg.maxTemp && _st != DState::DONE) {
    faultNowE(1, "over-temperature - check airflow / coil size",
               sensors.tMax(), _cfg.maxTemp);
    fanStep();
    return;
  }

  // -- safety: both sensors dead ---------------------------------------
  if (sensors.anyOk()) {
    _sensFailSince = 0;
  } else {
    if (_sensFailSince == 0) _sensFailSince = now;
    if (_st == DState::RUNNING && now - _sensFailSince > SENSOR_FAIL_GRACE) {
      faultNowE(4, "all chamber sensors stopped responding");
      return;
    }
  }

  // -- v2.0.17 DEGRADED: one chamber sensor left -> warn + keep drying --
  {
    bool s1 = sensors.s1ok(), s2 = sensors.s2ok();
    if ((s1 != s2) && !_singleWarned) {
      _singleWarned = true;
      warnE(4, s1 ? "DHT22 return sensor lost - running on the AHT10 only"
                  : "AHT10 top sensor lost - running on the DHT22 only");
    } else if (s1 && s2 && _singleWarned) {
      _singleWarned = false;
      clearWarn(4);
    }
  }

  // -- safety: battery empty -------------------------------------------
  if (lowCut && _st != DState::DONE && _st != DState::FAULT) {
    faultNowE(12, "battery empty - safe shutdown",
               battery.volts(), 0.0f);
    return;
  }

  // -- E13/E14: supply-voltage guards (12.8 V LiFePO4 band) -------------
  if (battery.valid()) {
    float vb = battery.volts();
    if (vb < 11.5f) {
      if (_voltLowSince == 0) _voltLowSince = now;
      else if (now - _voltLowSince > 60000UL)
        warnE(13, "supply below 11.5 V - check panel / charge", vb, 11.5f);
    } else _voltLowSince = 0;
    if (vb > 15.0f) {
      if (_voltHighSince == 0) _voltHighSince = now;
      else if (now - _voltHighSince > 30000UL) {
        faultNowE(14, "supply above 15 V - check the MPPT setting", vb, 15.0f);
        return;
      }
    } else _voltHighSince = 0;
  }

  switch (_st) {
    case DState::IDLE:
    case DState::FAULT:
      outputsAllOff();
      break;

    case DState::DONE:
      outputsAllOff();
      if (_doneAt && now - _doneAt > 60000UL) {   // #35: safe to open
        bz::play(BP::COOL_DONE);
        _doneAt = 0;
      }
      break;

    case DState::RUNNING: {
      _elapsed = (now - _tStart) / 1000UL;

      // -- E15 DEGRADED (v2.0.17): scale lost -> CONTINUE on time + RH ----
      if (scale.ok()) {
        if (_scaleLost) {
          _scaleLost = false;
          clearWarn(15);
          Serial.println(F("[dryer] scale RECOVERED - weight tracking resumes"));
        }
        _scaleWasOk = true;  _scaleLostSince = 0;
      } else if (_scaleWasOk) {
        if (_scaleLostSince == 0) _scaleLostSince = now;
        else if (now - _scaleLostSince > 10000UL && !_scaleLost) {
          _scaleLost = true;
          warnE(15, "HX711 lost - DEGRADED: continuing on time + RH stop");
        }
      }

      // -- E20: far above setpoint but under the hard cut (PID runaway) --
      float tA20 = sensors.tAvg();
      if (!isnan(tA20) && tA20 > _cfg.setTemp + 15.0f) {
        if (_tHighWarnSince == 0) _tHighWarnSince = now;
        else if (now - _tHighWarnSince > 60000UL)
          warnE(20, "chamber far above setpoint", tA20, _cfg.setTemp + 15.0f);
      } else _tHighWarnSince = 0;

      // -- #13/#19 battery critical + 5-min-to-timeout nags ---------------
      if (battery.valid() && battery.percent() < 10)
        bz::startRepeat(BP::BATT_CRIT, 30000);
      else bz::stopRepeat(BP::BATT_CRIT);
      if (remainingS() <= 300) bz::startRepeat(BP::TIMEOUT5, 30000);
      else                     bz::stopRepeat(BP::TIMEOUT5);

      // -- #14/#17/#18/#22 progress cues (owner spec) ---------------------
      float gNow = (scale.ok() && !isnan(scale.grams())) ? scale.grams() : NAN;
      if (!isnan(gNow) && !isnan(_wtStart)) {
        if (!_spReached && !isnan(sensors.tAvg()) &&
            sensors.tAvg() >= _cfg.setTemp - 1.0f) {
          _spReached = true;
          bz::play(BP::SETPOINT);                  // warm-up over
        }
        if (_cfg.targetG > 0) {
          float span = _wtStart - _cfg.targetG;
          if (span > 1.0f) {
            if (!_midway && (_wtStart - gNow) >= span / 2) {
              _midway = true;
              bz::play(BP::MIDWAY);                // 50 % moisture removed
            }
            if (gNow > _cfg.targetG && gNow <= _cfg.targetG * 1.05f)
              bz::startRepeat(BP::APPROACH, 60000);// target approaching
            else bz::stopRepeat(BP::APPROACH);
          }
        }
        if (!_anomWarned && !isnan(_lastG) && _elapsed > 60 &&
            fabsf(gNow - _lastG) > 150.0f) {       // tray shifted / fell
          _anomWarned = true;
          bz::play(BP::ANOMALY);
          Serial.printf("[warn] weight jumped %.0f -> %.0f g mid-cycle\n",
                        (double)_lastG, (double)gNow);
        }
        if (!isnan(gNow)) _lastG = gNow;
      }

      pidStep();
      fanStep();

      // ---- v2.0 watchdog: HEATER FAILURE (constant temp 3 min) ---------
      // While heating up (still 2 deg below target, duty >= 80 %), the
      // chamber temperature must creep up. Flat for 3 minutes = the coil
      // / fuse / PSU / EN path is dead -> fault (spec).
      {
        float tA = sensors.tAvg();
        if (!isnan(tA) && tA < _cfg.setTemp - 2.0f && _heatDuty >= 80) {
          if (_flatSince == 0) { _flatSince = now; _flatT0 = tA; }
          else if (now - _flatSince >= 180000UL && tA - _flatT0 < 0.5f) {
            faultNowE(3, "no temperature rise for 3 min while heating",
                     tA, _cfg.setTemp - 2.0f);
            return;
          }
        } else _flatSince = 0;
      }

      // ---- v2.0 watchdog: FAN ERROR (RH above humHigh for 5 min) -------
      // Bursts are firing but the humidity will not come down: fan dead,
      // blocked duct or the sensor is wet -> fault (spec).
      {
        float hM = sensors.hMax();
        if (!isnan(hM) && hM >= _cfg.humHigh) {
          if (_rhErrSince == 0) _rhErrSince = now ? now : 1;
          else if (now - _rhErrSince >= 300000UL) {
            faultNowE(6, "RH above 60% for 5 min despite the fan",
                     sensors.hMax(), 60.0f);
            return;
          }
        } else _rhErrSince = 0;
      }

      // ---- v2.0 target weight: warn 5 min before time-up + suggest ----
      if (_cfg.targetG > 0 && scale.ok() && !isnan(_wtStart) &&
          !isnan(scale.grams()) && !_tWarned) {
        float cur = scale.grams();
        uint32_t remainS = (uint32_t)_cfg.dryMinutes * 60UL - _elapsed;
        if (cur > _cfg.targetG * 1.05f && remainS <= 300UL) {
          _tWarned = true;
          float rate = scale.rate();          // g/min, positive = losing
          _suggestMin = (rate > 0.1f) ? (cur - _cfg.targetG) / rate : -1.0f;
          if (_suggestMin > 0.0f)
            Serial.printf("[warn] target weight not reachable in the last "
                          "%u min - at this rate it needs about +%.0f min\n",
                          (unsigned)(remainS / 60UL), (double)_suggestMin);
          else
            Serial.println(F("[warn] target weight not reachable and the "
                             "rate is too low to estimate"));
          buzzer.beep(2, 500, 300);           // distinct warning pattern
        }
      }

      bool timeUp   = _elapsed >= _cfg.dryMinutes * 60UL;
      bool humOk    = !_cfg.requireHum ||
                      (!isnan(sensors.hMax()) && sensors.hMax() <= _cfg.humTarget);

      // dry-to-weight: the batch itself says when it is dry. The rate
      // must stay under weightRateG (in g/min) for weightMinY minutes.
      // Scale absent + gate on -> warn once and fall back to time+RH.
      bool wtOk = true;
      static bool warnedNoScale = false;
      if (_cfg.requireWeight) {
        if (scale.ok() && !isnan(scale.rate())) {
          warnedNoScale = false;
          if (fabsf(scale.rate()) < _cfg.weightRateG) {
            if (_wtGoodSince == 0) _wtGoodSince = now;
          } else _wtGoodSince = 0;
          wtOk = (_wtGoodSince != 0 &&
                  now - _wtGoodSince >= (uint32_t)_cfg.weightMinY * 60UL);
        } else {
          if (!warnedNoScale) {
            Serial.println("[warn] weight gate requested but scale absent - falling back to time+RH");
            warnedNoScale = true;
          }
          _wtGoodSince = 0;
        }
      } else _wtGoodSince = 0;

      if (timeUp && humOk && wtOk) {
        // v2.0: within 5 % of the target weight (when set) = complete,
        // otherwise DONE-WITH-WARNING (the cycle still ends on time)
        bool tgtOk = true;
        if (_cfg.targetG > 0 && scale.ok() && !isnan(scale.grams()))
          tgtOk = scale.grams() <= _cfg.targetG * 1.05f;
        if (tgtOk) {
          strncpy(_endReason, "completed", sizeof(_endReason));
        } else {
          strncpy(_endReason, "E19 done - target weight NOT reached",
                  sizeof(_endReason));
          Serial.println(F("[warn] cycle ended: target weight not reached "
                           "within 5 % - re-run if needed"));
          buzzer.beep(2, 500, 300);
        }
        _st = DState::COOLDOWN;
        _cdStart = now;
        Serial.println("[dryer] cycle complete -> purge, then power cut");
      }

      // data log every 10 s
      if (_lastLog == 0 || now - _lastLog >= LOG_PERIOD_MS) {
        _lastLog = now;
        LogRec r{};
        r.t = _elapsed;
        r.tAvg10 = isnan(sensors.tAvg()) ? 0 : (int16_t)(sensors.tAvg() * 10);
        r.hAvg10 = isnan(sensors.hAvg()) ? 0 : (int16_t)(sensors.hAvg() * 10);
        r.hMax10 = isnan(sensors.hMax()) ? 0 : (int16_t)(sensors.hMax() * 10);
        r.heat = (int16_t)_heatDuty;
        r.fan  = (int16_t)_fanDuty;
        r.vb10 = (int16_t)(battery.volts() * 10);
        r.bat  = (int16_t)pct;
        r.wt10 = scale.ok() && !isnan(scale.grams())
                   ? (int16_t)constrain(scale.grams() / 10.0f, -3200.0f, 3200.0f)
                   : INT16_MIN;      // marker: no scale this cycle
        r.ot10 = (dht::outdoor.ok && !isnan(dht::outdoor.t))
                   ? (int16_t)constrain(dht::outdoor.t * 10.0f, -300.0f, 300.0f)
                   : INT16_MIN;      // marker: no outdoor sensor
        r.oh10 = (dht::outdoor.ok && !isnan(dht::outdoor.h))
                   ? (int16_t)constrain(dht::outdoor.h * 10.0f, 0.0f, 100.0f)
                   : INT16_MIN;
        if (_logN < LOG_MAX) { _log[_logN] = r; _logN++; _logHead = 0; }
        else { _log[_logHead] = r; _logHead = (_logHead + 1) % LOG_MAX; }
      }
      break;
    }

    case DState::COOLDOWN: {
      _heatDuty = 0;
      pwmWritePin(PIN_BTS_RPWM, 0);
      digitalWrite(PIN_BTS_EN, LOW);
      fanStep();                    // 100 % purge
      if (now - _cdStart >= (uint32_t)_cfg.cooldownSec * 1000UL) {
        outputsAllOff();
        setRelay(false);            // relay (if fitted) cuts the power
        cyclelog::finish(_endReason,
                    _scaleLost ? "E15 scale lost - degraded time/RH stop" : "");
        _finalG    = (scale.ok() && !isnan(scale.grams())) ? scale.grams() : NAN;
        _endElapsed = _elapsed;                 // completion summary (v2.0.15)
        _doneAt    = now;
        bz::cycleDone();             // 5 s continuous (spec)
        _st = DState::DONE;
#if RELAYS_ENABLED
        Serial.println("[dryer] DONE - power cut by relay");
#else
        Serial.println("[dryer] DONE - heater and fans off");
#endif
      }
      break;
    }
  }
}

/* ==========================  src/cyclelog.cpp  ========================== */
#include <LittleFS.h>
#include <Preferences.h>
#include <sys/time.h>
#include <algorithm>

namespace cyclelog {

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
/* ==========================  src/web.cpp  ========================== */
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

// v2.0.21: mirror a real time set into the DS1302 (no-op when absent /
// classic). The chip then holds the clock across a full power-down.
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
  jn(o, "hMax", sensors.hMax(), 1, sensors.anyOk());   o += ",";
  ji(o, "heat", dryer.heatDuty());                     o += ",";
  dryer.applySettings(cfg);
  server.send(200, "text/plain", "ok");
}

// ---------------------------------------------------------------------
//  clock: /api/settime (browser pushes its date & time automatically)
// ---------------------------------------------------------------------
  o += keypad.ok() ? "true" : "false";                o += ",";
  js(o, "last", keypad.last() ? String(keypad.last()) : String(""));
  o += "},";
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
  o += "},";

  o += "\"dht\":{\"ok\":";  o += dht::outdoor.ok ? "true" : "false";  o += ",";
  jn(o, "t", isnan(dht::outdoor.t) ? 0.0f : dht::outdoor.t, 1);   o += ",";
  jn(o, "h", isnan(dht::outdoor.h) ? 0.0f : dht::outdoor.h, 0);
  o += "},";

  o += "\"door\":{\"fitted\":";                     // lock workflow state
  o += door::fitted() ? "true" : "false";          o += ",";

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
  // v2.0.19: on-device menu state (web virtual keypad); null = menu closed
  {
    String m = "null";
    if (menu::active()) {
      m = "{\"cur\":" + String(menu::cursor()) + ",\"edit\":" +
          String(menu::editing() ? "true" : "false") + ",\"items\":[";
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

  }
  jn(o, "wtStart",   isnan(dryer.wtStartG())  ? 0.0f : dryer.wtStartG(),  0); o += ",";
  jn(o, "finalG",    isnan(dryer.finalG())    ? 0.0f : dryer.finalG(),    0); o += ",";
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
  jn(o, "outH", oH, 1, hasOut);             o += "},";

  o += "\"bat\":{";
  jn(o, "v", battery.valid() ? battery.volts() : 0.0f, 2); o += ",";
  ji(o, "pct", battery.percent());          o += ",";
  dns.start(53, "*", ip);

  server.on("/",            HTTP_GET,  handleRoot);
  server.on("/display",     HTTP_GET,  handleDisplay);   // kiosk screen
  server.on("/online",      HTTP_GET,  handleOnline);
  server.on("/api/data",    HTTP_GET,  handleData);
  server.on("/api/history", HTTP_GET,  handleHistory);
  server.on("/api/log.csv", HTTP_GET,  handleCsv);
  buildSettings(o, defaultSettings());
  o += "}}";
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
/* ==========================  src/main.cpp  ========================== */
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
#if OTA_NETWORK_ENABLED
#include <ESPmDNS.h>          // core built-ins - network OTA from the
#include <ArduinoOTA.h>       // Arduino IDE over the dryer hotspot
#endif

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
    PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST,
    PIN_SCALE_CLK, PIN_SCALE_DOUT, PIN_DOOR_LOCK, PIN_DOOR_REED,
    PIN_TXDISP_TX,
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
  Serial.printf("[diag] %s\n", rtc::statusText());
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

  // 3c. DS1302 RTC (v2.0.21): coin-cell date & time that survives a
  // FULL power-down. Runs BEFORE web::begin() so a good RTC time wins
  // over the (stale) NVS restore - clockBoot() then sees a live clock
  // and backs off. No chip / untrusted time = today's phone-sync clock.
#if RTC_ENABLED
  rtc::begin();
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
  // 4b. network OTA: Arduino IDE -> Tools > Port -> "smart-dehumidifier at
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
/* ==== END OF FILE ====
 * total lines (wc -l): 8537   non-blank lines: 7880
 * build 2026-09-25 - if these numbers differ from what you see,
 * you are looking at an older copy; regenerate: node tools/single-file/assemble.js
 */
