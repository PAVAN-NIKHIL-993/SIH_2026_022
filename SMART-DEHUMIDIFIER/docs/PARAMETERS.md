# PARAMETERS — the complete reference

Every value you can change, what it does, its range, and what happens when
you push it too far. Runtime parameters are edited on the website
(**Parameters** page — one click "↳ Reset all to defaults" restores factory
values). Compile-time parameters live in `src/config.h` / the `.ino` and
need a re-flash (now possible over the air: website → **⬆ Firmware update**).

---

## A · RUNTIME — website "Parameters" page (saved in NVS, live mid-cycle)

### Temperature

| # | Parameter | Range | Default | What it does / cautions |
|---|---|---|---|---|
| 1 | **Target temperature** `setTemp` | **40–80 °C (v2.0)** | **60 °C** | PID holds the *average* of both sensors here. 45–60 °C = classic slow recipes (fragrance survives best); 70–80 °C = fast agarbatti curing (SILICAGEL = 80). The website/keypad/serial all clamp to 40–80; 95 °C stays as the invisible hardware cut. |
| 2 | **Control band ±** `tempHyst` | 0.2–5 °C | 1.5 °C | Smoothing/switching band. Smaller = tighter hold but busier PWM; bigger = lazier. Informational for the PID, real for boost exit. |
| 3 | **Safety cutoff** `maxTemp` | setTemp+5…110 °C | **95 °C** | HARD stop: heater + fans off, FAULT state. Must stay ≥ 10 °C under the chamber's 100 °C rating. Back it up with a bimetal thermostat. |

### Humidity / ventilation

| # | Parameter | Range | Default | What it does / cautions |
|---|---|---|---|---|
| 4 | **Fans ramp above** `humHigh` | 20–95 %RH | 60 %RH | Above this humidity the fans ramp: duty = fanMin + fanSlope × (RH − humHigh), capped 100 %. Lower it for humid monsoon days. |
| 5 | **Fans stop below** `humLow` | 10–80 %RH | 40 %RH | Below this the fans stop entirely — keeps heat and prevents over-drying (sticks get brittle). Keep humLow < humHigh − 10. |
| 6 | **Target RH** `humTarget` | 5–70 %RH | 35 %RH | The "dry enough" humidity number. |
| 7 | **Wait for target RH** `requireHum` | on/off | off | ON: the cycle will NOT end when time is up until RH also ≤ humTarget (quality gate). OFF: time alone ends it. |

### Time

| # | Parameter | Range | Default | What it does / cautions |
|---|---|---|---|---|
| 8 | **Drying time** (hours + minutes) `dryMinutes` | 1 min–12 h | 120 min | Set manually — the cycle ends by TIME (optionally + RH gate), then purge, then power off. +15 min button extends live. |

### Fans

| # | Parameter | Range | Default | What it does / cautions |
|---|---|---|---|---|
| 9 | **Minimum fan speed** `fanMin` | 0–60 % | 20 % | Circulation speed inside the RH band (gentle, keeps air moving without heat loss). |
| 10 | *(legacy)* **Intake fan scaling** `fanIn` | 10–100 % | 100 % | Unused in v2.0 (the intake fan was removed); kept so old settings load. |
| 11 | **Exhaust fan scaling** `fanOut` | 10–100 % | 100 % | Same for exhaust. Exhaust ≥ intake keeps the chamber slightly negative (no scent leaks). |
| 12 | **Ramp per RH point** `fanSlope` | 1–12 %/RH | 6 %/RH | How aggressively fans spin up past humHigh. High slope + small chamber = overshoot oscillation. |

### Heater

