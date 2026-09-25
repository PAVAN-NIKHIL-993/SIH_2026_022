# ESP32-S3 VARIANT — the commercial-pillar MCU

Same firmware, same website, same safety chain — on the MCU the product
should ship with. Build: `node tools/single-file/assemble.js s3` →
`arduino-ide/SMART-DEHUMIDIFIER-s3-single-file/SMART-DEHUMIDIFIER-s3-single-file.ino`.

## Why the S3 (what you gain)

| | Classic ESP32 | **ESP32-S3** |
|---|---|---|
| GPIO budget | tight — display SHARES pins with relays/buzzer | **every module gets its own pins** |
| RAM (curve log) | 6 h in RAM | **18 h in RAM** (512 KB SRAM) |
| USB | via USB-UART chip | **native USB-C** — serial + flashing, no driver |
| Physical start/stop | needs keypad | **the BOOT button on the devkit** (zero wiring) |
| Serial console | yes (shared feature) | yes — over the same USB-C |
| Future | — | touch buttons, PSRAM (BLE deliberately DISABLED for power, v2.0.18) |

## Board settings (Arduino IDE 2)

1. Boards Manager → "esp32 by Espressif Systems" (same package as before)
2. Tools → Board → **ESP32S3 Dev Module**
3. Tools → **USB CDC On Boot: Enabled** ← serial over USB-C
4. Tools → Flash Size: **16 MB** · PSRAM: **Disabled**
   (the firmware never uses PSRAM — your chip test reported "PSRAM: None"
   even though the board is sold as N16R8; that is fine, nothing changes.
   GPIO 35/36/37 stay untouched either way, so an R8 with working PSRAM
   is equally happy)
5. Port → the USB-C port → Upload. OTA works exactly as before.

## Pin map (S3 — exactly what `config-s3.h` compiles, v2.0.21)

| GPIO | Function | | GPIO | Function |
|---|---|---|---|---|
| 1 / 2 | HX711 CLK / DOUT — weigh scale (dry-to-weight) | | 4 / 5 | battery ADC (ADC1_CH3) + divider gate |
| 8 / 9 | I2C0 SDA/SCL — AHT10 #1 + PCF keypad (0x20) + EEPROM (0x50) | | 10 | DHT22 DATA — chamber source #2 (+10k pull-up) |
| 12 / 13 | BTS7960 RPWM (via 555 shifter) / EN | | **41** | DHT11 DATA — outdoor, in shade (v2.0.20: was 33) |
| 14 | P-MOSFET latch hold (HIGH = stays on) | | **21** | door limit switch, closed = LOW (v2.0.20: was 34) |
| 15 / 16 | BUTTON-1 (master power) / BUTTON-2 (default automation) | | 39 | solar/bypass toggle |
| 17 | L298N ENB — exhaust fan PWM (direction hard-wired on the module) | | 18 | supply optocoupler OUT |
| 6 / 7 | supply relay CH1 (solar) / CH2 (bypass) | | 38 | buzzer KY-012 |
| **40 / 42 / 47** | **DS1302 RTC** RST / SCLK / I-O — date & time on CR2032 (v2.0.21; auto-detected) | | **0** | **BOOT button = start/stop** (on-board) |
| free | **3, 11** — **GPIO 35/36/37 = PSRAM, never wire** | | 48 | WS2812 status pixel (on-board) |

**Why 41 and 21 (v2.0.20 remap):** many S3 devkit headers (the pillar's
included) do NOT break out GPIO 22–34. The outdoor DHT11 therefore moved
**33 → 41** and the door reed **34 → 21**. Nothing else moved. The door is
a LIMIT SWITCH ONLY (no solenoid) — start is gated in software,
mid-cycle open = FAULT.

## Helpful features in this variant (and shared)

- **BOOT button** (GPIO0, already on the devkit): short press = start/stop,
  long press (2 s) = power on after DONE/FAULT. `[btn]` line at boot.
- **Every module on its own pins** — supply relays 6/7, buzzer 38, keypad
  on I2C, pixel 48: nothing shares, nothing gets sacrificed.
- **USB/serial command console** (both variants): type `help` in the serial
  monitor — `start stop power stat | temp 60 | time 90 | knob 100 |
  defaults`. Service and demos without touching the phone.
- 18 h curve log in RAM (LOG_MAX 6480) — a full day of batches on one graph.

## Wiring deltas vs the classic build

Only the ESP32 side changes: I2C0 → 8/9, battery → 4/5, scale → 1/2,
BTS → 12/13, latch/buttons → 14/15/16, L298N → 17 (ENB only),
opto → 18, door → 21, DHT22 → 10, DHT11 → 41, relays → 6/7, toggle → 39,
buzzer → 38, pixel → 48. Every module, coil, fan and sensor is identical —
swap the ESP32, re-home ~15 wires, done. The v2.1 front-panel plan
(relays / buzzer / buttons / toggle / opto / door on one PCF#2) is
documented in `docs/PINS-S3-PCF2.md` — when it lands, 6/7/15/16/18/21/
38/39 all free up.

*(Everything else — control logic, website, safety chain, OTA, docs — is
shared from `src/`; this folder only owns the config.)*