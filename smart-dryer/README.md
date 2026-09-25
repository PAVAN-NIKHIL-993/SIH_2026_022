# 🌿 Smart DeHummidifier — PILLAR

A production-ready ESP32 controller that dries agarbattis (incense sticks) in
a **pillar-shaped closed chamber** on **100 % solar power**: one **550 W
panel** feeds a 500 W heater class, the MCU holds the chamber anywhere in the
**45–100 °C** range and vents humidity automatically — with a built-in
**hotspot website** (fire/water themed, background you can restyle from your
phone) where you watch every value and set every threshold yourself.

**Platform at a glance**

| | |
|---|---|
| Power class | **550 W solar panel → 500 W pillar heater** (0.32 Ω nichrome, BTS7960 43 A bridge) |
| Continuous draw | **full 500 W at 100 % duty** — 550 W panel + battery carry the peaks; BTS7960 43 A with heatsink |
| Chamber | **6–8 ft pillar** (30 × 30 cm section): battery at the base, controller at the top, solar panel external |
| Temperature | recipes 45–80 °C (AHT10 zone), hard cut **95 °C**, chamber rated to 100 °C |
| Control | **door-locked calibrate→load→ready workflow** → full-power heat-up → PID hold → **RH-triggered outlet-fan bursts (60 %/1 min → 100 %/60 s)** → target-weight tracking → purge → auto power-off |
| Feedback | AHT10 + DHT22 chamber (S3), DHT11 outdoor, battery %, **web display (`/` + `/display` kiosk — no hardware screen)**, hex keypad (A/B/C/D), **weigh scale (dry-to-weight)**, live graphs, 5-day forecast, cycle history + CSV, [warn]/[stat] serial diagnostics |

