# Sensor test suite — one sketch per sensor

Each folder is a **stand-alone Arduino sketch**: one file, no libraries
beyond the ESP32 core, no `src/` includes. It works the exact same way the
firmware samples that sensor (same pins, same drivers, same debounce/
oversampling), so a PASS here means the sensor will pass in the real build.

| Sketch | Checks | Classic ESP32 pins | S3 pins |
|---|---|---|---|
| `aht10-test` | AHT10 chamber temp/RH (raw I2C, 0x38) | #1: 21/22 · #2: 32/33 | 8/9 (one sensor) |
| `dht-test` | DHT22 chamber + DHT11 outdoor (bit-bang 40-bit frames) | bench override `TEST_DHT_PIN` | DHT22: 10 · DHT11: 41 |
| `scale-test` | HX711 + load cells (bit-bang, ch A x128) | bench override `TEST_SCALE_CLK/DOUT` (12/34 per config.h) | SCK 1 / DOUT 2 |
| `door-test` | door limit switch, debounce + edge count | bench override `TEST_DOOR_PIN` | 34 (closed = LOW) |
| `battery-test` | battery divider + ADC1 (16x oversample, gated) | ADC 36 / EN 19 | ADC 4 / EN 5 |
| `supply-test` | solar toggle + supply optocoupler (read-only) | toggle 27 / opto 35 | toggle 39 / opto 18 |
| `buttons-test` | BUTTON-1 / BUTTON-2 press durations | 15 / 18 (hold 16) | 15 / 16 (hold 14) |
| `keypad-test` | PCF8574 backpack + all 16 keys, with menu meanings | 21/22 @ 0x20 | 8/9 @ 0x20 |
| `eeprom-test` | AT24C256 presence, registry header, write/restore test | 21/22 @ 0x50 | 8/9 @ 0x50 |
| `rtc-test` | DS1302 date & time (bit-bang, scratch-RAM probe, WP/CH flags) | bench override `PIN_RTC_*` | RST 40 / SCLK 42 / I/O 47 |

The board is **detected automatically** — flash the same file on either the
classic ESP32 or the ESP32-S3 and it uses the right pin map.

## The OTA story (why every sketch has it)

Every test starts the same hotspot the dryer uses — **AgarbattiDryer** /
`dryer1234` — and registers **ArduinoOTA under its own name** (`aht10-test`,
`scale-test`, …). So:

1. Flash **any one** sketch over USB (say `aht10-test`).
2. Join the hotspot, then in the Arduino IDE: **Tools → Port →
   `aht10-test at 192.168.4.1`**.
3. From then on you can upload **any other test** — or the main
   `SMART-DEHUMIDIFIER-single-file.ino` / S3 firmware — straight over WiFi.
   The port name changes to match whichever sketch is running, so you
   always know what is on the board.

No USB cable needed for the whole test-and-flash cycle.

## Reading a test

- **Serial** (Monitor @ **115200**) and the **phone page**
  (join AgarbattiDryer → open `http://192.168.4.1`, auto-refresh 1/s)
  show the same data. The page carries a big **verdict**:

  | Verdict | Meaning |
  |---|---|
  | **PASS** | sensor answers and the values behave |
  | **WARN** | chip answers but something is off (stale data, jitter, glitches) |
  | **FAIL** | nothing where the sensor should be |
  | **NOT FITTED** | this board/variant doesn't wire that sensor |

- **Serial commands** (where useful):

  | Sketch | Commands |
  |---|---|
  | `scale-test` | `t` = tare · `c <grams>` = calibrate with a known weight · `f` = show factor/offset |
  | `battery-test` | `s <0-3>` = chemistry (0=3S Li-ion 1=4S Li-ion 2=12V SLA 3=4S LiFePO4) |
  | `eeprom-test` | `h` = re-read registry header · `w` = run write test again |

## Arduino IDE settings (Tools →)

| | classic ESP32 | ESP32-S3 DevKit |
|---|---|---|
| Board | **ESP32 Dev Module** | **ESP32S3 Dev Module** |
| USB CDC On Boot | — (UART bridge) | **Enabled** (native USB) |
| PSRAM | — | **OPI PSRAM** (R8 boards) |
| Port (flash) | USB | native USB |
| Port (OTA) | `xxx-test at 192.168.4.1` | same |

## Safety notes

- The suite only **reads sensors and safe status inputs**. It never drives
  the heater, the fan, or the 2-channel feed relay (on the S3 that would
  switch the chamber's power). `supply-test` watches the toggle/opto only.
- `buttons-test` keeps the P-MOSFET latch alive, so a wired-up harness stays
  powered — and unlike the real firmware, **holding BUTTON-1 does not cut
  the system** in this test.
- `eeprom-test` writes only to byte 32744 (the unused tail of the chip) and
  always restores the original byte. Header and cycle records are never
  touched.
- `scale-test`/`door-test`/`dht-test` have `-1` pin overrides in their
  headers for the classic board, where the dryer build leaves those pins
  free — set them if you want to bench-test a spare sensor.