| # | Parameter | Range | Default | What it does / cautions |
|---|---|---|---|---|
| 13 | **Heater power cap** `heaterMax` | 10–100 % | **100 % = full 500 W** | Soft cap on coil duty — boost, PID and the manual knob all obey it. Reduce only to tame a hot chamber; the BTS7960 (43 A, heatsink fitted) is sized for 100 %. |
| 14 | **Full-power heat-up** `boostHeat` | on/off | on | ON: BTS at cap until chamber reaches target − band, then PID. OFF: PID from cold (slow first hour). |
| 15 | **Kp** | 0–100 %/°C | 10 | Proportional gain: how hard duty reacts to the error. Too high = temperature oscillation. |
| 16 | **Ki** | 0–10 %/(°C·s) | 0.2 | Integral: removes steady-state offset. Too high = slow overshoot waves. |
| 17 | **Kd** | 0–100 | 5 | Derivative: damps approaching the target. Raise if it overshoots > 3 °C. |

### Safety / battery

| # | Parameter | Range | Default | What it does / cautions |
|---|---|---|---|---|
| 18 | **Purge before off** `cooldownSec` | 10–600 s | 45 s | Fans at 100 % flush hot/moist air before power-off (protects the chamber + fans from heat soak). |
| 19 | **Battery type** `battType` | 0–3 | **3 = 4S LiFePO4** | 0=3S Li-ion · 1=4S Li-ion · 2=12 V SLA · 3=4S LiFePO4 — sets the voltage→% curve. **Recommended: 12.8 V LiFePO4 50–100 Ah with ≥ 50 A BMS** (2000+ cycles, safest chemistry); SLA works but 300–500 cycles and must deliver ~42 A. |
| 20 | **Safe shutdown below** `cutoffPct` | 0–40 % | 10 % | Battery at/below this % → FAULT + safe shutdown (protects the cells). |

### Dry to weight (weigh scale — the batch is its own calibration)

| # | Parameter | Range | Default | What it does / cautions |
|---|---|---|---|---|
| 21 | **Dry to weight** `requireWeight` | on/off | off | ON: the cycle also ends when the batch stops losing weight — no more time-guessing. Needs the HX711 scale (S3 variant: GPIO 1 CLK + 2 DOUT; classic ESP32 has no free output pin with the display on — options in config.h). Scale absent + ON → warns once, falls back to time+RH. |
| 22 | **Settled below** `weightRateG` | 0.5–50 g/min | 2 g/min | Weight-loss rate under this = "dry enough". Too low = keeps running after dry; too high = ends early. |
| 23 | **Stable for** `weightMinY` | 2–120 min | 10 min | How long the rate must stay low before ending (filters fan vibration / tray bumps). |

*Scale tools (dashboard):* **Tare** = zero now; **Calibrate** = put a known
weight (e.g. 1000 g) on the trays, type its grams, press — factor + tare
are saved in NVS. `[stat]` prints `wt=1234g/1.2g/min`; the cycle CSV gains
a `wt_g` column (blank on runs without a scale).

Settings store version is now **v6** (+5 scale fields): after flashing this
release, press **Reset all to defaults** once so old v5 blobs migrate.

### Dashboard-only controls (not in the form)

| Control | What it does |
|---|---|
| **Start / Stop** | manual cycle control (AUTO_START off by default) |
| **+15 min** | extends a running cycle |
| **Power On** | re-energise after DONE/FAULT |
| **Manual heat knob** | exact BTS duty for 60 s, then automatic control returns on its own; capped by heaterMax; never overrides a safety cut |
| **🎨 background picker** | 6 presets + custom colour + hair-lines on/off (saved per phone) |
| **⬆ Firmware update** | OTA: upload a compiled `.bin` over the hotspot; refused while RUNNING |

---

## B · COMPILE-TIME — `src/config.h` (re-flash to change; OTA makes this easy)

**Two variants, one codebase:** classic ESP32 (`src/config.h`) and
**ESP32-S3** (`variants/esp32-s3/config-s3.h` — every module gets its own
pins, 18 h RAM log, BOOT-button start/stop; see `variants/esp32-s3/README.md`).
Pin tables below are the CLASSIC build; the S3 map lives in the variant.

