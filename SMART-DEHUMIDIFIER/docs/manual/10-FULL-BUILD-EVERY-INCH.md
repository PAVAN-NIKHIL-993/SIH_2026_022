# 10 · FULL BUILD DOCUMENTATION — every inch

The complete physical build of the **Pillar dryer**: dimensions, cut list,
every wire, every screw, assembly order. Read with `02-BUILD-STEP-BY-STEP`
(the electrical quick guide) and `docs/datasheet.md` (the product spec).

---

## 1. The pillar — dimensions (6–8 ft product format)

The commercial pillar: **battery in the base, controller at the top,
drying chamber between, solar panel external.** Nothing lives outside the
pillar except the panel.

```
   ┌─────────────────────────┐ ─┐
   │ CONTROLLER BAY   200 mm │  │ ESP32 + BTS7960 + L298N + buck
   │ (sealed, own vents)     │  │ glands on the bottom face
   ├─────────────────────────┤ ─┤
   │ EXHAUST FAN 80 mm       │  │ 30 mm below the chamber lid
   ├─────────────────────────┤  │
   │                         │  │
   │  CHAMBER                │  │ 6 ft pillar: 1100 mm / 6 trays
   │  300 × 300 mm section   │  │ 8 ft pillar: 1700 mm / 9 trays
   │  sticks standing on     │  │ trays 180 mm apart (stainless mesh)
   │  stainless trays        │  │ AHT10 #1 → 50 mm below the lid
   │                         │  │
   ├═════════════════════════┤ ─┤ coil plenum shelf (steel mesh)
   │ ~ ~ ~ 500 W COIL ~ ~ ~  │  │ coil plane 120 mm below the chamber
   │                         │  │ AHT10 #2 → 80 mm above the coil, RETURN
   ├─────────────────────────┤ ─┤ bulkhead: sealed steel + grommets
   │ INTAKE FAN 80 mm        │  │ side entry, just below the bulkhead
   ├─────────────────────────┤  │
   │                         │  │
   │ BATTERY BAY     380 mm  │  │ 12.8 V LiFePO4 50–100 Ah (≥50 A BMS),
   │ (ventilated, sealed     │  │ 50 A fuse + holder, MPPT controller,
   │  from the chamber)      │  │ panel input gland on the side
   └─────────────────────────┘ ─┘
     6 ft (1830 mm) standard · 8 ft (2440 mm) option · feet + castors
```

| Item | Dimension |
|---|---|
| Overall | **6 ft (1830 mm) standard, 8 ft (2440 mm) option**, 30 × 30 cm interior |
| Wall | 0.8–1 mm steel/SS **or** 12 mm sealed plywood |
| Insulation | 10 mm ceramic wool / glass-mat on the chamber walls (rated ≥ 200 °C) |
| Controller bay | 200 mm tall, top, sealed from chamber heat, own vents |
| Battery bay | 380 mm tall, base, ventilated, **sealed bulkhead** from the chamber |
| Trays | 6 (6 ft) / 9 (8 ft) × 290 × 290 mm stainless mesh, rails 180 mm apart |
| Door | chamber-height front panel, 3 hinges + 3 latches, 3 mm silicone rope gasket |
| Intake port | 80 mm hole with 80 mm fan, just below the coil bulkhead |
| Exhaust port | 80 mm hole with 80 mm fan, 30 mm below the chamber lid |
| Coil plenum | steel mesh shelf 120 mm below the chamber floor |
| Feet | 4 × locking castors (a 6 ft pillar must roll for demos) |

**Why 500 W is enough:** 30 × 30 cm section, insulated, closed loop — the
coil holds 80 °C across 1.1–1.7 m of chamber with the fans recirculating
vertically. The 550 W panel + battery carry the 42 A peaks (see §3).

## 2. Cut list (steel/plywood build)

| # | Part | Qty (6 ft) | Size (mm) |
|---|---|---|---|
| 1 | Side wall | 4 | 300 × 1830 |
| 2 | Lid (controller bay top) | 1 | 330 × 330 |
| 3 | Base plate | 1 | 330 × 330 |
| 4 | Door (replaces one side, chamber zone) | 1 | 300 × 1100 |
| 5 | Controller bay floor (sealed bulkhead) | 1 | 300 × 300 + grommets |
| 6 | Battery bay lid (sealed bulkhead) | 1 | 300 × 300 + grommets |
| 7 | Tray rails | 12 | 290 long |
| 8 | Coil shelf (mesh) | 1 | 295 × 295 |
| 9 | Fan plates | 2 | 110 × 110, 80 mm hole |
| 10 | Insulation sheets | 4 + 2 | match chamber walls + lid |

## 3. The coil — 500 W

| | |
|---|---|
| Wire | nichrome, **1.0 mm diameter** |
| Length | **2.3 m total** → two halves of 1.15 m (0.64 Ω each) wired in **parallel** = 0.32 Ω |
| Power | 12.6 V ÷ 0.32 Ω = 39.4 A ≈ **500 W** (BTS7960 43 A — heatsink + fan airflow mandatory) |
| Form | each half zig-zagged across the plenum, 20 mm pitch, 40 mm between halves |
| Mounting | M4 ceramic standoffs every 100 mm; NO contact with metal walls |
| Leads | 2 × 2.5 mm² silicone wire, as short as possible (< 200 mm) to the BTS7960 M+/M− |

**Sizing rule if you change the wire:** target 0.30–0.36 Ω. Thinner wire =
more Ω = less power; never go below 0.29 Ω (43 A bridge limit).

## 4. Sensor placement — the 95 °C trick

AHT10 is rated to 85 °C, the chamber runs to 95 °C. The gap is closed by
**placement**, not electronics:

