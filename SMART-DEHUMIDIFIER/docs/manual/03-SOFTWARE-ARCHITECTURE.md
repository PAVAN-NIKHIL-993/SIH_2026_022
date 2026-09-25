# 03 — Software Architecture

## Source map (what every file does)

| File | Role |
|---|---|
| `src/config.h` | **Everything you'd tune**: pins, PWM, timing, default thresholds. Change things HERE first, never deep inside logic files |
| `src/main.cpp` | Boot order (relay off → sensors → battery → FS → AP → relay on) + the non-blocking loop |
| `src/aht10.*` | Dependency-free, non-blocking AHT10 driver; one instance per I2C bus (fixed 0x38 address). 20-bit values, checksum + sanity checked |
| `src/sensors.*` | Fuses both AHT10s: averages (heat PID), peaks (fan law, safety), health flags |
| `src/pwm.*` | LEDC wrapper that compiles on Arduino-ESP32 core 2.x AND 3.x |
| `src/battery.*` | Gated divider read (16× oversampled), volts→% per chemistry, bypass pin, 2 V "nothing connected" detection |
| `src/buzzer.*` | Non-blocking beep patterns (start/stop/power/done/fault) |
| `src/display.*` | Optional 3.5" ILI9488 SPI status screen — library-free driver, 1 Hz status page |
| `src/keypad.*` | Optional PCF8574 4×4 hex keypad; 15 ms debounced scan, one event per press |
| `src/scale.*` | Optional HX711 weigh scale (dry-to-weight): bit-bang driver, 2 Hz, tare/calibrate in NVS, 5-min rate window |
| `src/door.*` | Door limit switch (no lock fitted): the calibrate→load→ready workflow — start gated until scale calibration, batch weighed on close, opening mid-cycle = FAULT |
| `src/supply.*` | v2.0 power: P-MOSFET latch hold + BUTTON-1 (3 s hard off / 10 s reboot), BUTTON-2 default automation, solar⇄bypass toggle + optocoupler verify + 2-channel feed relay (switch only with loads quiet) |
| `src/menu.*` | v2.0 on-device menu: keypad navigation (2/4/6/8, A, B, #, *) + digit parameter entry on the TFT |
| `src/control.*` | **The brain**: Settings (NVS-persisted, versioned), state machine, heater PID, fan law, safety cuts, RAM datalog, manual heat override (web knob, 60 s then back to automatic) |
| `src/cyclelog.*` | Per-cycle CSV files in LittleFS, timestamps via phone-synced clock, auto-prune to 40, listing JSON |
| `src/web.*` | AP + captive DNS + all HTTP endpoints |
| `src/webui.h` | The entire embedded website as one PROGMEM page (Dashboard / Parameters, one-click defaults, built-in typical forecast) |
| `tools/online/sync-online.js` | Regenerates the GitHub-Pages site (`docs/index.html`) from webui.h + injects bridge & GitHub-backup layer |
| `tools/ui-test/` | jsdom headless tests: embedded UI (40 asserts) + online UI (12 asserts) |

## State machine

```
IDLE ──Start──> DRYING ──time+RH ok──> PURGING ──cooldownSec──> DONE(relay OFF)
  ▲                │                                                      │
  └──── Power On ──┴──────────────────────────────────────────────────────┘
                   │
                   ├── over-temp / both sensors dead 15 s / battery ≤ cutoff%
                   └──────────────────────> FAULT (relay OFF + reason)
User STOP (in DRYING) → PURGING (fans 100 %) → DONE.
```
Every state is shown on the badge; FAULT carries a text reason.

## Control laws

**Heater (BTS7960)** — PID every 1 s on the *average* chamber temperature:
```
duty% = Kp·e + Σ(Ki·e·1s) + Kd·(Δe/1s)     e = setTemp − tAvg
```
- integral clamped −20…100 (anti-windup), output clamped 0…heaterMax.
- Defaults Kp 10, Ki 0.2, Kd 5 → for a typical 100–300 W coil in a 60 L box.

**Fans (L298N)** — every 1 s from the *wettest* sensor:
| Condition | Duty |
|---|---|
| RH ≥ humHigh (and outside NOT wetter, smartVent) | fanMin + fanSlope·(RH−humHigh), max 100 %, then ×fanIn / ×fanOut per channel |
| RH ≥ humHigh and outside air wetter | fanMin (venting paused — pulling wet air in would raise RH) |
| humLow < RH < humHigh | fanMin (circulation) |
| RH ≤ humLow | 0 (keep heat, avoid over-drying) |
| PURGING | 100 % |

Outdoor priority: **fresh forecast (phone) → stale forecast → manual**.

**Boot (v2.0):** all outputs LOW → splash (ARCHITECTS OF SOLUTIONS /
SMART DEHUMIDIFIER) + 2/s power-on beeps → self-test → 10 s initialisation
window → main screen. **Supply:** solar ⇄ bypass by toggle, verified by
the optocoupler, relay switched only with loads quiet (refused under
load = 5 s error beep). **Fan law (v2.0):** one outlet fan; RH ≥ 60 %
for 1 min → 100 % for 60 s; RH ≥ 60 % for 5 min = fan FAULT; heat-up
flat 3 min = heater FAULT; target weight warned 5 min before time-up.

**Batch workflow:** door LOCKED until the scale is calibrated → unlock →
load + close → batch weight measured (diff vs tare) → READY → Start
(refused before that) → locked during RUNNING/COOLDOWN → unlock at
DONE/FAULT. Classic variant enforces the same flow in software.

**Completion gates (all must pass to end a cycle):** time elapsed ·
optional RH ≤ humTarget · optional weight-rate < weightRateG for
weightMinY (dry-to-weight).

**Battery** — sampled 5 s, % from a per-chemistry voltage window; ≤ bypassPct
→ bypass relay ON + badge; ≤ cutoffPct → safe shutdown power cut.

**End of cycle** — manual time elapsed (and RH target if required) → purge
→ **relay cuts heater+fans** → cycle CSV written to flash → 3 beeps.

## Settings reference (all website-editable, saved in NVS)

| Field | Range | Default | Meaning |
|---|---|---|---|
| setTemp | 25–95 °C | 80 | PID target (pillar recipes 45–95) |
| tempHyst | 0.2–5 | 1.5 | control band (display/smoothing) |
| maxTemp | setTemp+5…110 | 95 | hard safety cut (chamber rated 100 °C; keep AHT10s in the cool return path above 85 °C) |
| humHigh | 20–95 % | 60 | fans ramp above |
| humLow | 10–80 % | 40 | fans stop below |
| humTarget | 5–70 % | 35 | optional completion RH |
| requireHum | bool | off | finish only when RH also reached |
| dryMinutes | 1–1440 | 120 | the manually set drying time |
| fanMin | 0–60 % | 20 | circulation speed (gentle: 5–10) |
| fanIn / fanOut | 10–100 % | 100 / 100 | independent intake / exhaust scaling (gentle: 40–60 / 60–80) |
| fanSlope | 1–12 %/RH | 6 | ramp steepness above humHigh (gentle: 2–3) |
| heaterMax | 10–100 % | 100 | coil soft cap (full 500 W; see PARAMETERS.md) |
| cooldownSec | 10–600 s | 45 | purge before power cut |
| bypassPct | 5–50 % | 20 | battery % → bypass ON |
| cutoffPct | 0–40 % (< bypass) | 10 | battery % → safe shutdown |
| battType | 0–3 | 0 | 3S Li-ion / 4S Li-ion / 12 V SLA / 4S LiFePO4 |
| tzMinutes | −720…840 | 330 | local UTC offset (IST) |
| smartVent | bool | on | pause venting when outside is wetter |
| boostHeat | bool | on | BTS at MAX until setTemp−band, then PID holds |
| kp / ki / kd | 0–100 / 0–10 / 0–100 | 10 / 0.2 / 5 | heater PID |

## HTTP API (base `http://192.168.4.1`)

| Endpoint | Method | Purpose |
|---|---|---|
| `/` | GET | embedded dashboard |
| `/online` | GET | bridge page → GitHub-Pages UI (falls back to `/`) |
| `/api/data` | GET | everything: sensors, duties, battery, clock, weather, settings, defaults (polled 2 s) |
| `/api/history` | GET | last ≤600 log points (chart seeding) |
| `/api/log.csv` | GET | current run's live CSV |
| `/api/cycles` | GET | saved cycle list + meta |
| `/api/cycle?file=` | GET | one saved cycle CSV |
| `/api/settings` | POST | JSON settings (validated, clamped, NVS-saved) |
| `/api/start` `/api/stop` `/api/power` | POST | state actions |
| `/api/addtime?min=` | POST | extend/shrink remaining time |
| `/api/defaults` | POST | restore compiled-in defaults |
| `/api/settime?epoch=&tz=` | POST | clock set: auto phone push, manual picker (site Clock card), tap-the-clock on `/display`, keypad menu rows 6/7 |
| `/api/weather` | POST | weather push from the browser |
| `/api/clearcycles` | POST | wipe history |

## Data formats

Live RAM log: 10 s cadence, 2160 records (6 h), then ring-overwrite.
Cycle file (LittleFS, survives power loss, max 40):
```
# started,2026-09-02 14:00:00
# ended,2026-09-02 16:02:11
# reason,completed        (completed / stopped / fault)
# setTemp,45.0
# dryMinutes,120
# elapsedMin,122.2
# remainMin,0.0
sec,temp_avg_C,hum_avg_RH,hum_peak_RH,heat_pct,fan_pct,batt_V,batt_pct
0,30.4,68.2,69.1,100,20,12.5,84
...
```

## Key code excerpts (orientation only — full code in the files)

Start of a cycle (`control.cpp`):
```cpp
void Dryer::start() {
  ...
  setRelay(true);            // loads powered
  battery.setBypass(false);
  cyclelog::start();         // history entry opens
  buzzer.beep(2, 120, 80);   // start beeps
  _st = DState::RUNNING;
}
```
The one safety rule you should never edit away (`tick()`):
```cpp
if (!isnan(tMax) && tMax >= _cfg.maxTemp) raiseFault("Over-temperature!");
if (!sensors.anyOk() && running > 15 s)    raiseFault("Both AHT10 dead");
if (pct <= _cfg.cutoffPct)                 raiseFault("Battery empty");
```

## Build & verify commands

```bash
pio run                          # firmware build (or Arduino IDE upload)
node tools/online/sync-online.js # regenerate Pages site after webui.h edits
cd tools/ui-test && npm install && node ui-test.js && node online-test.js
```