### Pins (FROZEN to the built board — see manual 10 §5 for the wire schedule)

| Define | GPIO | Function |
|---|---|---|
| `PIN_I2C0_SDA/SCL` | 21 / 22 | AHT10 #1 (top) |
| `PIN_I2C1_SDA/SCL` | 32 / 33 | classic: AHT10 #2 · **S3: bus free** (chamber #2 = DHT22) |
| `PIN_BTS_RPWM` | 25 | heater PWM → 555 shifter → RPWM |
| `PIN_BTS_LPWM` | −1 | tied to GND at the module |
| `PIN_BTS_EN` | 26 | BTS R_EN + L_EN |
| `PIN_L298_ENA` / `PIN_L298_IN1/IN2` | −1 / −1 | intake channel removed (v2.0) |
| `PIN_L298_ENB` | 14 (S3: 17) | outlet fan PWM — the ONLY wired L298N pin |
| `PIN_L298_IN3/IN4` | −1 / −1 | direction hard-wired on the module: IN3→5V, IN4→GND (v2.0.10); 18 = opto, 21 = door (current build) |
| `FAN_FIXED_DIR` / `FAN_PWM_FLOOR` | **1 / 40 %** | ENB-only PWM: kick-start spins the fan up, the 40 % floor stops the low-duty hum |
| `PIN_VBAT_ADC` | 36 | battery divider |
| `PIN_VBAT_ENABLE` | 19 | divider gate |
| `PIN_LOAD_RELAY` | 23 | load relay (when fitted) |
| `PIN_BYPASS_CTRL` | 2 | bypass relay (when fitted) |
| `PIN_BUZZER` | 13 (S3: 38) | buzzer (fitted by default; S3 moves to PCF#2 P7 with the front panel) |
| `PIN_TFT_*` / `PIN_TXDISP_TX` | (unused) | display removed (v2.0.7+): the website is the screen; set `DISPLAY_ENABLED 1` to revive |
| `KEYPAD_ADDR` | 0x20 | PCF8574 keypad on Wire |
| `DOOR_ENABLED` | 0 / **1 (S3)** | door limit switch (S3: GPIO21) |
| `DOOR_LOCK_ENABLED` | **0** | no solenoid lock fitted — start gated in software; set 1 + a free GPIO (3/11) if a lock is ever added |
| `PIN_POWER_HOLD` / `PIN_BTN1` / `PIN_BTN2` | 16/15/18 classic · 14/15/16 S3 | v2.0 power: latch hold, master button (3 s off / 10 s reboot), default-automation button |
| `PIN_SOLAR_TOGGLE` / `PIN_SUPPLY_OPTO` / `PIN_SUPPLY_CH1/2` | 27/35/− classic · 39/**18**/6+7 S3 | supply selector: toggle, live-check, feed relay (opto on 18 = R8-safe, v2.0.13) |
| `OTA_NETWORK_ENABLED` / `OTA_HOSTNAME` | **1** / "SMART-DEHUMIDIFIER" | Arduino-IDE network OTA over the dryer hotspot (website OTA at `/update` is always available too) |
| `DISPLAY_ENABLED` / `TXDISP_ENABLED` | **0 / 0** | v2.0.7: the website is the display — `/` full control + `/display` kiosk page; TFT + UNO bridge compile out (3, 11 free; 40/42/47 = DS1302 RTC v2.0.21; 41 = DHT11, 48 = pixel) |
| `SENS2_DHT` / `DHT_CHAMBER_ENABLED` / `PIN_DHT22_CHAMBER` / `DHT_CHAMBER_MS` | **1 / 1 / 10 / 3 s (S3)** | chamber source #2 = **DHT22 on the cool-return path** — averages, RH-peak and safety cut keep working with one AHT10 fewer (classic: 0 = 2× AHT10) |
| `DHT_OUT_ENABLED` / `PIN_DHT11_OUT` / `DHT_READ_MS` | **1 / 41 / 10 s (S3)** | **DHT11 outdoor** (shade): real-time smart venting, web OUT line + weather card, CSV columns. GPIO 41 free (v2.0.20: moved off 33 — not broken out on many devkit headers) — works on N8 and N16R8 |
| `RTC_ENABLED` / `PIN_RTC_RST` / `PIN_RTC_SCLK` / `PIN_RTC_IO` | **1 / 40 / 42 / 47 (S3)** · 0 / −1 / −1 / −1 classic | **DS1302 real-time clock** (v2.0.21): date & time on the module's CR2032 — survives a full power-down; auto-detected (scratch-RAM magic probe); every real time set (phone sync, keypad menu, serial `rtcset`) is mirrored to the chip; no chip = the old phone-sync + NVS clock |
| `DOOR_LOCK_ACTIVE` / `DOOR_CLOSED_LEVEL` | HIGH / LOW | driver polarity — flip if your lock board is active-LOW |

### Feature switches

| Define | Default | Effect |
|---|---|---|
| `RELAYS_ENABLED` | 0 | 1 = drive load/bypass relays (fit hardware first) |
| `BUZZER_ENABLED` | 0 | 1 = beep patterns on start/stop/done/fault |
| `DISPLAY_ENABLED` | 1 | 3.5" status display (0 = free GPIO 12/0/2/17/23) |
| `KEYPAD_ENABLED` | 1 | hex keypad on PCF8574 |
| `AUTO_START` | false | true = one drying cycle starts by itself 15 s after every power-up (plug-and-play) |
| `DRYER_RTOS` | 0 | 1 = FreeRTOS task architecture instead of the cooperative loop |

### Timing / behaviour

| Define | Default | Effect |
|---|---|---|
| `AUTO_START_DELAY_MS` | 15000 | grace before an auto-started cycle |
| `MANUAL_HEAT_MS` | 60000 | how long a knob turn lasts before auto-release |
| `DIAG_PERIOD_MS` | 15000 | `[stat]` serial line interval |
| `TIME_SAVE_MS` | 30 min | wall-clock persistence to NVS; restored at boot after a full power-down (stale until the first phone sync) |
| `cfg.tzMinutes` | **330** (IST) | wall-clock timezone (minutes). Set from the site Clock card (auto phone push or manual picker), the `/display` page (tap the clock) or keypad menu rows **6/7** (YYMMDD / HHMM) |
| `targetG` | 0 (off) | **target batch weight (g)**: within 5 % at time-up = complete, else DONE-WITH-WARNING (warned 5 min before end with a suggested +min) |
| `fanTrigRH` / `fanTrigMin` / `fanBurstS` | 60 / 1 / 60 | v2.0 burst venting: RH ≥ 60 % for 1 min → outlet fan 100 % for 60 s |
| `mode` | 0 | 0 AGARBATTI (60 °C) · 1 USER DEFINED · 2 SILICAGEL (80 °C) — C key / BUTTON-2 / website / serial `mode` |
| `SENSOR_PERIOD_MS` | 2000 | AHT10 poll rate |
| `SENSOR_FAIL_GRACE` | 15000 | both sensors dead this long → FAULT |
| `AHT10_HOT_C` | 82.0 | `[warn] AHT10 #n HOT` threshold |
| `LOG_PERIOD_MS` / `LOG_MAX` | 10000 / 2160 | RAM curve: one record per 10 s, 6 h |
| `WX_STALE_MS` | 2 h | phone-relayed weather freshness window |
| `HEATER_PWM_FREQ` / `FAN_PWM_FREQ` / `PWM_RES_BITS` | 1 kHz / 1 kHz / 10-bit | LEDC setup (100 % = 1023/1023 — verified true full power) |
| `AP_SSID` / `AP_PASS` | AgarbattiDryer / dryer1234 | hotspot credentials |

### Battery divider calibration

| Define | Default | Effect |
|---|---|---|
| `VBAT_DIV_RTOP/RBOT` | 100 k / 15 k | physical divider resistors |
| `VBAT_ADC_REF` | 3.30 | measure your 3V3 rail once with a DMM and enter it |
| `VBAT_CAL_OFFSET` | 0.0 | volts added to the computed reading (fine trim) |

---

## Batch calculator — sticks & paste (owner spec, v2.0.16)

Moisture is a property of the **paste** — raw agarbatti mix is typically
**30–40 % water**, finished sticks want **8–10 % residual moisture**. So
the calculator lives in Parameters (site → Parameters → Batch calculator;
keypad users type the final target directly):

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| `stickCount` | **0 (off)** | 0–3000 | sticks in this batch; 0 = type the target by hand |
| `stickWetG` | 2.5 | 0.5–20 g | average WET stick mass |
| `pasteWaterPct` | **35** | 5–60 % | water content of the raw paste (per recipe!) |
| `targetMoistPct` | **10** | 3–20 % | residual moisture wanted in the finished stick |

**1. Target weight per stick:**
`dry = wet × (1 − (pasteWater% − finalMoist%) / 100)`
Example: 2.5 g stick, 35 % → 10 %: `2.5 × (1 − 0.25) = 1.875 g` — each
stick must lose **0.625 g** of water. With `stickCount > 0` the firmware
computes the batch target automatically (`applySettings`), the site shows
the full preview, and **E17 cross-checks it against the measured batch at
START** — if the paste was wetter than the parameter says, the start is
refused with E17 (fix `pasteWaterPct` and retry).

**2. Total water per batch (heater/dehumidifier sizing):**
`D = N × (M_wet − M_dry)` — the site preview shows this live ("total
water to remove: X g / L"). Example: 400 sticks × 0.625 g = **250 g**
per batch. Rule of thumb: ~0.63 kWh evaporates 1 kg of water, so a
250 g batch needs ≈ 0.16 kWh — about 6 min of the 500 W coil; the rest
of the cycle is distribution/conditioning.

**3. RH by dry-bulb/wet-bulb (reference for MANUAL chambers):**
`RH ≈ 98 − ((T_dry − T_wet) / T_dry) × 300` with temperatures in **°F**
(industrial shorthand). Example: 113 °F / 95 °F → 98 − 47.8 ≈ **50 % RH**.
This dryer does NOT need it — the AHT10/DHT22 measure RH directly; the
formula is kept for checking a manual sling psychrometer against the
machine (and for chambers without sensors).

> Settings store bumped to **v8** — a v7 unit picks up factory defaults
> once after updating (re-enter your parameters).

## Indication: 37-pattern buzzer engine + RGB status pixel (owner spec, v2.0.15)

`src/buzzer.*` is a pattern/repeat engine: `bz::play()` one-shots (higher
priority preempts), `bz::startRepeat()` nags every period until stopped.
~30 of the 37 owner-spec alerts are live (single-tone active buzzer —
cadence carries the identity; the 13-frequency palette needs a passive
piezo): key click #1 · invalid #2 · hold ticks #3 · mode #4 · saved #5 ·
back #6 · door-ajar-at-START #7 · no-trays #8 · unstable weight #9 ·
ready #11 · battery-low-at-start #12 · battery-critical #13 · setpoint
reached #14 · fan-on #15 · door-open-mid-cycle #16 · 50 % moisture #17 ·
target-approaching #18 · 5-min-to-timeout #19 · brownout-resume #21 ·
weight anomaly #22 · warning #23 · critical #24 · recovered #25 ·
load-cell #26 · AP-up #27 · client-joined #28 · log-saved #29 ·
storage-90 % #30 · maintenance/50-cycles #33 · init-fail #34 ·
cool-down #35 · shutdown #36 · factory-reset #37. n/a by design: #10
(per-tray prompts), #20 (no pause state), #31/#32 (no charge sensing).

**RGB status pixel** (`src/pixel.*`, on-board WS2812, GPIO48, driven via
hardware SPI — zero libraries): boot blue · door-open yellow · IDLE dim
cyan pulse · RUNNING green breathing (boost = orange blink, battery < 20 %
= yellow blink) · PURGE teal blink · **DONE solid green = the completion
LED** · FAULT fast red. `PIXEL_ENABLED` / `PIN_PIXEL` / `PIXEL_COUNT`
(raise the count to chain an external strip on the same wire). DevKitC-1
v1.1 boards have the LED on GPIO38 — then the buzzer must move (e.g. to
PCF #2 P7).

## E-code fault system (owner spec, v2.0.14)

Faults carry the owner-documentation codes. `sev 0` = WARNING (cycle
continues, amber banner on the site + `[warn] E..` on serial); `sev 1` =
CRITICAL (cycle halted, power cut, 5 s beep, manual reset via Power On).

| Code | Meaning | Status |
|---|---|---|
| E01 | heater over-temperature (hard cut) | implemented |
| E02 | power-source change-over refused under load | implemented (warn) |
| E03 | heater failure — no temp rise 3 min at ≥80 % duty | implemented |
| E04 | all chamber sensors dead | implemented — **ONE** sensor left = warning + continue (v2.0.17 degraded) |
| E05 | load-cell implausible (negative) — start refused | implemented (warn) |
| E06 | RH > 60 % for 5 min despite the fan | implemented |
| E07 | door opened mid-cycle | implemented |
| E08 | door-sensor self-test | reserved (needs a door-test press) |
| E09 | blower RPM proof | reserved (needs a tach wire) |
| E10 | heater-current sense | reserved (needs ACS712/shunt) |
| E11 | selected feed reads DEAD (opto) | implemented (warn) |
| E12 | battery empty — safe shutdown | implemented |
| E13 | supply < 11.5 V for 60 s | implemented (warn) |
| E14 | supply > 15.0 V for 30 s | implemented (halt) |
| E15 | HX711 stopped responding / absent | implemented — **DEGRADED** (v2.0.17): keeps drying on time + RH stop |
| E16 | batch > 9.5 kg — start refused | implemented (warn) |
| E17 | target weight ≥ batch or < 50 g — start refused | implemented (warn) |
| E18 | invalid parameters | by design (all inputs range-clamped) |
| E19 | time-up with target not reached | logged in the cycle reason |
| E20 | chamber > setpoint + 15 °C for 60 s | implemented (warn) |

## Long-term cycle registry — AT24C256 EEPROM (owner request, v2.0.17)

The LittleFS files keep **detailed** per-cycle CSVs but only the last 40.
The AT24C256 module (32 kB I2C EEPROM, addr 0x50) keeps a **40-byte summary
of EVERY cycle** — ≈817 cycles, years of batches — that survives any
filesystem reformat. When the ring fills, the oldest summary is overwritten.

- **Wiring**: VCC→3V3, GND→GND, SDA→GPIO 8, SCL→GPIO 9 (same I2C0 bus as
  the AHT10 + PCF8575s), A0/A1/A2→GND = address **0x50**. Auto-detected at
  boot — no chip fitted, everything else works normally.
- **Each record**: sequence number, start time, duration, mode, end reason
  (done/stopped/fault/timeout/interrupted), E-code, setpoint, avg + max
  temp, max RH, outdoor temp, start/end/target weight, battery start/end,
  flags (scale lost / target reached). CRC-protected; a power cut mid-write
  rolls the record back at the next boot.
- **Website → History**: "Long-term registry … N/817 cycles stored ·
  ⬇ Download ALL cycle summaries (CSV)" — one row per cycle ever run.
  Clearing history from the site wipes the registry too (explicitly).
- **Wear**: one append per cycle ⇒ ≈817 rewrites of the header before any
  cell sees real cycling — at ~1 M erase/write cycles per cell and a batch
  a day, the chip outlives the machine.

## Degraded mode — sensors fail, the batch keeps drying (owner request, v2.0.17)

A dead sensor used to stop the machine. Now the machine tells you and keeps
going, so the batch is saved while the technician is on the way:

| What failed | Behaviour | Signals |
|---|---|---|
| ONE chamber sensor (AHT10 or DHT22) | runs on the survivor | E04 **warning**, nag beeps, banner + CLEARED on recovery |
| BOTH chamber sensors | still a full stop (E04) — no control left | E04 fault, as before |
| Weigh scale / HX711 lost (before or during a cycle) | **continues**: time-based + wet-RH completion replace weight tracking; boost + timeouts unchanged | E15 **warning**, banner "DEGRADED", nag beeps, yellow blinking pixel |
| Scale recovers mid-cycle | weight tracking resumes automatically | CLEARED + recovery beep |

Serial `[stat]` line gains a `deg:` field (active warning code or `-`);
the RGB pixel blinks **yellow** while running degraded (vs green breathing
normal / red blinking fault). Door, heater-overheat, empty-battery and the
other hard faults still stop the machine — degraded mode never overrides a
*fire-and-safety* stop, only a *measurement* stop.

## Virtual keypad — touch displays (v2.0.19)

If the pillar display is a touch screen (phone/tablet on `/display`, or the
control page), the physical keypad is optional:

- **Control page**: the "⌨ Keypad" pill (header) opens a right-side drawer
  with the live on-device menu + a 4×4 touch keypad.
- **/display kiosk page**: "⌨ keypad" (top bar) flips the page into a
  landscape two-pane layout — menu/status text on the left, keypad on the
  right; only that is shown, like a landscape tablet app.
- Keys go through the same path as the pillar keypad (`/api/key`), so beep
  feedback, the menu, value entry, start/stop (D) and mode (C) all behave
  identically.

## Power & unused pins (v2.0.18)

- **BLE is totally disabled**: the firmware never starts it, and at boot the
  BLE controller + stack RAM is handed back (`[pwr] BLE OFF` on serial) —
  lower power, ~10+ KB more heap. WiFi runs hotspot-only (AP), which has no
  modem-sleep trade-off to make.
- **Every unused GPIO is parked disabled** (`INPUT_PULLDOWN`) at boot —
  nothing floats, nothing half-drives: `[pins] N unused pads parked
  DISABLED`. On the S3 that's 3, 11 (21 = door, 40/42/47 = DS1302 RTC, 41 = DHT11, 48 = pixel). Never touched:
  35/36/37 (R8 PSRAM lines — even boards that report no PSRAM), 0/45/46
  (boot straps), 19/20 (USB), 43/44 (UART0), 26–32 (module flash).
- **PSRAM is not used at all** — some boards sold as "N16R8" report
  `PSRAM: None` (plain N16 silicon or the IDE option off). Either way the
  firmware is happy: ~350 KB free heap is several times what it needs.

## C · How the pieces interact (the 30-second mental model)

```
setTemp ──────────────┐  heaterMax caps everything the heater does
  boostHeat: t<setTemp−band → duty=heaterMax (500 W heat-up)
  then PID: Kp·e + ΣKi·e + Kd·Δe, clamped 0…heaterMax → BTS duty
humHigh/humLow ───────┐  fanMin/fanIn/fanOut/fanSlope shape fan duty
maxTemp ──────────────┐  95 °C hard stop overrides ALL of the above
cutoffPct ────────────┐  empty battery overrides ALL of the above
time (dryMinutes) ────┘  end → purge (cooldownSec) → power off
```

*Full firmware architecture: `docs/manual/03`. Every-inch build: `manual/10`.
Product spec: `docs/datasheet.md`.*
