# Components Checklist — Smart Dehumidifier Pillar v2.0

**Print this, tick the boxes, buy in the phase order below.**
Full specs + notes: `01-BOM-FULL.csv` · Wiring: manual 02 (§4 = every module) ·
Assembly sign-off: `02-ASSEMBLY-CHECKLIST.md` · Final tests: `03-QC-TEST-PROCEDURE.md`.

> **Board choice:** the **ESP32-S3 DevKitC** is the product board — every v2.0
> feature gets its own pin. The classic DevKit V1 runs the same firmware with
> shared pins (no supply relay, software door workflow). Buy ONE of the two.
> **S3 boards: N8, N16 and N16R8 (octal PSRAM) all work** — the v2.0.13
> pin map never touches GPIO 35/36/37 (PSRAM on R8). Opto = GPIO 18, DHT11 = GPIO 41 (v2.0.20).

---

## Phase 1 — Bench kit (buy first, test on the desk before any metalwork)

| ✔ | # | Component | Qty | Est. ₹ | Connects to |
|---|---|---|---|---|---|
| ☐ | 1 | ESP32-S3 DevKitC (N8/N16 quad-flash) *or* ESP32 DevKit V1 | 1 | 450–700 | the brain |
| ☐ | 2 | AHT10 sensor module | 1 | 160–300 | I2C0 (S3: 8/9) — chamber TOP |
| ☐ | 2b | DHT22 sensor + 10 kΩ (chamber cool-return) | 1 | 150–250 | GPIO 10 + 10 k pull-up DATA→3V3 |
| — | 3 | ~~display~~ **REMOVED** — the website is the screen (`/` + `/display` kiosk); any spare phone/tablet mounts on the pillar | — | ₹0 | no component to buy |
| ☐ | 4 | 4×4 hex keypad + PCF8574 backpack (**not** PCF8574A) | 1 | 150–280 | I2C0 @0x20 |
| ☐ | 4b | PCF8574 module #2 (front panel) + 8 × 10 kΩ pull-ups | 1 | 60–90 | I2C0 @0x21 (strap A0→3V3, A1/A2→GND); relays/buttons/toggle/opto/door/buzzer |
| ☐ | 5 | Buzzer KY-012 (active) | 1 | 10–30 | S3: 38 · classic: 13 |
| ☐ | 6 | HX711 + 2 × 5 kg half-bridge load cells | 1 | 300–400 | S3: CLK 1, DOUT 2 |
| ☐ | 7 | BTS7960 43 A (heatsink fitted) | 1 | 250–350 | S3: RPWM 12 (via 555), EN 13 |
| ☐ | 8 | L298N dual H-bridge + 10 kΩ (ENB→GND) | 1 | 60–100 | S3: ENB 17 only; IN3→5V + IN4→GND tied on the module; jumper cap OFF |
| ☐ | 9 | 555 timer level shifter board | 1 | 80–150 | 3.3 V → 5 V PWM |
| ☐ | 10 | 5 V buck (LM2596/mini-360) — **set to 5.0 V first** | 1 | 60–120 | ESP32 supply |
| ☐ | 11 | ~~Door solenoid lock~~ **not needed** — limit switch only (item 12) | — | ₹0 | optional later: DOOR_LOCK_ENABLED 1 + a free GPIO (3/11) |
| ☐ | 12 | Limit switch (or reed contact) | 1 | 20–50 | S3: 21 (v2.0.20: was 34; → PCF#2 P6) |
| ☐ | 12b | DHT11 outdoor sensor + 10 kΩ | 1 | 100–180 | GPIO 41 + 10 k pull-up DATA→3V3, shade (v2.0.20: was 33) |
| ☐ | 12c | DS1302 RTC module (CR2032 coin cell) | 1 | 30–60 | S3: RST 40 / SCLK 42 / I-O 47, VCC→3V3, BZ unused (v2.0.21); auto-detected |

**Phase 1 subtotal ≈ ₹2,600–4,000.** Bench-check: flash the v2.0 sketch,
`[diag]` self-test all OK, `/display` page shows live values, keypad navigates (serial echo).

## Phase 2 — v2.0 power hardware + safety chain

| ✔ | # | Component | Qty | Est. ₹ | Connects to |
|---|---|---|---|---|---|
| ☐ | 13 | P-MOSFET (SUP53P06/IRF9540) + 100 kΩ resistor | 1 | 30–60 | latch: S3 hold 14 · classic 16 |
| ☐ | 14 | Momentary panel buttons (BUTTON-1, BUTTON-2) | 2 | 20–40 | S3: 15, 16 · classic: 15, 18 |
| ☐ | 15 | SPDT toggle (SOLAR ⇄ BYPASS) | 1 | 15–30 | S3: 39 · classic: 27 |
| ☐ | 16 | PC817 optocoupler module | 1 | 15–25 | classic GPIO 35 · S3 GPIO 18 |
| ☐ | 17 | 2-channel relay module (CH1 solar, CH2 bypass) | 1 | 60–100 | S3: 6, 7 (classic: manual) |
| ☐ | 18 | 50 A blade fuse + holder | 1 | 150–300 | battery + → heater rail |
| ☐ | 19 | 80 mm 12 V fan (**outlet only** — no intake in v2.0) | 1 | 100–200 | L298N OUT3/4 |

**Phase 2 subtotal ≈ ₹400–850.**

## Phase 3 — Solar power chain

| ✔ | # | Component | Qty | Est. ₹ | Notes |
|---|---|---|---|---|---|
| ☐ | 20 | 550 W solar panel (24 V nom, ~41 Vmp) | 1 | 15,000–20,000 | check Voc vs MPPT input range |
| ☐ | 21 | MPPT charge controller (20 A+, 12 V side) | 1 | 3,000–8,000 | PWM saves money, loses ~20 % |
| ☐ | 22 | 12.8 V LiFePO4 4S 50–100 Ah, ≥50 A BMS | 1 | 15,000–28,000 | budget path: 12 V SLA 100 Ah+ |
| ☐ | 23 | Wire 2.5 mm² silicone (red/black) | 1 m | 100–200 | coil + battery runs < 300 mm |

## Phase 4 — Heating + chamber structure

| ✔ | # | Component | Qty | Est. ₹ | Notes |
|---|---|---|---|---|---|
| ☐ | 24 | Nichrome wire 1.0 mm — 2.3 m (2 × 0.64 Ω parallel = 0.32 Ω) | 1 | 150–250 | 500 W @ 12.6 V |
| ☐ | 25 | Ceramic standoffs M4 | 8 | 100–200 | coil never touches metal |
| ☐ | 26 | Pillar body 300×300 × 1830 mm (steel/SS or 12 mm plywood) | 1 | 3,000–8,000 | 8 ft option: 2440 mm |
| ☐ | 27 | Insulation 10 mm, ≥200 °C (ceramic wool/glass mat) | 0.5 m² | 400–800 | chamber walls + lid |
| ☐ | 28 | Stainless mesh trays 290×290 mm + rails | 6 | 1,200–2,400 | 8 ft option: 9 |
| ☐ | 29 | Hinges + latches (stainless) | 3 + 3 | 300–600 | |
| ☐ | 30 | Silicone rope gasket 3 mm (door seal) | 1 m | 150–300 | |
| ☐ | 31 | Locking castors 50 mm | 4 | 400–800 | the demo pillar must roll |
| ☐ | 32 | Cable glands | 6 | 300–600 | |

## Phase 5 — Consumables

| ✔ | # | Component | Qty | Est. ₹ |
|---|---|---|---|---|
| ☐ | 33 | Wire 1.0 / 0.5 mm² (fans, buck, signals) | 3 m | 100–200 |
| ☐ | 34 | Terminal strips + label set | 1 set | 200–400 |
| ☐ | 35 | Heat-shrink + zip ties | 1 set | 150–300 |

---

## Totals

| Scope | Range |
|---|---|
| Electronics (Phases 1+2) | **≈ ₹3,000–4,800** |
| + solar chain (Phase 3) | ≈ ₹18,000–56,000 (panel + MPPT + battery dominated) |
| + structure (Phases 4+5) | ≈ ₹6,000–13,000 |
| **Complete pillar** | **≈ ₹27,000–74,000** (LiFePO4 + 550 W panel at the top; SLA budget path ~₹8k lower) |

## Tools you must have

☐ Multimeter · ☐ Clamp meter (DC, for the 40 A heater check) · ☐ Soldering
iron + solder · ☐ Crimper / wire strippers · ☐ Small spanner set ·
☐ USB data cable for the board (USB-C for the S3) · ☐ Phone with WiFi (the
"remote" — the dryer's website runs on its own hotspot).

## On-delivery checks (common traps)

- **PCF8574 vs PCF8574A**: the A version answers at 0x38 = AHT10 clash. Check the silkscreen.
- **AHT10 fakes**: some clones freeze at one value — the firmware warns
  (`[warn] AHT10 #n FROZEN`); a spare is cheap insurance, ~₹80–150.
- **S3 board**: N8, N16, N16R8 all fine (v2.0.13) — GPIO 35/36/37 are
  never used (PSRAM on octal-R8 boards). Run `arduino-ide/board-test-s3`
  first on a new board to confirm flash + PSRAM from the serial monitor.
- **Buck converter**: set 5.0 V with a multimeter BEFORE the ESP32 is connected.
- **Display**: none in this build — the website is the screen (`/` full
  control + `/display` kiosk page). Any spare phone/tablet works as the
  mounted screen. (A future SPI ILI9488 can return via DISPLAY_ENABLED 1.)
- **Load cells**: the two halves must be the same rating (2 × 5 kg); mount
  under the tray rails, OUTSIDE the hot chamber (HX711 board in the control bay).
- **Panel Voc**: must sit inside the MPPT controller's input range (usually
  ≤ 100 V — fine for a single 24 V panel).
- **Solenoid**: 12 V drawing < 1.5 A; driver board with a flyback diode (or add
  1N4007 across the coil).

*Serial scheme PIL-YYYY-NNN · model SDP-600 · firmware v2.0.0*
