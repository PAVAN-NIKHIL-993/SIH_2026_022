# PIN MAP — Every Component, Every Pin (firmware v2.0)

The single source of truth for wiring. Source: `src/config.h` (classic) and
`variants/esp32-s3/config-s3.h` (S3) — if a number here ever disagrees with
those files, **the config files win**.

**Board note (S3):** DevKitC **N8, N16 or N16R8** all work (v2.0.13) —
the pin map never touches GPIO 35/36/37, which octal-PSRAM (R8) boards
use internally. (Opto → GPIO 18, DHT11 → GPIO 41, door → GPIO 21.)

---

## 1. Master table — ESP32-S3 DevKitC (recommended product board)

| Component | Signal | GPIO | Direction | Notes |
|---|---|---|---|---|
| **AHT10 #1 + keypad** (I2C0 `Wire`) | SDA | **8** | in/out | pull-ups on the modules |
| | SCL | **9** | out | |
| **DHT22** (chamber source #2, cool return) | DATA | **10** | in (one-wire) | +10 k pull-up to 3V3 · replaces AHT10 #2 (v2.0.9) — I2C1 free, GPIO 11 spare |
| | SCL | **11** | out | |
| **HX711 weigh scale** | PD_SCK (CLK) | **1** | out | 2 Hz reads, bit-banged |
| | DOUT | **2** | in | |
| **BTS7960 heater driver** | RPWM | **12** | out (PWM 1 kHz) | through the 555 level shifter |
| | R_EN + L_EN (jumpered) | **13** | out | HIGH = driver enabled |
| | LPWM | — (GND) | — | hard-tied to GND at the module |
| **L298N fan driver** (outlet only) | ENB | **17** | out (PWM 1 kHz) | **remove the ENB jumper** · IN3→5V + IN4→GND tied on the module (fixed dir, v2.0.10) · 10 k ENB→GND · 18 = opto, 21 = door (current build) |
| | IN3 | — (module: 5V) | — | fixed direction, v2.0.10 |
| | IN4 | — (module: GND) | — | fixed direction, v2.0.10 |
| | ENA / IN1 / IN2 | — | — | intake channel removed in v2.0 |
| **Battery sense** | divider output | **4** | in (ADC1_CH3) | 100 k/15 k divider, WiFi-safe |
| | divider gate (2N7000) | **5** | out | samples only while reading |
| **Supply relay 2-ch** | CH1 (solar feed) | **6** | out | never both channels on |
| | CH2 (bypass feed) | **7** | out | MCU switches only with loads quiet |
| **Buzzer KY-012** | signal | **38** | out | active module, active-HIGH |
| **Door limit switch** (no lock fitted) | contact | **21** | in (pull-up) | closed = LOW; v2.0.20 remap from 34 (header doesn't break out 22–34) |
| **P-MOSFET power latch** | hold | **14** | out | HIGH = stay powered (asserted at boot) |
| **BUTTON-1** (master power) | sense | **15** | in (pull-up) | to GND; 3 s = hard off, 10 s = reboot |
| **BUTTON-2** (default automation) | sense | **16** | in (pull-up) | to GND; press = AGARBATTI + start |
| **SOLAR toggle (SPDT)** | position | **39** | in (pull-up) | HIGH = solar requested |
| **Supply optocoupler (PC817)** | output | **18** | in | HIGH = selected feed is live; → PCF#2 P5 later (GPIO 35 = PSRAM on R8 boards — never used) |
| **DHT11 outdoor** | DATA | **41** | in (one-wire) | v2.0.20 remap from 33 (header doesn't break out 22–34); north-side shade |
| **DS1302 RTC** | RST / SCLK / I-O | **40 / 42 / 47** | 3-wire bit-bang | v2.0.21: date & time on the module's CR2032; VCC → 3V3, GND → GND, BZ unused; auto-detected |
| **BOOT button** (on the board) | — | **0** | in (pull-up) | short = start/stop, 2 s = power on |
| **USB** | console/OTA | native USB | — | 115200 baud |

Free pins on the S3: **3, 11** (21 = door, 40/42/47 = DS1302 RTC, 41 = DHT11, 48 = on-board pixel; no display in this build — the website is the screen). GPIO **35/36/37 = PSRAM on R8** boards — never wired. **45/46** strapping — keep free.

## 2. Master table — classic ESP32 DevKit V1 (30-pin)

| Component | Signal | GPIO | Notes vs the S3 |
|---|---|---|---|
| **AHT10 #1 + keypad** (I2C0) | SDA / SCL | **21 / 22** | |
| **AHT10 #2** (I2C1) | SDA / SCL | **32 / 33** | |
| **BTS7960** | RPWM / EN | **25 / 26** | same scheme (LPWM → GND) |
| **L298N** (outlet fan) | ENB | **14** | IN3/IN4 tied on the module (5V/GND); ENA/IN1/IN2 unused |
| **Battery sense** | ADC / gate | **36 / 19** | GPIO36 = ADC1, input-only |
| **Buzzer** | signal | **13** | pin freed by removing the intake fan |
| **P-MOSFET latch hold** | hold | **16** | |
| **BUTTON-1 / BUTTON-2** | sense | **15 / 18** | |
| **SOLAR toggle** | position | **27** | |
| **Supply optocoupler** | output | **35** | input-only GPIO |
| **Supply relay CH1/CH2** | — | **not wired** | no free outputs: mode shown + opto verified, switching is manual |
| **Weigh scale (HX711)** | CLK / DOUT | **12 / 34** | always available — no display in this build |
| **Door limit switch** | — | **not wired** | `DOOR_ENABLED 0`: the calibrate→load→ready workflow runs in software (Start still refused until calibration) |
| **BOOT-button start/stop** | — | **not available** | S3 feature only |

## 2b. PCF8574 compatibility — what can move off the S3

A PCF8574 does slow digital only: inputs and on/off outputs at I2C speed
(~0.2–0.3 ms per update). It cannot PWM, measure analog, or time
microseconds. Rule of thumb: **clicks, beeps, switches, presses → PCF;
humming (PWM), measuring (ADC), µs-timing → stays on the chip.**

| Verdict | Components (current S3 pin) |
|---|---|
| ✅ fully PCF-able | keypad (already PCF #1 @0x20) · supply relays CH1/CH2 (6, 7) · buzzer (38) · door limit switch (21) · BUTTON-1/2 (15, 16) · SOLAR toggle (39) · supply optocoupler (18) · status LEDs · (a future door lock) |
| ⚠️ with compromise | outlet fan ENB (17): on/off only (v2.0 burst law is 100 %-or-off by default) — loses 0–100 % trim + kick-start · BTS7960 EN (13): works, keep with RPWM |
| ❌ never | BTS7960 RPWM (12, 1 kHz PWM) · battery ADC (4) + gate (5) · HX711 (1/2, µs bit-bang) · latch hold (14, boot-critical) · AHT10s (I2C devices) |

**Front panel on ONE mixed PCF (user decision, no PCF #3):** ONE mixed
PCF #2 @0x21 carries the whole front panel — P0/P1 supply relays
(active-LOW module, boot-safe), P2/P3 BUTTON-1/2, P4 toggle, P5 opto,
P6 door switch, P7 buzzer wired low-side (+ to 3V3 — PCF sinking =
beep, boot = silent). 10 kΩ pull-ups on all pins; ₹80, one chip cleans
the whole panel loom, no timing-critical pin touched — frees
6/7/38/15/16/39/18/21 as spares (33/34 aren't exposed on this header;
and with no display in the build 3/11 are spare too —
41 = DHT11, 48 = pixel; the DS1302 RTC is an S3-only part).



## 3. Bus addresses & shared-bus rules

| Bus | Devices | Addresses |
|---|---|---|
| I2C0 (S3: 8/9 · classic: 21/22) | AHT10 #1 + PCF8574 keypad | 0x38 (fixed) + **0x20** |
| I2C1 (classic: 32/33) | AHT10 #2 (classic only) | 0x38 — on the S3 the bus is FREE (chamber #2 = DHT22 on GPIO10) |

- **Buy PCF8574, never PCF8574A** — the A version answers on 0x38 = AHT10 collision.
- AHT10 address is fixed; two sensors can never share one bus — that's why the S3 chamber source #2 is a DHT22 (v2.0.9), not a second AHT10.

## 4. Polarities & levels (all in config.h)

| Item | Setting |
|---|---|
| Buzzer | active-HIGH (`BUZZER_ACTIVE_HIGH 1`) |
| Door lock | energized = LOCKED (`DOOR_LOCK_ACTIVE HIGH`) |
| Door switch | magnet near / closed = **LOW** (`DOOR_CLOSED_LEVEL LOW`), internal pull-up |
| Optocoupler | HIGH = feed present (`OPTO_ACTIVE_HIGH 1`) |
| Relay modules | ACTIVE-HIGH (`RELAY_ACTIVE_LOW false`) — flip for blue modules |
| Buttons / toggle / BOOT | to GND, internal pull-ups (LOW = pressed / solar) |
| Fan direction | IN3=H + IN4=L = blow OUT — swap the pair if it spins backwards |

## 5. Power wiring (not GPIO)

```
solar panel ── MPPT ── battery (4S LiFePO4, 50 A BMS)
                      └─ 50 A fuse ── P-MOSFET latch ──┬─ BTS7960 B+/B− ── coil (0.32 Ω)
                                                    ├─ L298N +12 V ── outlet fan
                                                    └─ 5 V buck (set 5.0 V!) ── ESP32 + keypad + HX711 + relays
supply relay CH1: battery/solar feed ── chamber power bus
supply relay CH2: bypass supply   ── chamber power bus   (never both)
optocoupler LED side: across the SELECTED feed
```

- 555 level shifter: ESP RPWM pin → 555 pin 4, 555 pin 3 → BTS RPWM (3.3 V → 5 V PWM).
- BTS logic VCC = 5 V from the buck; LPWM tied to GND at the module.
- **All grounds common at one star point**: ESP GND = BTS GND = L298N GND = buck GND = battery −.
- Solenoid = 12 V from the buck-adjacent rail through its driver (flyback diode across the coil).

## 6. What changed in v2.0 (pin history)

| Pins | v1.x | v2.0 |
|---|---|---|
| S3 14/15/16 | L298N intake (ENA/IN1/IN2) | latch hold / BUTTON-1 / BUTTON-2 |
| Classic 13/15/18 | L298N intake | buzzer / BUTTON-1 / BUTTON-2 |
| Classic 16 | L298N IN2 | P-MOSFET latch hold |
| Fans | intake + exhaust, continuous RH-band law | ONE outlet fan, burst law (RH ≥ 60 % 1 min → 100 % 60 s) |
| Classic 27/35 | unused | solar toggle / supply optocoupler |

Everything else (scale, door, sensors, BTS, battery) is unchanged from v1.8 — a v1.8-built S3 board only needs the three intake wires re-purposed (and can un-wire the old display: the website is the screen now).
