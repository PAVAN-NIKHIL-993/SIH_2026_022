# Changelog — Smart Dehumidifier Pillar

All notable changes to the Smart Dehumidifier firmware & UI.
Firmware ships as one file: `arduino-ide/SMART-DEHUMIDIFIER-single-file/SMART-DEHUMIDIFIER-single-file.ino`.

## 2026-09 (v2.0.22 — sources restored, every sketch compiled for real)

- **Firmware sources restored exactly to v2.0.21.** The first import (from
  the Arena patch files) applied 23 partial diffs onto v2.0.19 text instead
  of their real base. 14 files were still garbled, so **neither sketch
  could compile**:
  - `control.h` had no `struct LogRec`;
  - `main.cpp` had lost the settings / sensors / battery / dryer init;
  - `web.cpp`, `config.h`, `buzzer.*` and `cyclelog.*` carried hundreds of
    duplicated lines;
  - `platformio.ini` had lost `board = esp32dev`.

  The files were recovered byte-exact: the v2.0.21 patch was applied onto
  its real base (the old `arena` repo, commit 48ee940), every file was
  hash-verified against the patch, and the v2.0.21 renames were then
  re-applied. `tools/audit-symbols.py` had been reporting this all along
  but was not wired into the gate.
- **Real compile on every push** (`scripts/compile-sketches.sh`, arduino-cli,
  esp32 core 3.3.12 / AVR 1.8.8) — 16 builds:
  - both firmware variants, plus the S3 firmware in `DRYER_RTOS=1` mode;
  - `board-test-s3`, the 10 sensor tests and the 2 UNO display sketches.

  It found and fixed:
  - **classic ESP32 firmware did not link**: the serial `rtc` / `rtcset`
    commands call the DS1302 driver, which compiles out with
    `RTC_ENABLED 0`. `rtc.cpp` now provides the no-op API that `rtc.h`
    documents.
  - `rtc-test`: its `bitWrite()` / `bitRead()` helpers clashed with
    Arduino's macros (renamed the way `src/rtc.cpp` was in v2.0.21).
  - `board-test-s3`: `ESP.getEfuseMac()` was misused, and the MAC line
    printed its bytes scrambled.
  - `battery-test`: a String label was passed as `const char*`.
  - `dht-test`: the IDE's auto-prototype landed above `struct DhtDev`.
- **Online UI**: `ONLINE_UI_URL` → `https://pavan-nikhil-993.github.io/SIH_2026_022/`.
  It pointed at the old `arena` repo, which has no Pages site. The Pages
  setup steps now say Source: **GitHub Actions**; a branch deploy cannot
  serve `SMART-DEHUMIDIFIER/docs`.
- **CI / tooling**:
  - `check-all.sh` grows from 4 to 6 steps: the symbol audit and the host
    DS1302 driver test join it.
  - GitHub Actions move to Node 24 (checkout / setup-node / setup-python
    v7, configure-pages v6, upload-pages-artifact v5, deploy-pages v5).
  - The Pages deploy now *skips* with a notice until Pages is enabled,
    instead of failing every push to main.
  - jsdom moves to 30.
- **Docs**:
  - stale S3 pins corrected (outdoor DHT11 = 41, door = 21);
  - ArduinoJson instructions removed (the firmware needs no libraries);
  - repo name updated; a broken README anchor fixed.
- **Repo hygiene**:
  - the four patch dumps (~15 MB) are removed from the root and kept in
    git history;
  - the host RTC test outputs are gitignored;
  - the final newline stripped by the import is restored in 104 files.
- `FW_VERSION` 2.0.22.

## 2026-09 (v2.0.21 — DS1302 real-time clock, S3)

- **Date & time that survive a FULL power-down**: DS1302 RTC module
  (CR2032 coin cell on the module) on S3 pins **RST 40 / SCLK 42 / I-O 47**
  (3-wire bit-bang in `src/rtc.*`, zero libraries). The S3's own crystal
  clock keeps counting during USB/serial power, so this is not just a
  backup — it keeps the calendar accurate with the pillar completely off.
- **Auto-detected**: scratch-RAM magic probe (write 0xA5 → read back).
  No chip fitted = firmware runs exactly as before (phone sync + NVS
  clock) with no config change, no error, no pin must be free.
- **Writes are authoritative**: every real time set — web
  "SET TIME FROM PHONE", keypad clock menu, serial `rtcset` — is
  mirrored into the chip. Boot restores time from the chip when present
  and sane (before phone sync), else falls back to NVS/phone as before.
- Console: `rtc` = status (present / valid / last sync / registers),
  `rtcset` = mirror the running clock into the chip. Web + serial show
  RTC status; web page has the time-sync row with the RTC state.
- Bench sketch: `arduino-ide/sensor-tests/rtc-test/` — same pins,
  "SET TIME FROM PHONE" with re-read verification.
- **Product name → "Smart Dehumidifier"** (PRODUCT_NAME + web UI titles).
- Free pool shrinks: S3 free GPIOs are now **3, 11** (40/42/47 carry
  the RTC). All pin docs, BOM, checklist, build manual, variants README,
  sensor-tests README and the design diagrams updated to match.

## 2026-09 (v2.0.20 — S3 pin remap for headers without GPIO 22–34)

- **The pillar's S3 devkit header does NOT break out GPIO 22–34**, so two
  S3 pins moved (classic ESP32 map untouched):
  - outdoor **DHT11: GPIO 33 → 41**
  - **door limit switch: GPIO 34 → 21**
- `config-s3.h` (pins + all stale pin comments), `board-test-s3`, the S3
  sensor tests (dht-test, door-test), the generated S3 single-file and
  every pin doc (PIN-CONFIG, PIN-MAP, WIRING-S3-PCF, PINS-S3-PCF2,
  PARAMETERS, variants README, manual 02/09) now agree on 41/21.