- **AHT10 #1 (top)** — 50 mm below the lid, dead centre. At the top, the
  air has already given up heat to the exhaust stream; reads chamber temp.
- **Chamber sensor #2 (return)** — 80 mm **above the coil, on the side wall facing
  the intake flow** (the return/cool path). Entering air there is 10–20 °C
  cooler than the chamber core, so the sensor stays inside its rating while
  the PID still sees a true chamber signal.
- Keep both sensors out of direct line-of-sight of the coil (radiant heat).
- Firmware guard: `[warn] AHT10 #n HOT` fires at 82 °C — move the sensor.

## 5. Wire schedule — every connection

**Power (2.5 mm² unless noted, all runs fused at the battery):**

| From | To | Gauge | Note |
|---|---|---|---|
| Battery + | 50 A blade fuse | 2.5 mm² | fuse holder in the box |
| Fuse | BTS7960 B+ | 2.5 mm² | < 300 mm |
| BTS7960 B− | Battery − / GND bar | 2.5 mm² | star point |
| BTS7960 M+ | Coil lead A | 2.5 mm² silicone | < 200 mm |
| BTS7960 M− | Coil lead B | 2.5 mm² silicone | < 200 mm |
| Battery + | L298N +12 V | 1.0 mm² | |
| L298N GND | GND bar | 1.0 mm² | |
| L298N OUT1/OUT2 | Intake fan + / − | 0.5 mm² | 60 mm fan |
| L298N OUT3/OUT4 | Exhaust fan + / − | 0.5 mm² | 80 mm fan |
| Battery + | 5 V buck IN + | 0.5 mm² | |
| 5 V buck IN − | GND bar | 0.5 mm² | |
| 5 V buck OUT | ESP32 VIN + GND | 0.5 mm² | set buck to 5.0 V FIRST |

**Signal (0.25 mm² dupont / 0.5 mm²):**

| ESP32 GPIO | To | Note |
|---|---|---|
| 21 / 22 | AHT10 #1 SDA / SCL | + 3V3 + GND to the module |
| 32 / 33 | AHT10 #2 SDA / SCL | separate bus, 4-wire lead |
| 25 | 555 shifter in → 555 out → BTS RPWM | 1 kHz PWM, 5 V after shifter |
| 26 | BTS R_EN + L_EN (jumpered) | enable |
| 13 | L298N ENA (remove jumper) | intake PWM |
| 15 / 18 | L298N IN1 / IN2 | direction: blow in |
| 14 | L298N ENB (remove jumper; 10 kΩ ENB→GND) | exhaust PWM — the only fan wire |
| — (on module) | IN3 → 5 V header, IN4 → GND | fixed direction tied on the module (v2.0.10) |
| 36 | battery divider mid-point | ADC1 |
| 19 | 2N7000 gate (divider low side) | sampling gate |
| 23 | LOAD relay IN *(optional, RELAYS_ENABLED)* | active-HIGH |
| 2 | BYPASS relay IN *(optional)* | active-HIGH, lights on-board LED |
| 17 | Buzzer S *(optional, BUZZER_ENABLED)* | KY-012 |
| — | BTS LPWM | hard-tied to GND at the module |
| GND | BTS GND + L298N GND + all module GND | **common star point** |

**Battery divider:** BAT+ ──[100 kΩ]──┬── GPIO36 ──[15 kΩ]── GND-when-sampling
via 2N7000 (gate = GPIO19). Calibrate `VBAT_ADC_REF` with a DMM once.

## 6. Bays — controller (top) and battery (base)

**Controller bay (top, 200 mm, sealed from chamber heat):** layout
left→right on the bay floor: 50 A fuse holder → BTS7960 (heatsink against
the bay wall, own vent slots) → L298N → 5 V buck → ESP32 (antenna clear of
metal) → terminal strips. Cable glands on the bay floor for: 2 sensor
leads, 2 fan pairs, coil pair, battery pair (down the inside back wall in
a loom). Never put electronics in the battery bay (hydrogen/heat).

**Battery bay (base, 380 mm, ventilated):** battery on the floor plate,
MPPT controller on the bay wall, 50 A fuse holder on the battery + lead,
panel input gland on the side. Louvre vents top + bottom of the bay ONLY
(outdoors-air path); the bulkhead to the chamber stays sealed.

## 7. Assembly order

1. Cut + fold the walls; fit insulation; fit tray rails.
2. Mount coil standoffs on the mesh shelf; form + mount the coil; measure
   0.30–0.36 Ω with a DMM **before** wiring.
3. Mount fans in their plates; fit plates with gaskets.
4. Fit the door with the rope gasket — it must close airtight.
5. Mount sensors per §4 (zip-tie to a wooden stick, not touching walls).
6. Build the control box per §6; wire per §5; label BOTH ends of every wire.
7. Set the buck to 5.0 V **before** connecting the ESP32.
8. Bench test with no battery: USB-powered ESP32, verify boot log, website,
   fans spin, BTS enable LED, no smoke.
9. First coil test outdoors: knob → 100 % for 30 s (website), coil glows
   dull red at most, BTS heatsink stays touchable-with-a-wet-finger warm.
10. Load trays, commission per `04-DEPLOYMENT-AND-COMMISSIONING`.

## 8. Bill of materials (mechanical)

Wire 2.5 mm² 1 m + 1.0 mm² 2 m + dupont stock · nichrome 1.0 mm × 2.5 m ·
ceramic standoffs ×8 · M4 screws/nuts · 2 hinges + 2 latches · 3 mm silicone
rope 1 m · ceramic wool 0.5 m² · cable glands ×6 · rubber feet · labels.

---

*Every electrical module, tool and cost: `01-PARTS-AND-TOOLS`. Commissioning
and first runs: `04`. Errors and fixes: `05`.*