# SMART DEHUMIDIFIER PILLAR — one page

**Problem:** agarbatti drying burns fuel/electricity, needs attendance, and
quality depends on guesswork. Rural workshops have sun but no internet.

**Solution:** a 6-ft solar pillar. Load sticks, press Start, walk away.
It holds temperature, vents humidity, protects itself, and remembers
every batch — with zero fuel and zero internet.

| | |
|---|---|
| Power | 550 W panel → 500 W heater, 12.8 V LiFePO4 buffer |
| Capacity | 6 trays (6 ft) / 9 trays (8 ft), ~1.1 m drying column |
| Control | 45–95 °C PID hold, humidity-driven fans, auto power-off |
| Safety | 95 °C cut, sensor-failure cut, purge, battery cut — logged |
| Interface | phone hotspot website: live data, graphs, every parameter, OTA updates |
| Records | per-cycle CSV history, 40 cycles on board, unlimited on backup |
| Running cost | sunlight |

**Why now:** LiFePO4 + 550 W panels hit commodity prices; ESP32 makes the
brain cost less than a tray. This is the moment the economics close.

**Roadmap:** weight-based drying (dry-to-weight, not timers) → recipes by
incense type → solar telemetry ("is today a drying day?") → fleet.

*Concept render: `media/pillar-concept.png` · Demo flow: `demo/DEMO-SCRIPT.md`*