```
Solar panel ──> BMS ──> Battery ──> BMS output ──> [ LOAD RELAY ] ──> Heater coil + Fans

**Platform at a glance**

| | |
|---|---|
| Power class | **550 W solar panel → 500 W pillar heater** (0.32 Ω nichrome, BTS7960 43 A bridge) |
| Continuous draw | **full 500 W at 100 % duty** — 550 W panel + battery carry the peaks; BTS7960 43 A with heatsink |
| Chamber | **6–8 ft pillar** (30 × 30 cm section): battery at the base, controller at the top, solar panel external |
| Temperature | recipes 45–80 °C (AHT10 zone), hard cut **95 °C**, chamber rated to 100 °C |
| Control | **door-locked calibrate→load→ready workflow** → full-power heat-up → PID hold → **RH-triggered outlet-fan bursts (60 %/1 min → 100 %/60 s)** → target-weight tracking → purge → auto power-off |
|---|---|---|
| MCU | ESP32 DevKit V1 (30-pin) | reads sensors, runs control laws + the website |
| Heater driver | **BTS7960 43 A H-bridge** | PWM-switches the heating-coil current; duty set by a PID that holds chamber temperature at your setpoint |
| Fan driver | **L298N** | PWM speed for the ONE outlet fan: RH ≥ 60 % for 1 min → 100 % for 60 s (burst venting) |
| Sensors | **AHT10 (top) + DHT22 (cool return)** on the S3 — classic: 2 × AHT10 | temp + RH at the top and return of the chamber (averaged, RH-peak tracked) |
| Power | Solar panel → BMS → battery → BMS out | battery % measured on the ADC through a divider + low-side transistor pull-down |
| Cutoff | *(relay optional — not fitted)* | cycle end: heater + fans stop; with a relay fitted it also cuts all load power |
| Bypass | *(relay optional — not fitted)* | engages automatically when battery % drops to your bypass threshold (`RELAYS_ENABLED 1`) |
| **Buzzer** | active module — **fitted by default** (v2.0: classic GPIO13, S3 GPIO38) | start = 3 s · end = 5 s · power-on = 2/s × 3 s · error = 5 s · door = 1 s · mode change = 3 s |

## ⚠️ Two wiring rules that matter

1. **The AHT10 address is factory-fixed at `0x38`** — max one per I2C bus.
   Classic: sensor #1 on `Wire` (GPIO21/22), sensor #2 on `Wire1`
   (GPIO32/33). S3 (v2.0.9): ONE AHT10 on `Wire` (GPIO8/9, shared with the
   keypad PCF) + a **DHT22 on GPIO10** as chamber source #2 — I2C1 free.
2. **ADC2 pins don't work while WiFi is on.** Battery sense uses GPIO36
   (ADC1), which keeps working in AP mode.

| Power | Solar panel → BMS → battery → BMS out | battery % measured on the ADC through a divider + low-side transistor pull-down |

| ESP32 pin | Connects to | Notes |
|---|---|---|
| 21 / 22 | AHT10 #1 SDA / SCL | top of chamber (classic pin; S3: 8/9) |
| 32 / 33 | AHT10 #2 SDA / SCL | classic only (S3: DHT22 on GPIO10 + DHT11 outdoor on 35) |
| 25 | BTS7960 **RPWM** *(through the 555 level shifter: GPIO25 → 555 pin 4, 555 pin 3 → RPWM)* | 1 kHz PWM of coil current |
| — (GND) | BTS7960 **LPWM** | hard-tied to GND at the module (resistive load) |
| 26 | BTS7960 **R_EN + L_EN** (jumpered together) | driver enable |
| 13 | **Buzzer** (KY-012) | v2.0 beep pattern set |
| 15 | **BUTTON-1** sense | master power: 3 s = hard off, 10 s = reboot |
| 18 | **BUTTON-2** | default automation (agarbatti preset + start) |
| 14 | L298N **ENB** | the ONE outlet fan wire (remove the ENB jumper, 10 kΩ ENB→GND) |
| 5 / 4 | spare | IN3→5V + IN4→GND are tied ON THE MODULE (fixed direction, v2.0.10) |
| 16 | **P-MOSFET latch hold** | keeps the pillar powered (hard power-off) |
| 27 | **SOLAR toggle** | requests solar / bypass mode |
| 35 | **supply optocoupler** | verifies the selected feed is live (input-only) |
| 36 | battery divider output | ADC1, WiFi-safe |
| 19 | divider enable (N-MOSFET gate) | pulls divider to GND only while sampling |
| 23 | LOAD RELAY *(not fitted — RELAYS_ENABLED 0)* | cuts heater+fans at end of cycle / faults |
| 2 | BYPASS control *(not fitted)* | engages the bypass supply when the battery is low |
| 17 / 12 / 0 / 2 / 23 | spare | no display in this build — the website is the screen (`/` + `/display`) |
| 21/22 | I2C hex keypad (optional) | PCF8574 backpack @ 0x20, shares Wire with AHT10 #1 |
| 34 | weigh-scale DOUT (classic) | HX711 (S3: CLK=1, DOUT=2) |
| 34 | door limit switch (S3) — no lock fitted | calibrate→load→ready workflow (software gates) |

All grounds common: ESP32 GND = BTS7960 GND = L298N GND = BMS output −.

| 13 | **Buzzer** (KY-012) | v2.0 beep pattern set |
| 15 | **BUTTON-1** sense | master power: 3 s = hard off, 10 s = reboot |
| 18 | **BUTTON-2** | default automation (agarbatti preset + start) |
| 14 | L298N **ENB** | the ONE outlet fan wire (remove the ENB jumper, 10 kΩ ENB→GND) |
| 5 / 4 | spare | IN3→5V + IN4→GND are tied ON THE MODULE (fixed direction, v2.0.10) |
| 16 | **P-MOSFET latch hold** | keeps the pillar powered (hard power-off) |
| 27 | **SOLAR toggle** | requests solar / bypass mode |
- Remove the ENA/ENB jumpers (ESP32 provides PWM there).
- `12 V` input from BMS output (through the LOAD RELAY), `5 V` regulator
  jumper left ON is fine, `GND` common.
- Fan: ONE 12 V DC outlet fan on OUT3/OUT4 (intake removed in v2.0).

### Battery sense (divider + transistor pull-down)

| 34 | weigh-scale DOUT (classic) | HX711 (S3: CLK=1, DOUT=2) |
| 34 | door limit switch (S3) — no lock fitted | calibrate→load→ready workflow (software gates) |

All grounds common: ESP32 GND = BTS7960 GND = L298N GND = BMS output −.

### BTS7960
- `B+ / B-` → battery + / − (through the LOAD RELAY, with an inline fuse sized for your coil).
- `M+ / M-` → heating coil.
- Logic side `VCC` = 5 V from the buck, `GND` common. 3.3 V logic works; if
  your particular board misbehaves, add a level shifter or drive VCC at 3.3 V.

### L298N
- Remove the ENA/ENB jumpers (ESP32 provides PWM there).
- `12 V` input from BMS output (through the LOAD RELAY), `5 V` regulator
  jumper left ON is fine, `GND` common.
- Fan: ONE 12 V DC outlet fan on OUT3/OUT4 (intake removed in v2.0).

### Battery sense (divider + transistor pull-down)

```
battery(+) ──[100 kΩ]──┬──────> GPIO36 (ADC)
- **Battery critical (≤ cutoff %, default 10 %):** safe shutdown — heater
  off, purge, relay opens. Same "power cut" path as the end of a cycle.

