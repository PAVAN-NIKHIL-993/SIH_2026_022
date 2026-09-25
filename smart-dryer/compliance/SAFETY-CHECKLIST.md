# Safety checklist (every protection, how it is guaranteed)

| # | Hazard | Protection | How to verify (QC #) |
|---|---|---|---|
| 1 | Over-temperature | 95 °C hard cut: heater+fans off, FAULT, logged | QC test 9 |
| 2 | Sensor death | both AHT10 dead 15 s → fault + purge | unplug both mid-run |
| 3 | Battery empty | ≤ cutoff % → safe shutdown | QC test 12 + set cutoff high briefly |
| 4 | Heat soak at end | 45 s purge before power-off | QC test 10 |
| 5 | Short/overload | 50 A fuse at the battery + | visual + DMM |
| 6 | Coil live during service | (with relay fitted) power cut at DONE/FAULT; label warns disconnect battery | label check |
| 7 | Battery bay gas | louvre vents, sealed bulkhead, no electronics in bay | assembly checklist |
| 8 | Manual override abuse | knob capped by heaterMax, 60 s auto-expiry, never overrides cuts | QC test 8 |
| 9 | OTA during drying | refused while RUNNING | QC test 13 |
| 10 | Sensor over-rating | 82 °C hot-guard warns; >85 °C recipes require return-path mounting | manual 10 §4 |
| 11 | Software backstop failure | bimetal thermostat recommended as independent layer | fit at build |
| 12 | Strapping/boot glitches | 5× power-cycle QC test | QC test 14 |
| 13 | Door opened mid-cycle (burn/scald) | door lock + reed: instant FAULT, purge, power off | QC test 16 |
| 14 | Un-calibrated scale mis-weighs the batch | start refused + door locked until a known-weight calibration | QC test 16 |
| 15 | Silent hardware degradation | frozen-sensor / heater-ineffective / battery-ADC / reed watchdogs + boot self-test | serial `[warn]` review |

**Known residual risks (honest):** no galvanic isolation on the heater
circuit (12 V domain — low risk); plywood build is combustible if the
chamber exceeds rating (steel preferred for commercial units); firmware
is single-sourced (OTA + versioned releases mitigate).