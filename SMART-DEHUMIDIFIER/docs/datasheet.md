# SMART DEHUMIDIFIER PILLAR — Product Datasheet

**Solar agarbatti drying pillar · 550 W panel class · ESP32-controlled**
Version 1.0 · 2026-09

---

## 1. Concept

A vertical, pillar-shaped drying chamber that runs entirely on one solar
panel. Load the sticks, press Start (or enable auto-start), and the pillar
does the rest: full-power heat-up, precise PID hold, humidity-driven
ventilation, safety supervision, and a documented, downloadable record of
every cycle.

## 2. Specifications

| | |
|---|---|
| Chamber | **6–8 ft pillar** (30 × 30 cm cross-section): battery bay in the base, controller bay at the top, drying chamber between; solar panel external |
| Solar input | 1 × **550 W panel** (typ. 41 Vmp / 13.4 A) via charge controller → 12 V battery bank |
| Heater | **500 W class** — 0.32 Ω nichrome (two 0.64 Ω halves in parallel, ~2.3 m of 1.0 mm wire) |
| Heater driver | BTS7960 43 A H-bridge, 1 kHz PWM through the 555 level shifter; heatsink mandatory |
| Continuous power | **full 500 W at 100 % duty** — the 550 W panel plus battery carry the 41.7 A peaks; fuse at 50 A |
| Temperature range | recipes **45–95 °C**, hard safety cut 95 °C, chamber rated to **100 °C** |
| Control | full-power heat-up → PID (Kp/Ki/Kd tunable) → humidity-band fan law → smart venting → purge → power off |
| Sensors | 2 × AHT10 (top + bottom of chamber, separate I2C buses), battery divider on ADC |
| Fans | 2 × 12 V fans on L298N, independent intake/exhaust scaling, kick-start, 100 % purge |
| User interface | built-in hotspot website (192.168.4.1): dashboard, gauges, multi-series graph, one-page parameters with one-click factory defaults, manual heat knob (60 s), 5-day typical forecast, background restyling; optional GitHub-Pages online UI · **3.5" TFT status screen + hex keypad (A/B/C/D)** on the unit itself |
| Data | per-cycle history in flash (40 cycles), CSV download, RAM curve graph |
| Feedback | buzzer patterns (when fitted), relay power cut (when fitted), `[warn]`/`[stat]` serial diagnostics |
| MCU | ESP32 DevKit V1, Arduino IDE single-file sketch, zero external libraries |

## 3. Power — the design advantage

Power is not a constraint; it is the product:

- **550 W of solar** into a **500 W class heater** means the pillar reaches
  operating temperature quickly and holds it through passing clouds on the
  battery buffer.
- heaterMax defaults to **100 %**: the coil delivers its full 500 W whenever
  the PID demands it (drop it on the Parameters page only if you want a
  gentler chamber). Fuse the heater rail at 50 A.
- At 12.6 V the 0.32 Ω coil draws 41.7 A peak: inside the BTS7960's 43 A
  rating **with the heatsink fitted** — the single most important assembly
  rule of this product.
- One panel, one battery, one box: fully off-grid, zero running cost.

## 4. Temperature — how 100 °C is covered safely

| Zone | Who guarantees it |
|---|---|
| 45–80 °C recipes | PID + AHT10 pair (rated to 85 °C) — the default operating band |
| 80–95 °C recipes | AHT10s mounted in the **cool return path** (below the coil): entering air is 10–20 °C cooler than the chamber core, keeping the sensors inside their rating while the core runs hot |
| 95 °C | hard software cut: heater + fans off, cycle faulted |
| 100 °C | chamber structural rating (insulation, fan plastic, wiring) — never exceeded |
| Runtime guard | `[warn] AHT10 #n HOT` fires at 82 °C — move the sensor before it is damaged |

**Independent backstop:** always fit a bimetal thermostat on the chamber in
addition to the software cut.

## 5. Safety chain (all active in every mode)

1. Over-temperature cut (95 °C) — heater + fans off, fault logged.
2. Both-sensors-dead grace (15 s) — fault, purge, stop.
3. Battery empty (≤ cutoff %) — safe shutdown.
4. Manual heat knob — capped by heaterMax, expires after 60 s, never
   overrides a safety cut.
5. Purge cycle — fans flush hot air before power-off.
6. Optional relay — physical power cut at end-of-cycle and on every fault.
7. Door workflow — locked until the scale is calibrated; locked during the
   cycle; opening mid-cycle = instant FAULT (S3 hardware lock + reed).
8. Hardware error detection — frozen sensors, sensor disagreement,
   battery ADC dead/over-voltage, door reed stuck, restless scale, low
   heap: edge-triggered `[warn]`s + boot self-test.
9. v2.0 faults — heater failure (temperature flat 3 min while heating at
   80 %+ duty) and fan error (RH ≥ 60 % for 5 min despite bursts) stop
   the cycle like any other fault.
10. Supply selection — solar ⇄ bypass relay switched only with loads
    quiet; change under load refused + 5 s error beep; feed verified by
    optocoupler; hard power-off via BUTTON-1 (P-MOSFET latch).

## 6. What's in the repository

| Path | Content |
|---|---|
| `arduino-ide/…/*.ino` | **the product firmware** — single file, opens in Arduino IDE 2, no libraries |
| `src/` + `platformio.ini` | same firmware as a PlatformIO project |
| `docs/manual/01–09` | parts list, build steps, architecture, commissioning, error fixes, limitations, RTOS mode, flashing |
| `tools/` | single-file assembler, online-UI sync, string/printf verifiers, UI test suites |
| `.github/workflows/pages.yml` | GitHub Pages deploy for the online UI |
| `README.md` | wiring, pin map, control logic, website guide |
| `CHANGELOG.md` | release history |
| `docs/PARAMETERS.md` | every parameter: ranges, defaults, effects |
| `docs/manual/10` | every-inch build documentation |
| `future/` | upgrade roadmap + one spec per future feature |
| `production/` | BOM, assembly checklist, QC procedure, serials, warranty |
| `demo/` + `compliance/` | demo script, one-pager, safety & standards |

## 7. Box (pillar) build notes

- Steel/SS or well-sealed plywood pillar, 6 ft (183 cm) standard / 8 ft (244 cm)
  option, 30 × 30 cm interior cross-section — see manual 10 §1.
- **Battery in the base bay** (ventilated, sealed bulkhead from the chamber):
  recommended **12.8 V LiFePO4 50–100 Ah with ≥ 50 A BMS** — longevity
  (2000+ cycles), safety and cost-per-cycle; the MPPT controller charges it,
  the dryer's low-voltage cut is the second guard.
- Coil in the lower plenum; fans top-mounted (exhaust) and side (intake).
- AHT10 #1 at the top of the chamber, AHT10 #2 near the return inlet.
- Insulate the pillar walls (5–10 mm wool/mat) — 500 W holds 80 °C easily.
- Keep all high-current runs short and fused at the battery (50 A class).
