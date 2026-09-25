# 07 — What's Left, and the Reach

## 1. Deliberately NOT built yet (the open to-do list)

Ordered by impact. Each maps to an entry in `ADVANCED-IDEAS.txt`.

| Gap | Why it matters | Effort |
|---|---|---|
| **Load cells — dry to weight** | Ends time-guessing; exact residual moisture | ** weekend |
| **Auto PID tune** | One press instead of manual Kp/Ki/Kd fiddling | 2 weekends |
| **Remote alerts (Telegram via bridge)** | Know a fault from anywhere | weekend |
| **OTA updates from the website** | Never open the box again to reflash | weekend |
| **PWA install + notifications** | App-icon feel on the phone | days |
| **Mid-cycle log flush to flash** | Crash-proof live graph | days |
| **DS3231 RTC** | Perfect timestamps with zero phone contact | hours |
| **Multiple dryers, one dashboard** | ESP-NOW mesh | 2–3 weekends |
| **Solar-following heater** | Drying on pure sunshine | 2 weekends |
| **Recipe learning from history** | The system suggests settings from YOUR best batches | weeks |

## 2. The reach — what this actually is

Scale this ladder honestly:

1. **A better dryer (today):** consistent 45 °C, humidity-aware venting,
   cycle records, solar power, zero fuel. Most village-scale units have
   none of this. Immediate quality + cost win.
2. **A data-verified process (load cells + learning):** every batch has a
   curve, a moisture number, and a quality score. You can PROVE why your
   sticks smell better — and charge for it.
3. **A product (multiply it):** the BOM is ~₹1,500 of electronics on any
   insulated box; the mesh + OTA + recipes turn it into a sellable kit for
   other agarbatti/drying units (herbs, chili, fish, papad — same physics).
4. **A small industry platform:** many chambers, one dashboard, energy
   accounting per kg out, GitHub as the free historian. From here it is a
   brand, not a gadget.

The physics ceiling is real but far: professional dehydrators run 3–10×
faster with heated shelves, dehumidification and airflow control — every
rung of that ladder is one of the advanced ideas (solar air heater, heat
recovery, desiccant, dampers), not a rewrite.

## 3. Where the danger hides (re-read before scaling)

- Heat + dust + fans = fire risk grows with coil size. Bimetal + fuse are
  mandatory at ANY scale; add a smoke alarm above the chamber at scale.
- Grid bypass wiring must be done by a qualified electrician, earthed,
  RCD-protected. The ESP only signals a relay — it must never be your
  only isolation.
- Battery: fuse at the terminal, ventilated box, correct chemistry
  settings on the site (battType) so the % reading means something.
- Data ≠ control: even with all sensors, agarbatti recipes are cultural —
  keep the human tasting step in the loop while the system learns.

## 4. Repo map for future-you

```
smart-dehumidifier/
├── README.md                    start here
├── ADVANCED-IDEAS.txt           the whole ladder, rated
├── platformio.ini               build config
├── arduino-ide/smart-dehumidifier.ino  Arduino IDE entry
├── src/                         firmware (config.h = your knobs)
├── docs/
│   ├── manual/                  THIS manual set (01–07)
│   ├── index.html               generated GitHub-Pages site
│   └── wiring-diagram.svg
└── tools/
    ├── online/sync-online.js    regenerate the Pages site
    └── ui-test/                 headless UI tests (52 assertions total)
```

Everything is optional from here on — the dryer already works. Build the
next rung only when the current one earns you money or time.