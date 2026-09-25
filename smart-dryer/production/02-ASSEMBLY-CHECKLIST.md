# Assembly checklist — sign-off sheet

Print and sign at every build. One line = one verifiable step. Do not
continue past a failed line. Full detail: `docs/manual/10`.

## Mechanical
- [ ] Pillar body cut per manual 10 §2 (6 ft: 300×300×1830 mm interior)
- [ ] Chamber walls insulated (10 mm, ≥200 °C rated), no exposed wool in airflow
- [ ] Bulkheads fitted: controller bay floor + battery bay lid (sealed, grommets)
- [ ] Tray rails level — tray slides full depth without binding (6 trays)
- [ ] Door closes on silicone rope gasket — no light gap at any edge
- [ ] Louvre vents: battery bay ONLY (top + bottom); chamber stays sealed
- [ ] 4 locking castors fitted, pillar stands square, rolls + locks

## Coil (500 W)
- [ ] Coil formed: 2 × 1.15 m of 1.0 mm nichrome, zig-zag on standoffs
- [ ] **DMM measured 0.30–0.36 Ω across the pair BEFORE wiring**
- [ ] No coil-to-metal contact (ceramic standoffs only)
- [ ] Leads ≤ 200 mm, 2.5 mm² silicone, to BTS M+/M−

## Wiring
- [ ] 50 A fuse at battery + (before anything else on that lead)
- [ ] Buck set to **5.0 V before** ESP32 connected (DMM verified)
- [ ] Every signal wire matches the pin table (manual 10 §5) — checked twice
- [ ] Both ends of EVERY wire labelled
- [ ] Common ground star point: BTS GND = L298N GND = battery −
- [ ] Battery bay: electronics absent, vents clear, bulkhead sealed

## Bench power-on (no battery, USB only)
- [ ] Boot log complete: banner v…, [cfg], [sens], [batt], [hist], [web]
- [ ] Hotspot `AgarbattiDryer` visible; 192.168.4.1 loads the dashboard
- [ ] Both AHT10s read (or `[warn]` names exactly the missing one)
- [ ] Fans spin on the 100 % purge test; directions: intake IN, exhaust OUT

## With battery + panel
- [ ] Battery % plausible vs DMM voltage (calibrate VBAT_ADC_REF)
- [ ] Knob → 100 %: coil glows dull red max, BTS heatsink warm not untouchable
- [ ] Stop → purge runs → DONE; heater + fans off
- [ ] OTA: /update accepts a test .bin and reboots

**Assembled by ________  Checked by ________  Date ______  Serial ________**