/**
 * @file config-s3.h
 * @brief Central pin map + compiled-in DEFAULT thresholds for the
 *        Smart Dehumidifier (ESP32-S3 + BTS7960 + L298N + AHT10
 *        chamber #1 + DHT22 chamber #2 + DHT11 outdoor).
 *
 * Everything in the "DEFAULTS" section can be overridden from the website
 * (slide 2 - Custom). The values here are what slide 1 ("Defaults") shows
 * and what gets restored when the user presses "Apply defaults".
 *
 * PIN RULES OBSERVED
 *  - No ADC2 pins while WiFi is on (ADC2 is disabled by the WiFi driver),
 *    so battery sense uses GPIO4 (ADC1_CH3, WiFi-safe on the S3).
 *  - AHT10 has a FIXED I2C address (0x38): max one per bus. v2.0.9: the
 *    chamber runs ONE AHT10 (top, Wire) + a DHT22 on the cool-return
 *    path - Wire1 is FREE for a future I2C part.
 *  - L298N ENB (the one pin that energises the fan) is on a
 *    non-strapping pin, so no fan twitch at boot.
 *  - GPIO0 is the on-board BOOT button = physical start/stop (zero
 *    wiring); GPIO48 is the on-board WS2812 status pixel (v2.0.15).
 *  - v2.0.20: this board's header does NOT break out GPIO 22-34, so the
 *    door reed (was 34) runs on 21 and the outdoor DHT11 (was 33) on 41.
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
// Enable Pages in repo Settings (main branch, /docs folder) and put the
// URL here. Leave as-is and /online simply falls back to the built-in UI.
#define ONLINE_UI_URL "https://pavan-nikhil-993.github.io/arena/"

// =====================================================================
//  PIN MAP  (ESP32-S3 DevKit - the pillar build; v2.0.20 remap in)
// =====================================================================
// ---- AHT10 sensors (fixed 0x38 address -> two separate I2C buses) ----
#define PIN_I2C0_SDA 8               // AHT10 (chamber top) + PCF8574 #1/#2
#define PIN_I2C0_SCL 9
#define PIN_I2C1_SDA -1              // I2C1 FREE (v2.0.9: chamber source
#define PIN_I2C1_SCL -1              // #2 = DHT22; GPIO 11 spare)

// ---- Heater coil: PILLAR CLASS 500 W ----------------------------------
// 12.6 V full battery across 0.32 ohm nichrome (two 0.64 ohm halves in
// parallel, ~2.3 m of 1.0 mm wire) = 500 W peak, 41.7 A through the
// BTS7960 (43 A rated - heatsink mandatory). The 70 % duty cap keeps
// CONTINUOUS draw at ~350 W inside the 33 A PSU; the 550 W solar panel
// carries the full 500 W at peak sun with the battery buffering clouds.
// ---- BTS7960 : heating coil driver (matched to the built board) -----
// Physical build: ESP32 -> 555 timer level shifter (3.3 V -> 5 V PWM,
// RESET-pin trick) -> RPWM; LPWM hard-tied to GND at the module.
#define PIN_BTS_RPWM 12              // 1 kHz PWM -> 555 pin 4 -> 555 pin 3 -> RPWM
#define PIN_BTS_LPWM -1              // tied to GND at the module (-1 = not a GPIO)
#define PIN_BTS_EN   13              // R_EN + L_EN of the module tied together

// ---- L298N : ONE outlet fan (v2.0: the intake fan was removed) -------
// The system has a single EXHAUST fan: RH > fanTrigRH for fanTrigMin
// minutes -> the fan runs at 100% for fanBurstS seconds (burst venting).
// The freed intake pins (14/15/16) carry the v2.0 power button + latch.
#define PIN_L298_ENA -1              // (intake channel removed)
#define PIN_L298_IN1 -1
#define PIN_L298_IN2 -1
#define PIN_L298_ENB 17              // outlet (exhaust) fan PWM - the
                                     // ONLY wired L298N pin (v2.0.10)
#define PIN_L298_IN3 -1              // direction HARD-WIRED on the
#define PIN_L298_IN4 -1              // module: IN3 -> 5V, IN4 -> GND.
                                     // REMOVE the ENB jumper cap; add
                                     // 10k ENB->GND (fan off at boot).
                                     // freed 18/21 here (later used: 18 = opto v2.0.13,
                                     // 21 = door v2.0.20).
#define FAN_FIXED_DIR  1             // ENB-only PWM (no IN-pin drive)
#define FAN_PWM_FLOOR  40            // enable-PWM below ~40% just hums

// ---- Battery sense ---------------------------------------------------
#define PIN_VBAT_ADC    4            // ADC1_CH3, WiFi-safe
#define PIN_VBAT_ENABLE 5            // gates the divider's low-side transistor
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
// the bypass supply (bypass mode) through a 2-channel relay on GPIO 6/7;
// an optocoupler input verifies which supply is actually live. The MCU
// switches the relay ONLY with the loads off (heater+fan quiet) - a mode
// change requested while a supply is under load = refused + error beep.
// When the battery hits the cutoff % and the bypass supply is live, the
// MCU switches to bypass automatically (and beeps).
#define PIN_LOAD_RELAY  -1           // (merged into the supply relay below)
#define PIN_BYPASS_CTRL -1
#define PIN_SUPPLY_CH1   6           // relay CH1: solar feed enable
#define PIN_SUPPLY_CH2   7           // relay CH2: bypass feed enable
#define SUPPLY_RELAYS_ENABLED 1

// ---- DHT sensors (v2.0.9) ----------------------------------------------
// Chamber: AHT10 top (I2C0) + DHT22 on the cool-return path = chamber
//   source #2 (averages + RH-peak keep working; one AHT10 fewer).
// Outdoor: DHT11 in permanent shade (north side) - weather + smart
//   venting (coarse but fine outdoors; the DHT22 does the precision)
// Both DHTs: 10k pull-up DATA->3V3, DATA direct on an S3 GPIO.
#define SENS2_DHT           1    // 1 = chamber source #2 is a DHT22
#define DHT_CHAMBER_ENABLED 1
#define PIN_DHT22_CHAMBER   10   // cool-return path
#define DHT_CHAMBER_MS      3000 // DHT22 spec min 2 s
#define DHT_OUT_ENABLED     1
#define PIN_DHT11_OUT       41   // shade! (v2.0.20 remap: many S3 devkit
                                     // headers (e.g. 18x13-pin boards) do
                                     // NOT break out GPIO33/34, so 33 -> 41)
#define DHT_READ_MS         10000

// Inputs:
#define PIN_SOLAR_TOGGLE  39         // physical toggle: SOLAR requested
                                     // (GPIO39 = JTAG TDO, fine as input)
#define PIN_SUPPLY_OPTO   18         // optocoupler: selected supply live
                                     // (v2.0.13: 18, not 35 - octal-PSRAM
                                     // boards like the N16R8 use GPIO
                                     // 35/36/37 for PSRAM. 18 was freed by
                                     // the L298N fixed-direction change.)
#define OPTO_ACTIVE_HIGH  1          // opto output HIGH = supply present

// Master power button + soft-latch (v2.0 hard power-off):
// press BUTTON-1 -> P-MOSFET latch feeds the MCU -> firmware asserts
// PIN_POWER_HOLD within ms of boot. BUTTON-1 held 3 s = firmware drops
// the hold -> the WHOLE system loses power (hard off). Held 10 s =
// reboot. See manual 02 sect.4.11 for the latch circuit.
#define PIN_BTN1        15           // master power button sense (input)
#define PIN_BTN2        16           // "default automation" button
#define PIN_POWER_HOLD  14           // keeps the P-MOSFET latch closed
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
#define LOG_MAX            6480      // 18 h of records (16 B each) - S3 has the RAM
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
//     "smart-dryer at 192.168.4.1". Uses only core built-ins (no extra
//     libraries to install). Set 0 to compile it out.
#define OTA_NETWORK_ENABLED 1
#define OTA_HOSTNAME "smart-dryer"

// ---- Buzzer (active module, e.g. KY-012) ---------------------------------
// MANDATORY since v2.0 (full beep-pattern set), dedicated GPIO38.
#define PIN_BUZZER       38
// ---- RGB status pixel (WS2812) - the DevKitC-1 on-board LED ----------
// v2.0.15: one data pin, driven via hardware SPI (no library). GPIO48 =
// DevKitC-1 v1.0 RGB; v1.1 boards use GPIO38 (then the buzzer must move,
// e.g. to PCF #2 P7 with the front panel). Chain an external strip by
// raising PIXEL_COUNT - same wire.
#define PIXEL_ENABLED  1
#define PIN_PIXEL      48
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
// The WEBSITE is the display now: "/" = full control, "/display" = a
// read-only kiosk page that behaves exactly like the hardware screen
// (a phone/tablet mounted on the pillar shows it). Set DISPLAY_ENABLED
// 1 + wire SCK 40 / MOSI 41 / CS 42 / DC 47 / RST 48 if a screen ever
// returns. Pins 3/11 are FREE in this build; 40/42/47 carry the DS1302
// RTC (v2.0.21), 41 the DHT11 (v2.0.20), 48 the status pixel (v2.0.15), so
// a returning screen must re-pick MOSI/RST around them.
#define DISPLAY_ENABLED  0
#define PIN_TFT_SCK      40          // SPI clock (any output GPIO)
#define PIN_TFT_MOSI     41          // SPI data
#define PIN_TFT_CS       42          // chip select
#define PIN_TFT_DC       47          // data/command
#define PIN_TFT_RST      48          // reset
#define TFT_SPI_HZ       26000000    // 26 MHz (GPIO-matrix safe)
// If colours look wrong/swapped: change 0x48 (MADCTL) or remove INVON.

// ---- Serial display bridge (v2.0.3) ------------------------------------
// Reuses a PARALLEL (D0-D7 + WR + RD "UNO-shield") TFT through a
// companion Arduino: the S3 streams the status screen as tiny text
// packets, the Arduino renders. One wire: GPIO3 (TX) -> Arduino RX
// (UNO/Nano pin 4 SoftwareSerial, Mega pin 19 RX1) + GND common;
// 9600 baud is plenty (~150 B/s). NEVER wire the Arduino 5 V TX to
// the S3. Companion sketch: arduino-ide/display-bridge-uno/. See
// manual 02 sect.4.12.
#define TXDISP_ENABLED  0          // (web display replaced it - v2.0.7;
#define PIN_TXDISP_TX   3          //  set 1 to drive a UNO+TFT again)
#define TXDISP_BAUD     9600

// ---- DS1302 RTC: date & time, coin-cell backed (v2.0.21) --------------
// Keeps the wall clock through a FULL power-down (even a battery
// disconnect). Auto-detected at boot: no chip = the phone-sync + NVS
// clock, nothing else changes. Every real time set (phone sync, keypad
// menu, serial 'rtcset') is written to the chip.
// Wiring: VCC -> 3V3, GND -> GND, SCLK -> 40, I/O -> 42, RST -> 47
// (the module's BZ pin unused; its CR2032 stays on the module).
#define RTC_ENABLED    1
#define PIN_RTC_RST    40
#define PIN_RTC_SCLK   42
#define PIN_RTC_IO     47

// ---- Weigh scale: HX711 + load cells - ENABLED on the S3 --------------
// "Dry to weight": the cycle ends when the batch stops losing weight.
// Mount the cells on the bottom tray rails OUTSIDE the hot chamber (the
// HX711 board itself lives in the control bay; cells drift with heat).
#define SCALE_ENABLED   1
#define PIN_SCALE_CLK   1           // HX711 PD_SCK
#define PIN_SCALE_DOUT  2           // HX711 DOUT

// ---- Door: LIMIT SWITCH ONLY (this build has no solenoid lock) --------
// The limit switch on GPIO21 (to GND, closed = LOW, internal pull-up)
// (v2.0.20 remap from GPIO34 - not broken out on many S3 devkit headers)
// gives the door OPEN/CLOSED status. Everything smart is software:
//   - START is refused until the scale is calibrated (gate)
//   - closing the loaded door weighs the batch -> READY
//   - opening the door mid-cycle = instant FAULT
// A 12 V solenoid lock can be added later: set DOOR_LOCK_ENABLED 1 and
// wire the driver to PIN_DOOR_LOCK (a free GPIO, e.g. 3 or 11).
#define DOOR_ENABLED       1
#define PIN_DOOR_REED      21          // limit switch, door closed = LOW
#define DOOR_CLOSED_LEVEL  LOW
#define DOOR_LOCK_ENABLED  0           // no lock fitted in this build
#define PIN_DOOR_LOCK      -1          // (e.g. 3 or 11 when fitted)
#define DOOR_LOCK_ACTIVE   HIGH

// ---- 16-key hex keypad on a PCF8574 I2C backpack ----------------------
// Shares Wire (GPIO8/9) with AHT10 #1 - no conflict (keypad 0x20..0x26,
// AHT10 0x38). Buy PCF8574, NOT PCF8574A (0x38 = AHT10 collision!).
// v2.0 key map (full on-device menu, see display.cpp):
//   2=UP  4=LEFT  6=RIGHT  8=DOWN   1/3/5/7/9/0 = digits (in edit screens)
//   * = HOME     # = BACK              A = ENTER/OK
//   B = MENU     C = MODE (AGARBATTI -> USER -> SILICAGEL)   D = RUN
#define KEYPAD_ENABLED   1
#define KEYPAD_ADDR      0x20

// ---- BOOT button on the devkit = physical start/stop (S3 freebie) ------
// Short press: start / stop the cycle. Long press (2 s): power on after
// DONE/FAULT. The button the board already has - zero wiring.
#define BTN_ENABLED      1
#define PIN_BTN          0           // BOOT button (active LOW)

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