- Free pool for new hardware (S3): **3, 11, 40, 42, 47** — GPIO 35/36/37
  remain never-wired (octal PSRAM on R8 boards).
- `FW_VERSION` 2.0.20 in BOTH variants; the battery-ADC row dropped from
  the variants README pin table is restored.

## 2026-09 (v2.0.19 — virtual 4×4 keypad for touch displays)

- `FW_VERSION` now reports the real version — **2.0.19** on the website
  header, serial banner, TFT/bridge screens (it had stayed "2.0.0" through
  every release since).

- **Main page**: "⌨ Keypad" pill in the header opens a right-side drawer —
  the on-device menu text + a touch 4×4 keypad (same keys as the pillar
  keypad: digits type, 2/4/6/8 move, A OK, B menu, C mode, D run/stop,
  * home, # back). Manual touch entry everywhere the physical pad works.
- **/display kiosk page**: "⌨ keypad" in the top bar switches the page to a
  landscape two-pane layout — text page left, keypad right (stacks on
  narrow screens). Only bar + text + keypad are shown, tablet-style.
- Firmware: new `POST /api/key?k=X` route feeds the SAME path as the
  physical keypad (`dryerKey()` — beep feedback, menu, start/stop, mode);
  `/api/data` gains a `menu` object (items, cursor, edit buffer + hint)
  while a menu screen is up. Nothing changes when no menu is open.

## 2026-09 (v2.0.18 — cycle-data downloads everywhere, pins & BLE off)

- **Home page**: "Cycle data" row right under Start/Stop — ⬇ ALL cycles
  (EEPROM registry, live count) and ⬇ latest cycle (full-detail CSV via the
  new `/lastcycle.csv` route). The kiosk `/display` page gets a "⬇ data"
  link in its top bar.
- **Unused pins parked DISABLED**: all spare GPIOs go `INPUT_PULLDOWN` at
  boot (S3: 3, 11, 21, 40, 41, 42, 47; count logged on serial). Bug fixes:
  GPIO 35 now on the never-touch list with 36/37 (R8 PSRAM lines), and the
  status-pixel pin (48) added to the used-pin list so it can never be parked.
- **BLE totally disabled for low power**: controller + stack RAM released at
  boot (`[pwr] BLE OFF`); hotspot-only WiFi (no modem-sleep trade-off).
- Board finding (owner chip test): 16 MB flash, **no PSRAM detected** —
  firmware does not use PSRAM, so nothing changes; docs updated (IDE
  setting: PSRAM Disabled is correct).

## 2026-09 (v2.0.17 — AT24C256 cycle registry + degraded mode)

**Long-term cycle registry (AT24C256, 32 kB I2C EEPROM @ 0x50)**
- `src/eelog.h/.cpp`: wear-aware ring of 40-byte CRC-protected cycle
  summaries (817 slots ≈ years of batches); torn writes rolled back at boot.
- Appended from `cyclelog::finish()` (end reason, stats from the in-RAM log,
  weights, battery) and on interrupted-cycle detect at boot.
- Website: History card shows registry usage + "Download ALL cycle
  summaries" (`/eelog.csv`); Clear-history wipes it too (explicit action).
- `/api/data` gains `eelog:{ok,used,slots}`; serial `[eelog]` boot line.

**Degraded mode — sensors fail, the batch keeps drying**
- E15 (scale/HX711 lost or absent, before or mid-cycle): **halt → warning**;
  time-based + wet-RH completion take over; recovery auto-resumes weighting.
- Single chamber sensor dead (AHT10 or DHT22): E04 **warning** + continue on
  the survivor; both dead still halts (no control input left).
- Start gate: a dead scale no longer refuses START; only an *uncalibrated
  working* scale does (calibration remains the workflow gate).
- Alerts: WARN nag beeps, dashboard banner + CLEARED, `deg:` field on the
  serial `[stat]` line, yellow blinking RGB pixel while running degraded.
- `_endReason` widened 16→32 bytes (the long E19 string was being truncated).

Docs: PARAMETERS (registry + degraded sections, E04/E15 rows), WIRING/PIN
docs + BOM + checklist + manual 05 updated; ui/online tests cover the
registry line. AT24C256 absent = registry simply off, nothing else changes.

## 2026-09 (v2.0.16 — stick & paste batch calculator, from the owner's drying math)

- **Owner's calculations wired into Parameters:** moisture is a paste property — raw mix 30–40 % water, finished stick 8–10 %. New settings `stickCount / stickWetG / pasteWaterPct / targetMoistPct` (defaults 0/2.5 g/35 %/10 %); with sticks > 0 the firmware **computes the batch target**: `N × wet × (1 − (paste% − final%)/100)` (Settings v8 — one-time defaults reset on old units).
- **Site:** new "Batch calculator — sticks & paste" card with a live preview — per-stick wet→dry, water per stick, batch wet→target, **total water to remove (g + L)** — auto-fills the target-weight field. Worked example (2.5 g, 35→10 % = 1.875 g dry, 0.625 g water/stick) baked into the help text.
- **E17 synergy:** the computed target is cross-checked against the MEASURED batch at START — a paste wetter than the parameter claims refuses with E17 (fix the paste %, retry).
- **Reference math documented** (PARAMETERS §batch calculator): the wet-bulb RH shorthand `RH ≈ 98 − ((Tdry−Twet)/Tdry)×300` (°F; 113/95 → 50 %) — reference-only, the AHT10/DHT22 read RH directly — plus the evaporation load `D = N × (Mwet − Mdry)` with the 0.63 kWh/kg rule of thumb.

## 2026-09 (v2.0.15 — 37-pattern buzzer engine + RGB status pixel + completion summary)

- **Buzzer engine rebuilt** (`src/buzzer.*`): segment/repeat player — `bz::play()` one-shots with priority preemption, `bz::startRepeat()` nagging patterns. **~30 of the owner's 37 alerts live** on the single-tone active buzzer (cadence carries the identity): key clicks, invalid chirps, BUTTON-1 hold ticks, door-ajar-at-START nag, no-trays, unstable-weight, ready, setpoint-reached, fan-on blip, door-open-mid-cycle rapid beeping, 50 %-moisture, target-approaching, 5-min-to-timeout, battery low/critical, weight anomaly, warning/critical fault tones + post-critical nag, recovered, log-saved, AP-up, client-joined, storage-90 %, maintenance-every-50-cycles, init-fail, brownout-resume, cool-down-complete, shutdown sweep, factory-reset sweep. n/a by design: #10, #20, #31/#32 (doc'd in PARAMETERS).
- **RGB status pixel** (`src/pixel.*`): the DevKitC-1 on-board WS2812, driven through hardware SPI at 6.4 MHz — zero libraries, nothing to wire. Status law: boot blue → door-open yellow → IDLE cyan pulse → RUNNING green breathing (boost = orange blink; battery < 20 % = yellow blink) → PURGE teal → **DONE solid green = the spec's completion LED** → FAULT fast red. `PIXEL_COUNT` chains an external strip on the same GPIO48 wire.
- **Completion summary (spec Step 11)**: DONE now captures final weight + duration; the dashboard shows a green batch-report card and the `/display` kiosk a big overlay — INITIAL / FINAL / TARGET weight, **MOISTURE REMOVED**, DURATION, "batch ready for packaging".
- New `cyclelog::cycleNo()/fileCount()` power the 50-cycle maintenance and 90 %-storage boot alerts; door close ends the ajar/rapid/unstable nags; `Dryer::finalG()/moistureG()/endElapsedS()` in `/api/data`.
- Docs: PARAMETERS (indication sections), PIN-CONFIG (+48, history v2.0.15), PINS-S3-PCF2, WIRING, diagram spare-pins box.