## Optional: online UI from GitHub Pages + cycle backup to GitHub

The dashboard can be **hosted on GitHub Pages** instead of (or alongside)
the page inside the ESP — update the website by pushing to the repo, no
reflash needed. Why the extra hop: a browser will not let an `https://`
GitHub page call the dryer's `http://192.168.4.1` API directly (mixed
content), so the ESP serves a **tiny bridge page** that frames the GitHub
site and relays every API call.

**One-time setup**
1. Repo → Settings → Pages → Deploy from a branch → `main` / `/docs`
   (the generated site is committed in `docs/index.html`).
2. Check `ONLINE_UI_URL` in `src/config.h` matches your Pages URL
   (default: `https://pavan-nikhil-993.github.io/arena/`).
3. On the phone (connected to the dryer hotspot, mobile data on), open
   **http://192.168.4.1/online** — bookmark it. The GitHub-hosted UI loads
   with full live data, thresholds, history and weather relay.

**Behaviour**
- No internet on the phone → `/online` auto-falls back to the built-in page.
- The embedded page at `/` is always there (link "online version" on the dashboard).
- **Back up cycles to GitHub**: in the online UI's *Past cycles* box, enter a
  repository, branch, folder and a fine-grained PAT (contents: read+write).
  Every cycle CSV stored on the ESP is committed to that repo. The token
  lives only in the browser's localStorage and is sent only to
  api.github.com — the ESP still never touches the internet.

**Editing the website:** change `src/webui.h` (single source of truth), then
`node tools/online/sync-online.js` regenerates `docs/index.html` (adds the
bridge layer + GitHub backup) — commit both, push, done.

**The buzzer is not optional** — it is part of the core build (GPIO13 classic / GPIO38 S3)
and always compiles in.

**Outdoor data priority (for the card + smart venting):**
`live forecast (phone) → stale forecast → manual entry`.
The badge on the Outdoor card shows which source is in use
(**LIVE / MANUAL / STALE**).

## Website (http://192.168.4.1)

## Optional: online UI from GitHub Pages + cycle backup to GitHub

The dashboard can be **hosted on GitHub Pages** instead of (or alongside)

Three slides:

1. **Dashboard** — pro dark UI with SVG **ring gauges** (temp with target
   marker, RH with band markers, battery), heater/fan duty bars, elapsed /
   remaining time, and an interactive **multi-series graph**: temp, RH,
   heater %, fan %, battery % with toggleable legend, dual axes, elapsed
   time axis and hover crosshair + tooltip. **Past cycles** each get a
   "view" button that graphs that run's full curve, plus per-cycle CSV
   download. Buttons: Start, Stop, +15 min, Power On.
2. **Parameters** — EVERY setting on one page: target temp ± band, safety
   cutoff, humidity band (fans ramp/stop), optional RH completion target,
   **drying time set manually (hours + minutes)**, fan/heater limits,
   battery type + cutoff %, advanced PID gains — plus **↳ Reset all to
   defaults** (one click) and a factory-defaults reference table below the
   form. **Save & Start** begins the cycle (manual by default — `AUTO_START`
   is off; flip it in config.h for plug-and-play auto-start).

