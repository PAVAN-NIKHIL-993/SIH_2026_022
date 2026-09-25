# 05 — Errors and Fixes (symptom → cause → fix)

> **E-codes (v2.0.14):** every fault now carries its owner-spec code —
> `E01` overheat · `E02` change-over refused · `E03` heater failure ·
> `E04` sensors dead · `E05/E16` scale · `E06` RH/fan · `E07` door ·
> `E11` feed dead · `E12/E13/E14` battery/supply · `E15` HX711 ·
> `E17` invalid target · `E19` timeout · `E20` over-band. Amber banner =
> warning (cycle continues), red banner = halted (fix, then Power On).
> Full table: `docs/PARAMETERS.md`.
>
> **Degraded mode (v2.0.17):** `E15` (scale/HX711 lost or absent) and a
> single-dead chamber sensor (`E04` warn) no longer stop the machine — the
> batch keeps drying on time + wet-RH completion, with nag beeps, the amber
> banner and a yellow blinking status LED until it's fixed or the tech
> arrives. Recovery (scale back, second sensor back) clears itself.
> Both chamber sensors dead, overheat, door-open and battery-empty still
> halt the machine — degraded never overrides a safety stop.

## A. Compile / upload time

| Symptom | Cause | Fix |
|---|---|---|
| (historical) `'ArduinoJson' no such file` | old code revision | current code is **dependency-free** — no library needed at all; if you use an old checkout, install ArduinoJson v7 or update |
| `TwoWire has not been declared` | wrong file opened in Arduino IDE | open the generated single-file sketch (`smart-dryer-single-file.ino`), not a single .cpp |
| A fatal error during `pio run` first time | platform downloading (needs internet once) | retry; or use Arduino IDE path |
| Upload stuck on `Connecting....____` | bootloader not auto-triggering | hold **BOOT** on the ESP32 until upload starts, release |
| Port not showing | charge-only USB cable / driver | use a **data** cable; install CP210x or CH340 driver |
| `serial port busy` | Serial Monitor open elsewhere | close other monitors, replug |
| Sketch too big | never happens today (~0.9 MB of 1.2 MB) | if you bloated it, partition scheme "No OTA (2 MB)" in IDE |

## B. Boot / sensors