## 2026-09 (v2.0.14 — E-code fault system E01–E20 + keypad 5 = OK)

- **Owner decisions from the uploaded spec PDFs** (extract in `docs/SPEC-USER-DOCUMENTATION.md`): full E-code fault system now; 37-pattern buzzer engine next; on-board RGB pixel LED as the status light after that; keypad **5 = ENTER/OK alongside A**.
- **Every fault carries its spec code** — `faultWhy()` is now `E01 HEATER OVERHEATING …` so serial `[stat]`, the site, `/display`, and the cycle CSV note all show it with zero extra plumbing — plus a structured `FaultRec` (code, name, severity, since, detection value, limit, active) in `/api/data` renders as a **rich fault card** (red, manual reset, value/limit) and an **amber warning banner** (sev-0 faults, cycle continues).
- Recoded existing faults: E01 overtemp · E03 heater-failure (flat 3 min) · E04 sensors dead · E06 RH/fan · E07 door · E12 battery empty. **New detections:** E13 supply < 11.5 V 60 s (warn) · E14 supply > 15 V 30 s (halt) · E15 HX711 lost mid-cycle (halt) · E16 batch > 9.5 kg · E17 target ≥ batch or < 50 g — both refuse START · E20 > setpoint + 15 °C 60 s (warn) · E02 change-over refused under load · E11 feed reads DEAD (both from supply.cpp). E19 prefixed into the cycle end-reason. Reserved (needs hardware): E08/E09/E10; E18 by-design (inputs clamped).
- Keypad: **5 accepts like A** in the list menu (owner spec's 5 = ENTER/OK; edit screens keep 5 as a digit), OTA-info screen closes on 5 too.
- Docs: PARAMETERS gained the full E-table, manual 05 got the code line-up, root README diagnostics line, SPEC doc carries the implementation status.

## 2026-09 (v2.0.13 — the user's board is an N16R8: pin map made octal-PSRAM-safe + board-test sketch)

- **User's board: ESP32-S3-WROOM-1 N16R8** (16 MB flash, 8 MB octal PSRAM). Octal PSRAM occupies **GPIO 35/36/37** — so the optocoupler (35) and the DHT11 (planned for 35) moved: **opto → GPIO 18** (freed by the L298N fixed-direction change), **DHT11 outdoor → GPIO 33** (free since no lock/display). The build now touches no R8-conflicting pin, so **N8, N16 and N16R8 boards are all supported**.
- **New `arduino-ide/board-test-s3/` sketch** — flash it first on a new board: prints chip model, flash size, PSRAM size (proves octal PSRAM + the 35/36/37 warning), heap, MAC; then scans the dryer's I2C bus (GPIO 8/9 — AHT10 0x38, PCF 0x20/0x21 when wired) and live-reads the build's input pins once a second so wiring can be checked piece by piece from the serial monitor.
- Docs swept for the two pin moves: PIN-CONFIG (+history), PINS-S3-PCF2 cheat sheet, WIRING, PIN-MAP (board note rewritten — R8 no longer "avoid"), PARAMETERS, BOM (board row: any DevKitC), checklist (R8 caution reversed; items 11/12b/16; notes), manual 02 §4.11/§4.13, manual 09 boot log, diagram legend + spare-pins box.

## 2026-09 (v2.0.12 — loophole audit: compile-break fix, new gate check, interrupted-cycle trace)

- **Found + fixed a real compile break:** the platform's snapshot restore had dropped part of the v2.0.6 edit set — `cyclelog.cpp` printed `r.ot10/oh10` but `struct LogRec` never had those fields (and the CSV header had lost `out_t_C,out_rh_RH`). The assembled `.ino` would not have compiled on a user machine — and the gate never compiles C++, so it stayed green. All three points restored.
- **New gate step `tools/audit-symbols.py` (lost-edit detector):** every `X::` used must be a declared namespace/class/enum, every `r.<field>` in cyclelog.cpp must exist in `struct LogRec`, every `PIN_x` used must be defined in a config header. Zero false positives (raw-string HTML stripped); negative-tested — it flags the exact bug class it was built from. check-all.sh is now 5 steps.
- **Power loss mid-cycle no longer silently forgets the batch:** `cyclelog` marks the cycle start in NVS; if the unit reboots without a clean finish, the history gains a `cycle<stamp>-INT.csv` ("INTERRUPTED by power loss") and serial prints a `[warn]`.
- `[stat]` second fan figure relabelled `fanOut=` (was a second bare `fan=`).
- Docs truth-sweep: root README (one outlet fan, S3 sensor set, classic pin table — stale intake/TFT rows fixed, no-lock wording), SMART-DEHUMIDIFIER README (web display, AHT10+DHT22, Wire1 25/26 typo → 32/33, TFT pin rows, ENB-only L298N), PARAMETERS (duplicate stale `PIN_L298_IN3/IN4` row deleted, `PIN_TFT_*` row marked unused, `PIN_BUZZER` row corrected), manuals 05 (weekly-check row, buzzer GPIO), 08 (task list), 10 (ENB-only wiring row, sensor naming).

## 2026-09 (v2.0.11 — clock everywhere: kiosk display + keypad entry + serial)

- User call: date & time must be settable and visible everywhere, not just the dashboard's auto phone-sync. **`/display` kiosk page now shows a live ticking clock in its top bar** (weekday, date, HH:MM:SS, computed from the ESP epoch + tz) and **tapping it opens a set pane** (datetime picker + "use this device" + pointers to the site/keypad).
- **Keypad menu rows 6/7: "Clock date" (YYMMDD) and "Clock time" (HHMM)** — typed like any other parameter; validated, applied via the new `web::clockSetManual()` (settimeofday + flag + instant NVS persist). Row 8 = firmware update as before.
- `[stat]` gains `clk:YY-MM-DD HH:MM`, `[diag]` reports `clock <set time>|unset`, and the missing-sensor watchdog now names **DHT22#2** (was AHT10#2) on the S3 build.
- Dashboard Clock card note now lists all four ways to set the clock. PARAMETERS gained `cfg.tzMinutes`; manual 03 API table updated.
- (The clock chain itself — NVS save every 30 min, boot restore after full power-down, cycle-CSV timestamps — already existed and is unchanged.)

## 2026-09 (v2.0.10 — L298N fixed direction: ENB is the only fan wire)

- **User decision:** tie IN3→5V and IN4→GND **on the module** (fixed direction) and drive speed with ENB PWM alone. GPIO 18 + 21 come back as spares; the fan loom drops from 3 wires to 1 (+2 ties on the module).
- `PIN_L298_IN3/IN4` = −1 in both configs; the IN-pin pinMode/digitalWrite block is gone; `FAN_FIXED_DIR 1` + `FAN_PWM_FLOOR 40` (enable-PWM below ~40 % duty just hums — the floor clamps it, the existing kick-start still spins the rotor up).
- ENB LOW = a real off (enable gates the whole bridge — both switches open, zero current, fan coasts), so duty 0 % behaves exactly like a digital-pin off; the strict 10 s power-on window is unchanged. Hardware note: **remove the ENB jumper cap** and add a 10 kΩ ENB→GND so the fan stays off during the ESP32's boot milliseconds.
- Docs: manual 02 §4.3 rewritten (one-wire table + real-off explanation), WIRING §B/§G (also removes a stale "I2C1 x2" count), PIN-CONFIG, PIN-MAP, PARAMETERS (stale ENA/IN1/IN2 rows fixed), BOM, checklist, diagram spare-pins box.

## 2026-09 (v2.0.9 — chamber sensors: AHT10 top + DHT22 return; DHT11 outdoor)

- **User decision:** one AHT10 (chamber top) + one DHT22 (cool-return path = chamber source #2) + one DHT11 (outdoor). One AHT10 fewer to buy; I2C1 + GPIO 11 freed.
- **`src/dht22.*` → `src/dht.*`** — the driver now runs TWO devices with per-model decode (DHT22 tenths + sign bit, DHT11 integer bytes), per-device cadence (chamber 3 s, outdoor 10 s), 3-strike absence, generation counter so `sensors.update()` recomputes the instant a new DHT22 frame lands.
- **`sensors.*` keeps its whole API** (`s2ok/t2/h2`…): on the S3 source #2 is the DHT22 (`SENS2_DHT 1`), on classic it stays AHT10 #2 — control, web, CSV, kiosk page all unchanged.
- **Fixed a silently-missed v2.0.6 edit:** `getOutdoor()` never actually had the measured-first priority (the patch aborted on an earlier anchor). Now wired: DHT11 > phone weather > none.
- Boot: `[dht] DHT22 chamber sensor on GPIO10` + `[dht] DHT11 outdoor sensor on GPIO35`; `[diag] AHT10#1 … DHT22#2 … DHT11-out …`; `[warn] DHT on GPIOx stopped responding`.
- Docs: manual 02 §4.1 (chamber sensors) + §4.13 (both DHTs), 09 boot log, PIN-CONFIG (+stale PCF#3 mentions), PIN-MAP, WIRING (also fixes a leftover "PCF #1/#2/#3" line), PARAMETERS, BOM (AHT10 ×1, DHT22 return, DHT11 outdoor), checklist, QC, diagrams.

## 2026-09 (v2.0.8 — every display connection deleted from the build docs)

- User call: no display hardware at all, not even "optional later" wiring. TFT + UNO-bridge rows/sections removed from WIRING-S3-PCF.txt (§F gone, §B bus rows gone), PIN-CONFIG.txt (§4 = "none"), PIN-MAP.md (TFT + bridge rows, classic clash notes), manual 02 (§4.7 short, §4.12 retired), BOM (TFT + UNO rows deleted), checklist item 3 = removed, draw.io pages 1+3 (display/UNO boxes gone; freed-pins box = "SPARE S3 GPIOs").
- S3 spare pins now: 3, 33, 40–48 (+ 6/7/15/16/34/35/38/39 once the panel sits on PCF #2; 45/46 strapping kept free).
- BOM gained the missing **PCF8574 module #2** row (front panel @0x21 + 8 × 10 kΩ) — the component list now covers every part including both PCFs.

## 2026-09 (v2.0.7 — the website IS the display: /display kiosk page)

- **Hardware display removed from the build** (`DISPLAY_ENABLED 0`, `TXDISP_ENABLED 0` — TFT driver + UNO bridge compile out; S3 pins 40/41/42/47/48, 3, 33 free).
- **Two web pages now:** `/` = the full-control dashboard (unchanged) · **`/display` = a read-only kiosk page that behaves exactly like the hardware display** — state/times/temps/RH/outdoor/battery/heat/fan bars/weight+target+diff/door/supply/faults in huge type, 1 Hz refresh from /api/data, screen **wake-lock** so a mounted tablet never sleeps, "waiting for the dryer" watchdog. Header link "🖥 Display view" opens it.
- Keypad menu unchanged (echoes on serial); a phone or tablet mounted on the pillar becomes the screen — one less part, one less cable, display size = any device you own.
- Docs: manual 02 §4.7 rewritten (web display + kept SPI wiring for a future return), README, PARAMETERS, PIN-CONFIG, BOM/checklist (display optional), draw.io pages regenerated.

## 2026-09 (v2.0.6 — DHT22 outdoor sensor: real-time smart venting)

- **New `src/dht22.*`** — library-free DHT22/AM2302 driver (one data wire, µs bit-bang, 40-bit frames with checksum, 3-strike absence detect, 10 s cadence). GPIO 35 + 10 kΩ pull-up, **north-side shade** mounting. Classic: off (no free pin).
- **`getOutdoor()` priority changed: measured DHT22 beats phone-relayed weather** — smart venting now compares the chamber against *live* outdoor air. TFT "OUT" line, `[stat] out=tC/h%`, boot `[diag]` row, website weather card ("measured by the dryer"); the phone path remains the fallback exactly as before.
- Cycle CSV gains `out_t_C,out_rh_RH` columns (appended — old parsers keep working; blank when no sensor).
- Docs: manual 02 §4.13 (wiring + the GPIO-35-until-PCF#2 note), 09 boot log, PARAMETERS, PIN-CONFIG, WIRING-S3-PCF, PIN-MAP, BOM, checklist (item 12b).

## 2026-09 (v2.0.5 — front panel on ONE mixed PCF, no PCF #3)

- **User decision:** the whole front panel runs on a single mixed PCF8574 #2 @0x21 — P0/P1 supply relays, P2/P3 BUTTON-1/2, P4 SOLAR toggle, P5 optocoupler, P6 door limit switch, P7 buzzer. Same 8 S3 pins freed as the two-PCF plan (6, 7, 38, 15, 16, 39, 35, 34) → parallel display bus still fits with 5 spare.
- **Boot-safety wiring** (PCF powers up ALL-HIGH): relay module = ACTIVE-LOW (blue board) so power-up means both relays OFF; buzzer wired LOW-SIDE (+ to 3V3, − to P7) so power-up means silent; 10 kΩ pull-ups on all pins (the PCF only ever sinks).
- Docs rewritten to the single-PCF layout: WIRING-S3-PCF.txt (mixed-port rules + wire counts), PIN-CONFIG.txt (merged table, no PCF #3), PIN-MAP.md, design-workflow.drawio pages 1+3 + SVGs (PCF#2 box = front panel; new "S3 PINS FREED" box).

## 2026-09 (v2.0.4 — door: limit switch only, no solenoid lock)

- **User build has NO door lock** — a limit switch (GPIO34) provides open/closed status only. All door intelligence is software and unchanged: START refused until the scale is calibrated, batch weighed on door close (READY), **opening mid-cycle = instant FAULT**.
- Firmware: `DOOR_LOCK_ENABLED 0` + `PIN_DOOR_LOCK -1` (GPIO33 now free — display bus D6 or a future lock); lock pin fully guarded; wording everywhere now says "START LOCKED" / "OPEN DOOR = FAULT" instead of implying a physical lock (serial, website pill, TFT, 403 message).
- BOM/checklist: solenoid row marked OPTIONAL-not-fitted; QC 16 re-worded (start refused, not locked). Docs: manual 02 §4.10 rewritten, 03, PARAMETERS (+`DOOR_LOCK_ENABLED`), PIN-MAP, PIN-CONFIG.txt, both READMEs, variants README, draw.io pages 1–3 + SVGs regenerated.
- PCF #2 P2 is now spare (was going to carry the lock).

## 2026-09 (v2.0.3 — display bridge: parallel TFT via a companion Arduino)

- **Reuse the parallel display after all:** new `src/txdisp.*` streams the whole status screen as tiny text packets (~150 B/s, 1 Hz, 9600 baud) out of **S3 GPIO3** to an **Arduino UNO/Nano/Mega** that drives the D0–D7+WR+RD "UNO-shield" TFT. One-way wiring (S3 TX → Arduino RX pin 4 / Mega RX1 19, GND common, never the 5 V Arduino TX back). Classic variant: no free output pin → compiled out.
- **New companion sketch** `arduino-ide/display-bridge-uno/display-bridge-uno.ino`: auto UNO/Nano (SoftwareSerial) vs Mega (Serial1), MCUfriend_kbv + Adafruit GFX (the ONLY place in the project with libraries), renders state/times/temps/RH/battery/heat/fan bars/weight+target+diff/door/supply/fault, "waiting for the dryer…" watchdog screen.
- Docs: manual 02 §4.12 (wiring + libraries + power), PIN-MAP (GPIO3 row, updated free pins + warning now offers the bridge), components checklist + BOM rows.

## 2026-09 (v2.0.2 — OTA in the on-device menu + display purchase guard)

- **"6 Firmware update" added as the last actionable entry of the keypad/TFT menu** (spec): A opens a full OTA instruction screen — hotspot name/password, http://192.168.4.1 website path, and the Arduino-IDE network path — echo on serial too; # closes.
- **Display purchase guard (user hit this):** docs now warn loudly that **D0–D7 + WR + RD = 8-bit PARALLEL "UNO" module — incompatible** (no SPI pins; the S3 cannot spare 11+ GPIO for a parallel bus). Buy the SPI version (SCK/SDI or SDA/SCL labels). New **pin-label translator table** (SCK/SCL/CLK, SDI/DIN/SDA/MOSI, DC/RS/A0, LED/BL) in manual 02 §4.7 + PIN-MAP + BOM + components checklist; confirmed zero display libraries needed (own 26 MHz bit-bang driver).

## 2026-09 (v2.0.1 — network OTA + strict power-on pin discipline)

- **OTA, two ways:** the website upload page (existing, refused while RUNNING) is now joined by **Arduino-IDE network OTA** — PC joins WiFi "AgarbattiDryer", Tools → Port → "SMART-DEHUMIDIFIER at 192.168.4.1", Upload. Core built-ins only (`OTA_NETWORK_ENABLED` / `OTA_HOSTNAME` in config); mDNS failure falls back gracefully.
- **Unused pins parked (spec: "remaining all pins disabled"):** every pad no module owns gets INPUT_PULLUP (plain INPUT on input-only pads); flash/PSRAM/USB/UART0/strapping pins never touched — variant-aware via `CONFIG_IDF_TARGET_ESP32S3`.
- **Strict power-on sequence (spec):** ALL pins LOW at boot — including the door solenoid and BOTH supply-relay channels (new `door::engage()` / `supply::engage()`) — then the 10 s initialisation + calibration window runs with real work inside (first AHT10 reads, battery sample, HX711 detection with saved calibration, splash, power-on beeps), then the relay + lock engage per state and normal operation starts. `[init]` log lines tell the story.

## 2026-09 (v2.0 — operator spec build: power hardware, menu, modes, bursts)

- **Pin quick-reference inside both .ino files:** the assembler now renders the variant's full pin table (GPIO × component, parsed from the config at build time — can never drift) plus module notes (scale/door/relay off on classic) into the sketch header; banner text refreshed to v2.0.
- **`docs/PIN-MAP.md`:** every component × every pin for BOTH variants (signals, directions, addresses, polarities, power wiring, v1.8→v2.0 pin history) — the wiring source of truth; linked from the root README and manual 00.
- **Post-release:** `production/06-COMPONENTS-CHECKLIST.md` — printable tick-box checklist (35 items in 5 buy phases, totals, tools, on-delivery traps: PCF8574-vs-A, AHT10 clones, S3 R8 PSRAM/GPIO35, buck pre-set, load-cell mounting); BOM de-duplicated + v2.0-corrected (one fan, buzzer fitted, door lock/limit-switch/supply rows).

- **Power (hard off + supply selector):** P-MOSFET soft-latch (hold asserted at boot); BUTTON-1 3 s = hard power off, 10 s = reboot; BUTTON-2 = AGARBATTI preset + start; SOLAR toggle + optocoupler live-check + 2-channel feed relay (S3 GPIO 6/7), switched only with loads quiet — change under load refused + 5 s error beep; dead feed = red mode + warn.
- **On-device menu (keypad + TFT):** B = menu, 2/4/6/8 navigate, digits enter values, A = save, # = back, * = home; C = mode cycle, D = run/stop. Editable: temperature (40–80), time, RH target, target weight.
- **Modes:** AGARBATTI DEFAULT (60 °C × 120 min) · USER DEFINED · SILICAGEL DEFAULT (80 °C × 120 min) — keypad C, BUTTON-2, website buttons, serial `mode`.
- **Fan law replaced (single outlet fan):** RH ≥ 60 % for 1 min → 100 % for 60 s (fanTrigRH/fanTrigMin/fanBurstS adjustable); intake fan removed (pins freed for the power hardware); purge unchanged.
- **New faults (spec):** heater failure = temperature flat 3 min while heating at 80 %+ duty; fan error = RH ≥ 60 % for 5 min despite bursts.
- **Target weight:** targetG setting; live initial/target/diff on TFT + site; 5 min before time-up a warning beeps and suggests the +minutes needed (from the live g/min rate); at time-up within 5 % = complete, else DONE-WITH-WARNING (time is the master).
- **Boot:** all outputs LOW, splash "ARCHITECTS OF SOLUTIONS / SMART DEHUMIDIFIER", 2/s × 3 s power-on beeps, 10 s initialisation window; full beep set (3 s start · 5 s end · 5 s errors · 1 s door · 3 s mode change); buzzer fitted by default (classic 13, S3 38).
- **TFT overhaul:** supply + parameter mode in the title, elapsed + remaining, weight line with target/diff, fan ON/OFF/%, external temperature, cycle spark-line graph, menu screens.
- **Website:** header brand, SOLAR/BYPASS pill (red when the feed is dead), mode preset buttons, target-weight + fan-burst fields, weight tile target/diff, setTemp clamped 40–80.
- Settings v7 (v6 blobs → defaults; press Reset all to defaults once after flashing); FW_VERSION 2.0.0; serial `mode` / `supply switch` / `power off`. S3 config un-staled (TIME_SAVE_MS was missing since v1.7 — the S3 sketch would not have compiled). Classic pins 13/15/16/18/27/35 repurposed (buzzer, buttons, latch, toggle, opto). Docs: manual 02 §4.11, 03, 05, 09; PARAMETERS; both READMEs; variants README; BOM; QC 17–18; datasheet.

## 2026-09 (v1.8 — door workflow + hardware error detection)

- **`src/door.*` + calibrate→load→ready workflow**: door LOCKED until the scale is calibrated (known weight); calibration unlocks it; closing the loaded door weighs the batch (diff vs tare) → READY + grams; Start refused everywhere before that (site 403, keypad, BOOT, serial); locked again during RUNNING/COOLDOWN; **opening mid-cycle = instant FAULT**; unlock at DONE/FAULT. S3 hardware: lock GPIO33 + reed GPIO34; classic = same flow in software.
- **Hardware error detection** (edge-triggered `[warn]`s): AHT10 frozen-at-one-value (clone sensors), #1/#2 disagreement ≥15 °C, heater-ineffective (90 %+ duty 10 min, no rise), battery ADC ~0 V, battery over-voltage vs chemistry, door reed never closed, scale restless at idle, low heap (<40 kB).
- **Boot self-test**: `[diag]` block — both AHT10s, keypad, scale + calibration state, battery V/validity, door fitted/closed, heap.
- Console: `tare`, `cal <g>`, `door unlock` (service override); `[stat]` gains `door:PHASE/LCK`; website door pill (🔒 CALIBRATE / 🔓 LOAD / ⚖ READY n g / 🔒 DOOR LOCKED); TFT gains the door/batch line.
- Honest limits documented: fan RPM, heater current, display backlight are NOT firmware-detectable without extra hardware (manual 05).
- Docs: manual 02 §4.10, 03 workflow, 05 error rows, 09 boot log, PARAMETERS, README, variants README, datasheet safety chain (+2), QC test 16, safety checklist (+3).
- **Root README rebuilt as the full single-page documentation** (15 sections: concept, specs, workflow, features, both pin maps, parameters, safety, error detection, build, flash/OTA, operation, troubleshooting, repo map, roadmap, license); FW_VERSION synced to 1.8.0.

## 2026-09 (v1.7 — clock persistence)

- **Wall clock survives full battery disconnects**: epoch saved to NVS every 30 min and on every phone sync (`clockTick`/`clockSave`); restored at boot (`clockBoot`) — a power-cycled unit shows the *last-saved* time instead of 1970.
- `clockSet` now means "a phone synced THIS boot": a restored-stale clock still reads unset, so the first dashboard visit always auto-corrects the drift.
- Refuses junk epochs (< 2023-11); cycle files keep sequence-number names until a real sync (cyclelog already had the fallback — now the timestamps behind it are sane too).
- Manual 05/09 + PARAMETERS (`TIME_SAVE_MS`) updated.

## 2026-09 (v1.6 — weigh scale: dry-to-weight)

- **`src/scale.*`**: library-free HX711 driver (25-pulse read, 2 Hz, 40 ms timeout), presence detection, tare + known-weight calibration persisted in NVS, 5-minute rolling rate window (g/min).
- **Dry-to-weight completion gate**: cycle ends when |rate| < weightRateG (default 2 g/min) for weightMinY (default 10 min) — the batch itself says when it's dry. Falls back to time+RH with a one-time `[warn]` if the scale is absent.
- **Website**: Batch-weight tile (grams + g/min + SETTLED badge), Tare / Calibrate-with-known-weight buttons (`/api/scale`), 3 new Parameters (requireWeight, weightRateG, weightMinY), weight graph series + `wt_g` CSV column.
- **Pins**: S3 CLK=1/DOUT=2 (enabled by default); classic ESP32 has no free output pin with the display on — SCALE_ENABLED 0 + options documented (display off, or S3).
- Settings v6 (5 new fields — Reset all to defaults once after flashing); `[stat]` gains `wt=…/…g/min`; miss-list + `[warn]` health; QC test 15; docs + BOM + future/01 marked BUILT.

## 2026-09 (v1.5 — ESP32-S3 variant + service features)

- **`variants/esp32-s3/`**: full S3 config (own pins for display/relays/buzzer/keypad — no more sharing, I2C 8/9 + 10/11, BTS 12/13, L298N 14–21, battery 4/5, relays 6/7, buzzer 38, TFT 40/41/42/47/48), LOG_MAX 2160→6480 (18 h curve), board-settings guide (USB CDC On Boot), why-S3 README + wiring deltas.
- **Assembler variants**: `node tools/single-file/assemble.js s3` → `arduino-ide/SMART-DEHUMIDIFIER-s3-single-file/…ino`; classic output unchanged; CI + check-all build & scan BOTH.
- **USB/serial command console** (both variants): `help start stop power stat | temp <C> time <min> knob <%> defaults` — service and demos without the phone.
- **BOOT button = start/stop** (S3): short press start/stop, 2 s press power-on; zero wiring.
- BOM gains the S3 devkit row; START-HERE/PARAMETERS/root README updated.

## 2026-09 (v1.4 — on-unit interface: display + keypad)

- **3.5" ILI9488 SPI TFT (480×320, non-touch) — RE-ADDED hex keypad.**
- `src/display.*`: library-free ILI9488 driver (remapped SPI on GPIO 12/0/2/17/23 — the ESP32's normal SPI pins are taken by the dryer hardware), 5×7 font, 1 Hz status screen: state, MAX HEAT-UP badge, T1/T2/AVG/RH, battery, time left, heat/fan bars, fault line. Note: display occupies the relay/buzzer pin slots (DISPLAY_ENABLED 0 frees them); brief garbage frame at power-on is normal (strapping pins).
- `src/keypad.*` re-added (was removed in v1.0): PCF8574 @0x20 on Wire (no new pins — no conflict with AHT10s), 15 ms debounce, A=start B=stop C=power D=+15 min, health in `[warn]`/`[stat]`/miss-list, `kp{ok,last}` in the API, "keypad last key" row on the website.
- Docs: manual 02 §4.7/§4.8 wiring, 03 module rows, 05 error rows, 08 panel task, 09 boot log, PARAMETERS pins/switches, README pin table, datasheet, BOM (display + keypad rows), future/01 load-cell pin-conflict fix (second PCF8574), CHANGELOG.
- Tests: +keypad assertion (54 embedded, 20 online PASS); scans CLEAN; .ino 3736 lines.

## 2026-09 (v1.3 — product-ready repository)

- **`production/`**: full BOM with costs (CSV) · printable assembly sign-off checklist · 14-point QC test procedure with pass criteria · serial/label/packaging scheme · warranty card template.
- **`demo/`**: the 10-minute government/investor demo script (minute-by-minute + Q&A ammo) · product one-pager.
- **`compliance/`**: safety checklist (every protection + how verified) · standards/certification roadmap.
- **`media/`**: AI pillar concept render + photo shot-list.
- **`scripts/check-all.sh`**: one-command release gate (regenerate → scans → suites → generated-sync check).
- **CI workflow** (`.github/workflows/ci.yml`): the same gate on every push; issue templates; root README is now a product landing page; CONTRIBUTING + SECURITY added.

## 2026-09 (v1.2 — commercial product decisions)

- **Pillar geometry finalised**: 6 ft (1830 mm) standard / 8 ft option, 30 × 30 cm section — battery bay in the base (ventilated, sealed bulkhead), controller bay at the top, solar panel external. Manual 10 fully re-dimensioned.
- **Battery decision**: default profile → **4S LiFePO4 (12.8 V, 50–100 Ah, ≥ 50 A BMS)** — longevity (2000+ cycles) + safety + cost-per-cycle; SLA documented as the budget alternative. `DEF_BATT_TYPE 3`.
- **License**: MIT (open — government problem statement, commercial-friendly).
- **Roadmap reordered rural-first**: no-internet upgrades first (load cells, recipes by type, solar telemetry); Telegram demoted to phone-data-optional.
- **Recipe spec gains the built-in library by incense type** (Sandal/Masala/Charcoal/Flower/Durbar/General).

## 2026-09 (v1.1)

### Firmware
- **OTA firmware updates**: website → **⬆ Firmware update** (`/update`) — upload a compiled `.bin` straight from the phone/laptop browser over the hotspot; progress bar; refused while a cycle is RUNNING; auto-reboot on success. No USB needed ever again.
- **heaterMax default 100 %** — the full 500 W (drop the cap on the Parameters page only if wanted).

### Product
- Firmware version plumbing: `FW_VERSION` ("1.1.0") on the serial banner, `/api/data`, and the website header — versions matter now that updates are OTA.
- `docs/manual/00-START-HERE.md` — one-page documentation map.
- `future/` — **future-ready plans sub-folder**: roadmap (phases + ground rules) and 7 specs: load cells (dry-to-weight), recipe presets, Telegram alerts, solar/MPPT telemetry, PWA, multi-pillar fleet, learning-from-batches.
- README project layout corrected (library-free, complete tree).

### Documentation
- `docs/manual/10-FULL-BUILD-EVERY-INCH.md` — every-inch build: dimensioned pillar, cut list, coil forming, sensor placement (95 °C cool-return trick), full wire schedule, control-box layout, 10-step assembly.
- `docs/PARAMETERS.md` — every parameter (20 runtime + all compile-time): range, default, effect, cautions, interaction map.

## 2026-09 (v1.0 "Pillar")

### Product platform
- **Pillar class**: 550 W solar panel → 500 W heater (0.32 Ω nichrome, BTS7960 43 A, heatsink mandatory); 70 % duty cap keeps continuous draw at ~350 W (33 A PSU).
- **Temperature range raised**: recipes 45–95 °C, hard cut 95 °C, chamber rated 100 °C; default target 80 °C.
- **AHT10 hot-guard**: `[warn] AHT10 #n HOT` at 82 °C; >85 °C recipes require cool-return-path sensor mounting (see `docs/datasheet.md`).
- `docs/datasheet.md` — full product datasheet (power story, temperature zones, safety chain, pillar build notes).

### User interface
- **Background personalisation**: 🎨 picker in the header — 6 preset shades, custom colour, hair-lines on/off; persisted per phone.
- **Elemental shiny tabs**: glowing blue borders; Dashboard = fire (gold-orange), Parameters = water (blue); animated sheen on the active tab.
- **One-page Parameters**: every setting editable in one place + **↳ Reset all to defaults** (one click) + factory reference table.
- **Built-in typical 5-day forecast** derived from the real date & time (badge TYP.); live phone-relayed weather still wins.
- Blue/gold/silver theme; gold manual-heat knob; ring gauges recoloured.

### Firmware
- **Manual heat knob**: `/api/heat?d=` drives the exact BTS duty for 60 s, then the MCU auto-releases to boost/PID; safety cuts always active.
- **Pin remap** to the built board: BTS EN=26, L298N ENA=13, IN1=15, IN3=5, IN2=18, IN4=4, RPWM=25, ENB=14, I2C 21/22 + 32/33.
- **Relays + buzzer not fitted** (compile-out flags `RELAYS_ENABLED` / `BUZZER_ENABLED`, default 0); `AUTO_START` off — manual start from the website.
- DHT22 outdoor sensor and hex keypad modules **removed**; outdoor data = phone forecast → stale → manual.
- Serial diagnostics: `[stat]` every 15 s + edge `[warn]`s; `[man]`, `[cfg]`, `[batt]` event lines.
- Diagnostics boot-crash fixed (`[stat]` printf argument-order NULL deref) + `tools/verify_strings.py --printf` format/arg-type checker added (count + %s-vs-numeric guards).
- String-literal and unsigned-cast compile fixes across cyclelog/control.

### Tooling & CI
- `tools/verify_strings.py` — raw-newline scanner + printf checker (validates against the historical crash).
- UI test suites: 50 embedded + 19 online assertions, jsdom-based, run offline.
- `.github/workflows/pages.yml` — Pages deploy for the online UI.
- `tools/single-file/assemble.js` regenerates the IDE sketch; `tools/online/sync-online.js` regenerates `docs/index.html`.

## Earlier milestones
- Live web configuration, `[cfg]`/`[batt]` logs, boost heat-up + MAX badge, gentle-airflow channels, frozen pin map, pro UI + multi-series graph, single-file library-free IDE sketch.
