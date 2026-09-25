#!/usr/bin/env node
/**
 * assemble.js - generates the SINGLE-FILE Arduino IDE sketch from the
 * verified multi-file sources (src/). Run after any change to src/:
 *
 *     node tools/single-file/assemble.js            # classic ESP32
 *     -> arduino-ide/smart-dryer-single-file/smart-dryer-single-file.ino
 *     node tools/single-file/assemble.js s3         # ESP32-S3 variant
 *     -> arduino-ide/smart-dryer-s3-single-file/smart-dryer-s3-single-file.ino
 *
 * Variants share ALL logic from src/; only the config differs
 * (src/config.h vs variants/esp32-s3/config-s3.h).
 * The output is one self-contained .ino with ZERO external libraries:
 * install only the "esp32 by Espressif Systems" board package and upload.
 */
'use strict';
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '../..');
const src = f => path.join(root, 'src', f);

// ---- variant: 'classic' (default) or 's3' --------------------------------
const variant = process.argv[2] === 's3' ? 's3' : 'classic';
const cfgPath = variant === 's3'
  ? path.join(root, 'variants/esp32-s3/config-s3.h')
  : src('config.h');
const cfgLabel = variant === 's3' ? 'variants/esp32-s3/config-s3.h' : 'src/config.h';
const outDir  = variant === 's3' ? 'smart-dryer-s3-single-file' : 'smart-dryer-single-file';
const outFile = variant === 's3' ? 'smart-dryer-s3-single-file.ino' : 'smart-dryer-single-file.ino';
const mcuLine = variant === 's3'
  ? ' *  VARIANT: ESP32-S3  (board "ESP32S3 Dev Module", USB CDC On Boot: Enabled)'
  : ' *  VARIANT: classic ESP32 DevKit V1  (board "ESP32 Dev Module")';

// local headers whose #include lines get stripped (order provides them)
const localHeaders = new Set([
  'config.h', 'pwm.h', 'aht10.h', 'sensors.h', 'battery.h', 'buzzer.h', 'eelog.h', 'rtc.h',
  'keypad.h', 'display.h', 'scale.h', 'door.h', 'supply.h', 'menu.h', 'txdisp.h', 'dht.h', 'pixel.h', 'buzzer.h', 'control.h', 'cyclelog.h', 'webui.h', 'web.h',
]);