| Symptom | Cause | Fix |
|---|---|---|
| `[sens] S1=MISSING` | wiring / swap | SDA↔SCL swapped, no 3V3, or module dead. Test sensor alone on 21/22 |
| Both sensors MISSING but one works alone | **both wired to one bus** | AHT10 has a FIXED address — #2 must go to GPIO25/26 (Wire1) |
| Sensor reads −40 °C / 0 %RH constantly | clone module / broken | replace; genuine AHT10 costs ~₹100 |
| Readings flat, never change | sensor saturated by condensation | move it out of dripping path; seal cable entry with silicone |
| `[keypad] not found` | address / variant | module must be **PCF8574 at 0x20**; PCF8574A collides with AHT10 — buy the right one |
| `[warn] weight scale no data` / wt `--` | HX711 absent / wiring / pins | S3: CLK→1, DOUT→2, 3V3, GND; classic: scale needs DISPLAY_ENABLED 0 (no free pin) — see manual 02 §4.9 |
| Weight jumps while fans run | vibration on the cells | raise weightMinY; stiffen the tray rails; damp the fans' kick-start |
| `[start] REFUSED` / door stays locked | by design: scale not calibrated yet | website or serial `cal 1000` with a known weight on the trays — door unlocks on success (service override: serial `door unlock`) |
| `[warn] AHT10 #n FROZEN` | clone/dead sensor stuck at one value | replace the sensor (genuine AHT10 ~₹100) |
| `[warn] AHT10 #1/#2 disagree` | placement/wiring | check both sensors sit in the chamber air path, not one in the coil plenum |
| `[warn] HEATER INEFFECTIVE` *(v1.x; v2.0 = FAULT `Heater failure: temperature constant for 3 min`)* | coil open / fuse blown / PSU off / EN wire — check with the knob at 100 % and a clamp meter |
| `[warn] battery ADC reads ~0 V` | divider wiring / ADC pin | check GPIO4 (S3) / GPIO36 (classic) divider + the 2N7000 gate |
| `[warn] battery OVER-VOLTAGE` | wrong battType or wrong charger | match Parameters → battery type to the actual pack |
| FAULT `Door opened during the cycle!` | door opened mid-cycle | by design: heat stops, purge, power off; close the door, Power On, Start |
| FAULT `Heater failure: temperature constant for 3 min` | coil / 50 A fuse / PSU / EN dead while heating | knob 100 % + clamp meter; check the fuse first |
| FAULT `Fan error: RH above 60% for 5 minutes` | fan dead or duct blocked (RH won't drop despite bursts) | spin test by hand (power off); check ENB/IN3/IN4 wiring |
| `[supply] REFUSED` + 5 s error beep | solar⇄bypass change while a supply is under load | by design: stop the cycle, then flip the toggle |
| `[warn] target weight not reachable...` | at the current g/min rate the target needs more time than remains | extend time (site/keypad) or accept DONE-WITH-WARNING |
| Cycle ends `done - target weight NOT reached` | weight still >5 % above target at time-up | by design (time is the master): re-run if the batch needs it |
| Pillar won't power on | soft-latch: BUTTON-1 press too short, or hold wire loose | press BUTTON-1 firmly ~1 s; check gate/100 kΩ + the latch-hold wire |
| `[warn] feed reads DEAD` (opto) | selected supply absent | check the panel/bypass source; the name turns red on screen/site |
| **Not detectable in firmware** (no extra hardware): fan RPM (needs a tach pin), heater current (needs a shunt) | — | the weekly check covers these: fans spin visibly, coil glows at knob 100 %, `/display` page updates live |
| Buzzer chirps once at plug-in | old build had the buzzer on GPIO15 (strapping pulse) | current build: classic GPIO13 / S3 GPIO38 (PCF#2 P7 with the front panel) — chirp gone; on an old build it was harmless |

## C. Control behavior

| Symptom | Cause | Fix |
|---|---|---|
| Temp overshoots > 3 °C | too much Kp / big coil | Kp 10→6, Kd 5→8; lower heaterMax; add fanMin circulation |
| Never reaches setTemp | coil undersized / leaks | seal door, insulate; bigger coil (04 §3 sizing); raise Kp |
| Duty pinned 100 %, temp stalls low | heat escaping as fast as it enters | check door seal, vent caps open, coil voltage matches rail |
| Temp oscillates ±2 °C constantly | Ki too high | Ki 0.2→0.05 |
| RH never falls below humLow | door leak / too-wet outside air | seal door; smartVent is ON? outside RH is shown on the card — if ≥ chamber RH the fans are intentionally held low; dry at a different hour |
| RH falls, fans never stop | humLow set too low | raise humLow (typical end-of-dry RH 30–45 %) |
| Chamber smells cooked | sticks too close to coil / setTemp high | 45 °C max for perfume batches; rack clearance ≥ 10 cm |
| Sticks crack/curl late in cycle | dried too fast | lower setTemp 2–3 °C, higher fanMin (gentler gradient) |

## D. Power / drivers

| Symptom | Cause | Fix |
|---|---|---|
| Fans weak | L298N drops ~2 V (design fact) | accept ~80 % speed, or 15 V rail, or see 06 §B for the upgrade path |
| Fans don't spin | ENA/ENB jumpers still on | **remove both jumpers** (they hold EN high, but then PWM pin does nothing / or module variant) — jumpers OFF, pins GPIO32/GPIO14 |
| One fan dead | wiring / L298N channel | swap fan to other channel to isolate |
| Heater never on | EN not driven / fuse / LPWM | GPIO18 → R_EN+L_EN must be connected; fuse continuity; GPIO17 (LPWM) must be LOW — it is, but check the jumper wire |
| Heater full-on regardless | wrong module pins | RPWM/LPWM swapped with EN; also check `RELAY_ACTIVE_LOW` matches your relay |
| ESP32 reboots when heater kicks | brownout from shared thin supply | buck input direct from battery with 470 µF cap; don't power ESP from L298N 5 V pin |
| Battery % jumps around | ADC noise / load swings | enable the transistor gating (19), calibrate `VBAT_CAL_OFFSET` at rest; ±5 % under load is normal (06 §C) |
| Relay buzzes | marginal coil drive | power relay module from 5 V buck, not ESP 3V3 |

## E. Website / network

| Symptom | Cause | Fix |
|---|---|---|
| Can't find the hotspot | AP not up | check serial `[web] AP ... up`; wait 10 s after boot |
| Connected but page won't open | captive portal blocked | type **192.168.4.1** manually in the browser |
| "OFFLINE" badge | phone left range / ESP rebooted | walk back in range; page auto-reconnects |
| Clock shows wrong time | phone never pushed it | open dashboard once with the phone (auto-push), or Custom → Clock manual |
| After a full battery disconnect the clock is "old" | expected: last-saved epoch restored (NVS, 30-min snapshots) | open the site once — the phone corrects it automatically; fresh units use sequence-number file names until then |
| Live weather stays "no data" | phone has no mobile data on hotspot | enable "mobile data always" for this WiFi (Android: keep both on), or set town in the card, or use manual entry |
| Past cycles empty | no completed cycle yet / cleared | cycles are written when a run ENDS (also on fault/stop) |
| Cycle CSV download empty | download blocked by browser | use the ⬇ CSV link per row (online UI pipes downloads) |

## F. Online mode (GitHub Pages)

| Symptom | Cause | Fix |
|---|---|---|
| `/online` falls straight back to `/` | phone has no internet in 7 s | check mobile data; the fallback is by design |
| Pages site 404 | Pages not enabled / wrong source | Settings → Pages → Source: **GitHub Actions**; check the workflow ran |
| Site loads, no live data | opened the github.io URL directly | browsers block https→http; open via **http://192.168.4.1/online** |
| Backup: 401/403 | PAT lacks permission | fine-grained token, Contents: **Read and write**, on that exact repo |
| Backup: 422 then fail | file exists, sha fetch failed | re-run — the script auto-updates by sha; 422-then-update is normal |
| Changes to website not on Pages | forgot to regenerate | `node tools/online/sync-online.js`, commit `docs/index.html`, push |

## G. Data / history

| Symptom | Cause | Fix |
|---|---|---|
| Cycle file name is `cycle-0000000012.csv` | clock was never set before that run | connect phone once — later files get real timestamps |
| `LittleFS.begin` slow first boot | initial format | one-time, up to ~10 s; later boots are instant |
| History older than 40 runs gone | auto-prune (design) | they should already be in GitHub via backup — run Back up regularly |
| Chart empty after refresh | `/api/history` only has the current run | normal: RAM log resets each cycle; saved history is in Past cycles |