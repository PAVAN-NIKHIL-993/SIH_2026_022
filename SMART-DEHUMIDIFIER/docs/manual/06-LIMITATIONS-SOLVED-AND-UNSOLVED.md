# 06 — Every Disadvantage, Honestly: solved / mitigated / open

Status legend: ✅ solved in this repo · 🟡 mitigated (reduces risk, not zero) · 🔴 open (accepted trade-off or future work → ADVANCED-IDEAS.txt)

## A. Architecture-level

| # | Disadvantage | Status | Explanation & fix |
|---|---|---|---|
| 1 | ESP32 has no internet → no cloud dashboards | ✅ | By design: phone's browser is the bridge (weather in, Pages UI out, GitHub backup out). Works because phones keep mobile data while on the hotspot |
| 2 | Browser https→http block would kill the Pages UI | ✅ | Solved with the `/online` postMessage bridge page |
| 3 | No real-time clock | 🟡 | Phone pushes time automatically; if never connected, files fall back to counter names. Zero-cost fix: connect once. Permanent: DS3231 RTC (₹150) |
| 4 | Dashboard needs someone in WiFi range (~30–50 m) | 🔴 | By design (offline-first). Remote: Telegram/LoRa/SIM800L in ADVANCED-IDEAS 6.x |
| 5 | One chamber, one ESP | 🔴 | ESP-NOW mesh of dryers is the scale-up path (6.2) |

## B. Driver electronics

| # | Disadvantage | Status | Explanation & fix |
|---|---|---|---|
| 6 | L298N wastes ~2 V (fans at ~80 % speed, heat) | 🟡 | Classic L298N reality. Mitigations: 15 V fan rail, or bigger fans. Real fix: swap L298N for a MOSFET driver (future; wiring-compatible) |
| 7 | BTS7960 needs airflow above ~10–15 A | 🟡 | Mount it in the airflow path / add a small heatsink fan for big coils |
| 8 | Relay contact rating vs big coil current | 🟡 | ≤ 10 A coil → relay module fine; above → use the relay to drive a **contactor**. DC arcs are brutal: fuse everything |
| 9 | No galvanic isolation on sensor lines | 🟡 | All low-voltage side; isolation matters only if you add grid bypass wiring — get an electrician for the grid side |

## C. Measurement

| # | Disadvantage | Status | Explanation & fix |
|---|---|---|---|
| 10 | ESP32 ADC is nonlinear (±5–10 %) | 🟡 | Battery % is indicative: 16× oversampling + calibration offset; per-chemistry windows. Precise: INA226 coulomb counter (1.6) |
| 11 | Battery % from voltage under load lies | 🟡 | Load shifts voltage. Read % with heater OFF moments (purge) or accept ±5 %. Fix: coulomb counting (1.6) |
| 12 | Two AHT10s can't share one bus | ✅ | Solved by design: Wire + Wire1 (21/22, 32/33) — but it's the #1 assembly error, hence the boot log check |
| 13 | %RH is temperature-dependent | 🔴 | Control is on %RH bands; smarter absolute-humidity control is a ~30-line upgrade (1.5) |
| 14 | No product-moisture measurement | 🔴 | Time+RH is a proxy. Load cells = dry-to-weight (1.1, top recommended upgrade) |

## D. Control & safety

| # | Disadvantage | Status | Explanation & fix |
|---|---|---|---|
| 16 | Safety is software (maxTemp, sensor-fail, battery cut) | 🟡 | Three independent software cuts + fault beeps. **Hardware backstops mandatory anyway**: fuse + bimetal thermostat (BOM) — a stuck MOSFET/relay is not software-fixable |
| 17 | PID gains are per-chamber | 🟡 | Defaults suit 60–100 L @ 100–300 W; tune per 04 §2. Auto-tune is future (2.1) |
| 18 | Door-open RH spike can confuse the PID | 🟡 | Integration clamps limit damage; dedicated door detection is future (2.3) |
| 19 | RAM log lost if power dies mid-cycle | 🟡 | Cycle file writes at END; a crash mid-run loses that run's graph (settings + old cycles survive in NVS/LittleFS). Fix: periodic flush (future) |
| 20 | maxTemp fixed at 60 °C default | ✅ | Website-adjustable (setTemp+5 … 110), per recipe |

## E. Data & history

| # | Disadvantage | Status | Explanation & fix |
|---|---|---|---|
| 21 | Only 40 cycle files kept | ✅ | Auto-prune by design; GitHub backup = unlimited archive |
| 22 | GitHub backup needs a PAT in the browser | 🟡 | Fine-grained token scoped to ONE repo, Contents RW only, in localStorage, revocable. Don't share the phone; logout/backups clear it |
| 23 | Weather refresh only while a browser is open | 🟡 | By design (phone = bridge); manual entry covers no-phone periods |
| 24 | No analytics over batches | 🔴 | CSVs + GitHub Actions can graph trends today (4.4); learning system is 2.6 |

## F. Web interface

| # | Disadvantage | Status | Explanation & fix |
|---|---|---|---|
| 25 | Embedded site needs reflash to change | ✅ | The Pages UI (via `/online`) is editable by git push, no reflash |
| 26 | No login on the dashboard | 🔴 | Anyone on the hotspot (WPA2, password `dryer1234` — change it in `config.h`!) can control it. PIN levels = 5.6 |
| 27 | Notifications only while page is open | 🟡 | Web Notifications work with the page open; remote alerts need the bridge (6.1) |
| 28 | 4 hotspot clients max | ✅ | Enough (phone + laptop + tablet); raise `AP_MAX_CLIENTS` if needed |

## G. Power system

| # | Disadvantage | Status | Explanation & fix |
|---|---|---|---|
| 29 | "BMS" confusion: protection ≠ charging | ✅ | Clarified in BOM + wiring: charge controller (panel→battery) AND protection BMS, or one combined board |
| 30 | Heater draws from battery when cloudy | 🔴 | Timer-based runs use battery. Solar-following duty = 3.1 (top-3 pick) |
| 31 | Vented lead-acid needs ventilated box / Li-ion needs respect | 🟡 | Battery box shaded+ventilated; never puncture/p short; fuse at the battery + |

## H. Component-specific gotchas discovered during THIS build

| # | Gotcha | Status |
|---|---|---|
| 32 | ADC2 pins die when WiFi is on → battery sense HAD to be on ADC1 (GPIO36) | ✅ designed around |
| 33 | GPIO2 bypass pin drives the onboard LED — it flickers at boot, then doubles as the bypass indicator | ✅ |
| 34 | Buzzer moved to GPIO17 — no strapping chirp; GPIO15 now serves L298N IN1 (strapping-safe as an output after boot) | ✅ |
| 35 | Fan pins chosen on non-strapping pins so fans don't twitch during boot | ✅ |
| 36 | Charge controller load outputs often current-limited (< battery feed) — power the heater rail from the battery via fuse, not the controller's LOAD terminal | ✅ wiring doc |

## I. The honest bottom line

This design gives you: industrial-style interlocks, logged quality data,
solar operation, a zero-cost cloud, and a website you can reshape from a
phone — on a hobby budget. Its real limits are: no product-moisture
feedback (load cells fix), no remote alerting (bridge fixes), L298N losses
(MOSFET fixes), and software-only safety (bimetal + fuse are your
non-negotiable hardware backstops). Everything else is polish.