**End of cycle:** time elapsed (and RH target if required) → fans run 100 %
to purge hot air → heater + fans stop (relay cuts power too, if fitted). The ESP stays alive so you can
press *Power On* for the next batch.

**Past cycles (saved in ESP flash):** every finished run is written to its
  lives only in the browser's localStorage and is sent only to
  api.github.com — the ESP still never touches the internet.

**Editing the website:** change `src/webui.h` (single source of truth), then
`node tools/online/sync-online.js` regenerates `docs/index.html` (adds the
bridge layer + GitHub backup) — commit both, push, done.

**The buzzer is not optional** — it is part of the core build (GPIO13 classic / GPIO38 S3)
and always compiles in.

**Outdoor data priority (for the card + smart venting):**
`live forecast (phone) → stale forecast → manual entry`.
The badge on the Outdoor card shows which source is in use
(**LIVE / MANUAL / STALE**).

## Website (http://192.168.4.1)

Connect your phone/laptop to the hotspot **`AgarbattiDryer`** (password
`dryer1234`) — the captive portal opens the page automatically. No internet
needed, nothing leaves the chamber 😉

Three slides:

1. **Dashboard** — pro dark UI with SVG **ring gauges** (temp with target

## Control logic in one page

- **Heater (BTS7960):** **full-power heat-up** (on by default, web toggle) —
  the BTS runs at MAX output until the chamber reaches
  target − control band, then a PID (Kp/Ki/Kd configurable) holds the
  *average* chamber temperature there (duty 0–100 %, heaterMax default
  **100 % = the full 500 W**; the manual knob obeys the same cap).
- **Manual heat knob** (dashboard, Heater card): drag to command the BTS
  duty yourself — e.g. 100 % to test max output. The MCU returns to
  automatic control **60 s** after your last turn (badge counts down);
  over-temperature and sensor-fault cuts stay active throughout.
  New duty every 1 s; every settings change applies **live**, mid-cycle.
  Serial prints a `[stat]` line every 15 s plus instant `[warn]` lines when
  any module goes MISSING/comes back, and `[batt] BYPASS ON` when the
  battery hits the bypass threshold.
- **Fan (L298N)** — ONE outlet fan, burst venting (v2.0), controlled on
  the *wettest* sensor: RH ≥ `fanTrigRH` (60 %) sustained `fanTrigMin`
  (1 min) → 100 % for `fanBurstS` (60 s), then re-arm. Below the trigger
  the fan idles at `fanMin` (default 0 = off — keep the heat in).
  A one-second 60 % kick-start keeps very low PWM duties reliable;
  purge at cycle end = 100 % continuous.
  - **FAULT** if RH stays ≥ 60 % for 5 min despite the bursts (fan error)
- **Safety:** RH/temp sensor both dead >15 s → power cut; any sensor over
  *safety cutoff* temp → immediate power cut; battery ≤ cutoff % → power
  cut. Every fault says why on the dashboard.

**End of cycle:** time elapsed (and RH target if required) → fans run 100 %

| Parameter | Default |
|---|---|
| Mode / target temperature | AGARBATTI · 60 °C (ceiling 80, hard cut 95) |
| Control band | ±1.5 °C |
| Hard safety cutoff | 95 °C (AHT10s in cool return path above 85 °C) |
| Fan burst trigger | RH ≥ 60 % for 1 min → 100 % for 60 s |
| Drying time (manual) | 120 min |
| Purge before power cut | 45 s |
| Bypass below / cutoff below | 20 % / 10 % |
**Live date & time:** the ESP32 has no battery-backed clock, so the website
*takes the initiative* — whenever the dashboard connects (and every 30 min)
it pushes your phone's date, time and timezone to the ESP automatically.
You can also set date & time manually under *Custom → Clock*, or change the
timezone offset there (default +330 min = IST). The header shows the live
clock; cycle files and history stamps use it.

**Outdoor weather — no internet on the ESP needed:** the ESP32 never goes
online. Instead, **your phone's browser acts as the bridge**: while connected
to the dryer hotspot it still has mobile data, so the dashboard fetches live
weather (Open-Meteo, free, no API key) and *pushes it into the ESP*. The
Outdoor card shows temp, RH, rain chance, wind and condition, refreshed
every 20 min while the page is open. One-time setup: type your town in the
card (geocoding finds it). No internet at all? Type outdoor temp/RH manually.
**Outdoor-aware venting** (Custom slide, on by default): when the outside air
is wetter than the chamber (e.g. rain), venting is paused at circulation
speed — pulling in wet air would slow drying — and the card says so.


```
smart-dryer/
├── arduino-ide/…                               ← THE FIRMWARE (zero libraries)
│     smart-dryer-single-file.ino                  classic ESP32 DevKit V1
│     smart-dryer-s3-single-file.ino               ESP32-S3 variant (recommended)
├── variants/esp32-s3/                          S3 pin map + config + why-S3 README
├── src/ + platformio.ini                       shared logic, PlatformIO layout
│     config.h (all pins/defaults) · main.cpp · control · sensors · aht10 ×2
│     battery · pwm · buzzer · cyclelog · web · webui (PROGMEM website)
├── docs/
│   ├── datasheet.md        product spec: power story, temp zones, safety chain
│   ├── PARAMETERS.md       EVERY parameter: range, default, effect, caution
│   ├── wiring-diagram.svg
│   ├── manual/00–10        the full story: parts, build, architecture,
│   │                       commissioning, fixes, limitations, RTOS, flashing,
│   │                       every-inch build (10)
│   └── index.html          online UI (GitHub Pages via the CI workflow)
├── future/                 future-ready plans: roadmap + one spec per upgrade
├── production/             build-to-sell: BOM · assembly sign-off · QC ·
│                           serials & labels · warranty card
├── demo/                   10-minute demo script + one-pager
├── compliance/             safety checklist · standards roadmap
├── media/                  concept render + photo shot-list
├── tools/                  single-file assembler · online-UI sync ·
│                           string/printf verifiers · UI test suites (jsdom)
├── scripts/check-all.sh    one-command release gate (same as CI)
├── .github/workflows/      CI (scans+tests+sync) · Pages deploy
├── CHANGELOG.md            release history
└── ADVANCED-IDEAS.txt      the original idea vault
```

## Full manual (optional deep-dive docs)

`docs/manual/` — the complete build story in 7 files:
[01 Parts & tools](docs/manual/01-PARTS-AND-TOOLS.md) ·
[02 Build step by step](docs/manual/02-BUILD-STEP-BY-STEP.md) ·
[03 Software architecture](docs/manual/03-SOFTWARE-ARCHITECTURE.md) ·
[04 Deployment & commissioning](docs/manual/04-DEPLOYMENT-AND-COMMISSIONING.md) ·
[05 Errors & fixes](docs/manual/05-ERRORS-AND-FIXES.md) ·
[06 Limitations solved & unsolved](docs/manual/06-LIMITATIONS-SOLVED-AND-UNSOLVED.md) ·
[07 What's left & the reach](docs/manual/07-WHATS-LEFT-AND-THE-REACH.md)

## Advanced ideas vault

`ADVANCED-IDEAS.txt` — 40+ upgrade ideas at every level (sensors, control
brain, solar power, data science, website, connectivity, mechanical,
quality-AI), each rated by difficulty with hooks into this codebase: load
cells that dry-to-weight, aroma protection via a VOC "nose",
solar-following heat, QR-coded batches, a digital twin, and more.

## Tuning tips

- Temperature overshoots → lower Kp, raise Kd slightly.
- **Safety:** RH/temp sensor both dead >15 s → power cut; any sensor over
  *safety cutoff* temp → immediate power cut; battery ≤ cutoff % → power
  cut. Every fault says why on the dashboard.

## Default thresholds

| Parameter | Default |
## Safety notes

- Fuse the heater circuit at the battery, sized for your coil.
- The 95 °C hard cutoff is a *software* backstop — also fit a bimetal
  thermostat on the chamber for independent protection.
- Never run the coil without fans enabled at high duty — the purge sequence
  handles cool-down; don't bypass it.
| Bypass below / cutoff below | 20 % / 10 % |
| Heater PID Kp/Ki/Kd | 10 / 0.2 / 5 |

*(These match `src/config.h`. Change them there to change slide ①.)*

## Build & flash

**PlatformIO (recommended):**
```bash
cd smart-dryer
pio run                 # build
pio run -t upload       # flash (USB)
pio device monitor      # serial console @115200
```

**Arduino IDE:** open `src/main.cpp`, install **ArduinoJson** (Benoit
Blanchon, v7) from Library Manager, board = *ESP32 Dev Module*, Upload.

## Project layout

```
smart-dryer/
├── arduino-ide/…                               ← THE FIRMWARE (zero libraries)
│     smart-dryer-single-file.ino                  classic ESP32 DevKit V1
│     smart-dryer-s3-single-file.ino               ESP32-S3 variant (recommended)
├── variants/esp32-s3/                          S3 pin map + config + why-S3 README
├── src/ + platformio.ini                       shared logic, PlatformIO layout
│     config.h (all pins/defaults) · main.cpp · control · sensors · aht10 ×2
│     battery · pwm · buzzer · cyclelog · web · webui (PROGMEM website)
├── docs/
│   ├── datasheet.md        product spec: power story, temp zones, safety chain
│   ├── PARAMETERS.md       EVERY parameter: range, default, effect, caution
│   ├── wiring-diagram.svg
│   ├── manual/00–10        the full story: parts, build, architecture,
│   │                       commissioning, fixes, limitations, RTOS, flashing,
│   │                       every-inch build (10)
│   └── index.html          online UI (GitHub Pages via the CI workflow)
├── future/                 future-ready plans: roadmap + one spec per upgrade
├── production/             build-to-sell: BOM · assembly sign-off · QC ·
│                           serials & labels · warranty card
├── demo/                   10-minute demo script + one-pager
├── compliance/             safety checklist · standards roadmap
├── media/                  concept render + photo shot-list
├── tools/                  single-file assembler · online-UI sync ·
│                           string/printf verifiers · UI test suites (jsdom)
├── scripts/check-all.sh    one-command release gate (same as CI)
├── .github/workflows/      CI (scans+tests+sync) · Pages deploy
├── CHANGELOG.md            release history
└── ADVANCED-IDEAS.txt      the original idea vault
```

## Full manual (optional deep-dive docs)

`docs/manual/` — the complete build story in 7 files:
[01 Parts & tools](docs/manual/01-PARTS-AND-TOOLS.md) ·
[02 Build step by step](docs/manual/02-BUILD-STEP-BY-STEP.md) ·
[03 Software architecture](docs/manual/03-SOFTWARE-ARCHITECTURE.md) ·
[04 Deployment & commissioning](docs/manual/04-DEPLOYMENT-AND-COMMISSIONING.md) ·
[05 Errors & fixes](docs/manual/05-ERRORS-AND-FIXES.md) ·
[06 Limitations solved & unsolved](docs/manual/06-LIMITATIONS-SOLVED-AND-UNSOLVED.md) ·
[07 What's left & the reach](docs/manual/07-WHATS-LEFT-AND-THE-REACH.md)

## Advanced ideas vault

`ADVANCED-IDEAS.txt` — 40+ upgrade ideas at every level (sensors, control
brain, solar power, data science, website, connectivity, mechanical,
quality-AI), each rated by difficulty with hooks into this codebase: load
cells that dry-to-weight, aroma protection via a VOC "nose",
solar-following heat, QR-coded batches, a digital twin, and more.

## Tuning tips

- Temperature overshoots → lower Kp, raise Kd slightly.
- Reaches target too slowly → raise Kp, or check fans aren't over-venting
  during heat-up (raise humLow or fanMin).
- Humidity stays high with fans at 100 % → coil too small for chamber
  volume, or the exhaust outlet is restricted.
- Battery % looks wrong → measure BMS output with a multimeter and adjust
  `VBAT_CAL_OFFSET`, or fix `VBAT_DIV_RATIO` to your actual resistor pair.

## Safety notes

- Fuse the heater circuit at the battery, sized for your coil.
- The 95 °C hard cutoff is a *software* backstop — also fit a bimetal
  thermostat on the chamber for independent protection.
- Never run the coil without fans enabled at high duty — the purge sequence
  handles cool-down; don't bypass it.
- L298N drops ~2 V — size fans accordingly (12 V fans run a bit slower).