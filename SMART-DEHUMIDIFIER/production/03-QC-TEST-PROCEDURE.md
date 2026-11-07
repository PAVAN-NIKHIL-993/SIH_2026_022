# QC test procedure (every unit, ~20 min)

Run after the assembly checklist is signed. Required: USB cable, phone,
DMM, (optionally) bench 12 V supply 45 A.

| # | Test | How | PASS |
|---|---|---|---|
| 1 | Boot | USB power, open serial 115200 | full banner → `[main] ready` in < 15 s, no panic/reboot loop |
| 2 | Version | banner + website header | matches the release being shipped |
| 3 | Sensors | read serial `[diag]` | each wired sensor (AHT10 / DHT22 / DHT11) = OK; missing ones named exactly |
| 4 | Hotspot | phone WiFi join | `AgarbattiDryer` / `dryer1234`, captive portal pops, 192.168.4.1 loads |
| 5 | Website tabs | open Dashboard + Parameters | gauges fill, forecast strip shows 5 days, Parameters form prefilled |
| 6 | Defaults | Parameters → Reset all to defaults | toast OK; setTemp 80 °C, cap 100 % shown |
| 7 | Fans | start a 2-min cycle with RH above humHigh (breathe into sensor #2) | fans ramp, intake in / exhaust out, kick-start works |
| 8 | Heater | manual knob → 100 % for 60 s | [man] log line; coil heats; badge MANUAL countdown → AUTO |
| 9 | Over-temp cut | Parameters → set safety cutoff to (current temp + 3) °C | FAULT within 1 s, heater+fans off, fault text on site |
| 10 | Purge + DONE | set time to 1 min, let it end | purge 45 s → DONE, log `[dryer] DONE` |
| 11 | History | Past cycles after DONE | new entry, CSV downloads and opens |
| 12 | Battery | compare site V vs DMM at the battery | within 0.3 V (else calibrate VBAT_ADC_REF) |
| 13 | OTA | /update with the shipped .bin | progress → reboot → same version on banner |
| 14 | Strapping sanity | power-cycle 5× | same boot log every time, no chirp/glitch anomalies |

| 15 | Weigh scale (S3) | Tare empty → load 1000 g → Calibrate 1000 | grams read 950–1050 and stable; Tare returns to ~0 |

| 16 | Door workflow (S3) | power-cycle with the scale calibrated → open door mid-run | start refused at boot until calibration; READY with batch grams after close; mid-cycle open → FAULT |
| 17 | v2.0 power hardware | hold BUTTON-1 3 s / 10 s; flip SOLAR toggle mid-run; unplug the feed | 3 s = pillar dies; 10 s = reboot banner; toggle mid-run = REFUSED + 5 s beep; dead feed = red mode + `[warn]` |
| 18 | v2.0 fan bursts + faults | force RH ≥ 60 % (wet cloth) for 6+ min; disconnect the fan | burst after 1 min; FAULT `Fan error` after 5 min |
| 17 | v2.0 power hardware | hold BUTTON-1 3 s / 10 s; flip SOLAR toggle mid-run; unplug the feed | 3 s = pillar dies; 10 s = reboot banner; toggle mid-run = REFUSED + 5 s beep; dead feed = red mode name + `[warn]` |
| 18 | v2.0 fan bursts + faults | force RH ≥ 60 % (wet cloth) for 6+ min; disconnect the fan | burst after 1 min; FAULT `Fan error` after 5 min |

Fail any line → do not ship; log the failure against the serial number.

**QC passed ________  Serial ________  FW version ________  Date ________**