function clean(code) {
  return code
    .split('\n')
    .filter(l => {
      const t = l.trim();
      if (t === '#pragma once') return false;
      const m = t.match(/^#include\s+"([^"]+)"/);
      if (m && localHeaders.has(m[1])) return false;
      return true;
    })
    .join('\n');
}

// ---- PIN QUICK-REFERENCE: parsed from THIS variant's config at build
// time, so the table can never drift from the actual pin map. --------
const cfgSrc = fs.readFileSync(cfgPath, 'utf8');
const defs = {};
for (const m of cfgSrc.matchAll(/^#define\s+(\w+)\s+(-?\d+)/gm)) defs[m[1]] = parseInt(m[2], 10);
const pinRows = [
  ['PIN_I2C0_SDA', 'I2C0 SDA  - AHT10 #1 + keypad backpack'],
  ['PIN_I2C0_SCL', 'I2C0 SCL'],
  ['PIN_I2C1_SDA', 'I2C1 SDA  - AHT10 #2 (own bus, fixed 0x38)'],
  ['PIN_I2C1_SCL', 'I2C1 SCL'],
  ['PIN_SCALE_CLK', 'HX711 weigh scale PD_SCK'],
  ['PIN_SCALE_DOUT', 'HX711 weigh scale DOUT'],
  ['PIN_BTS_RPWM', 'BTS7960 RPWM (via the 555 level shifter)'],
  ['PIN_BTS_EN', 'BTS7960 R_EN + L_EN (jumpered)'],
  ['PIN_L298_ENB', 'L298N ENB - the ONE outlet fan PWM'],
  ['PIN_VBAT_ADC', 'battery divider -> ADC'],
  ['PIN_VBAT_ENABLE', 'battery divider gate (2N7000)'],
  ['PIN_SUPPLY_CH1', 'supply relay CH1 - solar feed'],
  ['PIN_SUPPLY_CH2', 'supply relay CH2 - bypass feed'],
  ['PIN_SOLAR_TOGGLE', 'SOLAR/BYPASS toggle (HIGH = solar)'],
  ['PIN_SUPPLY_OPTO', 'supply optocoupler (HIGH = feed live)'],
  ['PIN_POWER_HOLD', 'P-MOSFET latch hold (hard power-off)'],
  ['PIN_BTN1', 'BUTTON-1: 3 s hard off / 10 s reboot'],
  ['PIN_BTN2', 'BUTTON-2: default automation'],
  ['PIN_DOOR_LOCK', 'door solenoid lock (OPTIONAL - not fitted)'],
  ['PIN_DOOR_REED', 'door limit switch / reed (closed = LOW)'],
  ['PIN_BUZZER', 'buzzer KY-012'],
  ['PIN_TXDISP_TX', 'display bridge TX -> companion Arduino'],
  ['PIN_DHT22_CHAMBER', 'DHT22 chamber sensor (cool-return path)'],
  ['PIN_DHT11_OUT', 'DHT11 outdoor sensor (shade!)'],
  ['PIN_PIXEL', 'RGB status pixel (on-board WS2812; 48=v1.0 boards, 38=v1.1)'],
  ['PIN_RTC_RST', 'DS1302 RTC RST (chip enable) - date & time, coin-cell'],
  ['PIN_RTC_SCLK', 'DS1302 RTC SCLK'],
  ['PIN_RTC_IO', 'DS1302 RTC data I/O'],
  ['PIN_TFT_SCK', 'ILI9488 TFT SCK'],
  ['PIN_TFT_MOSI', 'ILI9488 TFT MOSI'],
  ['PIN_TFT_CS', 'ILI9488 TFT CS'],
  ['PIN_TFT_DC', 'ILI9488 TFT DC'],
  ['PIN_TFT_RST', 'ILI9488 TFT RST'],
  ['PIN_BTN', 'BOOT button: start/stop (S3 boards)'],
];
// pins of modules that are compiled OUT on this variant are not wired:
const gated = {
  PIN_SCALE_CLK: 'SCALE_ENABLED', PIN_SCALE_DOUT: 'SCALE_ENABLED',
  PIN_DOOR_LOCK: 'DOOR_LOCK_ENABLED',  PIN_DOOR_REED: 'DOOR_ENABLED',
  PIN_SUPPLY_CH1: 'SUPPLY_RELAYS_ENABLED', PIN_SUPPLY_CH2: 'SUPPLY_RELAYS_ENABLED',
  PIN_TXDISP_TX: 'TXDISP_ENABLED',
  PIN_DHT22_CHAMBER: 'DHT_CHAMBER_ENABLED',
  PIN_DHT11_OUT: 'DHT_OUT_ENABLED',
  PIN_PIXEL: 'PIXEL_ENABLED',
  PIN_RTC_RST: 'RTC_ENABLED', PIN_RTC_SCLK: 'RTC_ENABLED', PIN_RTC_IO: 'RTC_ENABLED',
};
let pinRef = '';
for (const [macro, desc] of pinRows) {
  if (!(macro in defs) || defs[macro] < 0) continue;
  if (gated[macro] && defs[gated[macro]] === 0) continue;   // module off
  pinRef += ` *    GPIO${String(defs[macro]).padStart(2)}   ${desc}\n`;
}
pinRef += ' *\n';
const notes = [];
if (defs.SCALE_ENABLED === 0) notes.push('weigh scale OFF on this variant (config: SCALE_ENABLED)');
if (defs.DOOR_ENABLED === 0) notes.push('door hardware OFF - the calibrate->load->ready workflow runs in software');
if (defs.DOOR_ENABLED === 1 && defs.DOOR_LOCK_ENABLED === 0) notes.push('door = LIMIT SWITCH ONLY (no lock): start gated in software, mid-cycle open = FAULT');
if (defs.SUPPLY_RELAYS_ENABLED === 0) notes.push('supply relay not wired - mode shown + opto verified, switching is manual');
if (defs.BTN_ENABLED === 1) notes.push('BOOT button = start/stop, hold 2 s = power on');
notes.push(`keypad PCF8574 at 0x${(defs.KEYPAD_ADDR || 0x20).toString(16).toUpperCase()} (never PCF8574A - AHT10 clash)`);
for (const n of notes) pinRef += ` *    NOTE: ${n}\n`;

const banner = `/*
 * ============================================================================
 *  SMART DEHUMIDIFIER - SINGLE-FILE SKETCH (whole project, one file)
 * ============================================================================
 *  Generated from the multi-file sources by tools/single-file/assemble.js -
 *  do not edit by hand; edit src/ and regenerate.
 *
${mcuLine}
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
${pinRef} *
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
 *      WiFi AgarbattiDryer -> Tools > Port -> smart-dryer at 192.168.4.1)
 *    - hotspot website: dashboard + graph + knob, Parameters with
 *      one-click defaults, weight tools, history + CSV download,
 *      phone-synced clock, phone-relayed weather
 *    - serial console 115200: type "help"
 *
 *  Full docs: docs/manual/ (02 wiring, 05 errors, 09 flashing)
 *             + docs/PIN-MAP.md (every component x every pin)
 * ============================================================================
 */
`;

// assembly order: ALL headers (dependency order), then all implementations,
// then main. Every local header MUST be listed - the single-file sketch
// strips #includes, so a missing header is an undeclared-type compile error.
const order = [
  'config.h',
  'pwm.h', 'aht10.h', 'dht.h', 'sensors.h', 'battery.h', 'buzzer.h',
  'eelog.h', 'rtc.h', 'keypad.h', 'scale.h', 'door.h', 'supply.h', 'menu.h',
  'txdisp.h', 'display.h', 'pixel.h', 'control.h', 'cyclelog.h',
  'webui.h', 'web.h',
  'pwm.cpp', 'aht10.cpp', 'sensors.cpp', 'battery.cpp', 'buzzer.cpp', 'eelog.cpp', 'rtc.cpp',
  'keypad.cpp', 'display.cpp', 'scale.cpp', 'door.cpp', 'supply.cpp', 'menu.cpp', 'txdisp.cpp', 'dht.cpp', 'pixel.cpp', 'control.cpp', 'cyclelog.cpp', 'web.cpp',
  'main.cpp',
];

// guard: every stripped local include must be emitted before its includer,
// and no local header may fall through the cracks again.
{
  const pos = {};
  order.forEach((f, i) => {
    if (f in pos) throw new Error(`assemble: ${f} listed twice in order`);
    pos[f] = i;
  });
  for (const f of order) {
    const file = (f === 'config.h') ? cfgPath : src(f);
    for (const m of fs.readFileSync(file, 'utf8').matchAll(/^#include\s+"([^"]+)"/gm)) {
      const h = m[1];
      if (!localHeaders.has(h)) continue;
      if (!(h in pos)) throw new Error(`assemble: ${f} includes ${h}, which is missing from the emission order`);
      if (pos[h] > pos[f]) throw new Error(`assemble: ${h} must be emitted before ${f}`);
    }
  }
  for (const h of localHeaders) {
    if (!(h in pos)) throw new Error(`assemble: local header ${h} is never emitted`);
  }
}

let out = banner;
for (const f of order) {
  const file = (f === 'config.h') ? cfgPath : src(f);
  const label = (f === 'config.h') ? cfgLabel : 'src/' + f;
  out += `\n/* ==========================  ${label}  ========================== */\n`;
  out += clean(fs.readFileSync(file, 'utf8')).replace(/\s+$/, '\n');
}

// EOF marker with exact counts (keeps line-count discussions unambiguous)
const body = out.replace(/\n$/, '');
const stamp = new Date().toISOString().slice(0, 10);
let final = body, lines = 0, nonBlank = 0;
for (let i = 0; i < 3; i++) {                 // converge: marker states its own true size
  lines = final.split('\n').length - (final.endsWith('\n') ? 1 : 0); // wc -l equivalent
  nonBlank = final.split('\n').filter(l => l.trim().length).length;
  final = body + `\n/* ==== END OF FILE ====\n * total lines (wc -l): ${lines}   non-blank lines: ${nonBlank}\n * build ${stamp} - if these numbers differ from what you see,\n * you are looking at an older copy; regenerate: node tools/single-file/assemble.js\n */\n`;
}

const dest = path.join(root, 'arduino-ide', outDir);
fs.mkdirSync(dest, { recursive: true });
const file = path.join(dest, outFile);
fs.writeFileSync(file, final);
console.log('written', file, (final.length / 1024).toFixed(1) + 'KB,',
            lines, 'lines (wc -l),', nonBlank, 'non-blank');