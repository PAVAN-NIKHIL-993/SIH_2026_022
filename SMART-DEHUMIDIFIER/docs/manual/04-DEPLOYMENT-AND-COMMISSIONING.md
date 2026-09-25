# 04 — Deployment and Commissioning

## 1. Bench test (mandatory before the coil meets battery power)

1. ESP32 + modules on the desk, coil replaced by a 12 V bulb.
2. Full dummy run: 5 min, setTemp 40. Verify on the website:
   state badge DRYING → PURGING → DONE; relay clicks OFF at DONE; buzzer
   3 beeps; the run appears in Past cycles and downloads.
3. Fault drills (one at a time): unplug both AHT10s → FAULT in ≤ 20 s,
   relay OFF; squeeze the battery divider to read < 10 % → FAULT. Press
   **Power On** to re-arm after each drill.

## 2. Real first run (with coil)

1. Coil connected, fuse fitted, chamber loaded with a NORMAL batch.
2. setTemp 45 °C, time 60 min (short run), fanMin 20.
   Coil check: dashboard → Heater card → drag the **manual knob** to 100 % —
   the coil should glow/hum at full power for 60 s, then the badge flips
   back to AUTO and the PID resumes control on its own.
3. Watch the first heat-up: duty should settle under 100 % once near 45 °C.
   If it stays pinned at 100 % and temp plateaus low → coil too small or
   chamber leaks (see 05 §C4).
4. Tune: overshoot > 3 °C → Kp 10→6, Kd 5→8. Sluggish → Kp 10→14.
5. Confirm exhaust air turns from moist to dry-feeling and RH falls
   through the humLow threshold (fans stop) near the end.

## 3. Solar sizing (worked example — adapt to your numbers)

Demand: 200 W coil at ~45 % average duty for 3 h = **270 Wh**
+ fans 2×5 W×3 h = 30 Wh + ESP+buck ≈ 1.5 W continuous (24 h) = 36 Wh
≈ **335 Wh/day**.

Supply: 150 W panel × 4.5 peak-sun-hours × 0.75 derating ≈ **500 Wh/day** ✔
Battery: 12 V 20 Ah (240 Wh usable at 80 % DoD) = buffer for ~1 cloudy
morning + overnight ESP ✔.

Rules of thumb:
- Panel W ≈ 1.5 × coil W for same-day solar drying; ≈ 1 × coil W if the
  battery carries mornings.
- Battery Wh ≈ coil W × hours you want without sun.
- Coil 12 V rails only with BTS7960 from a 3S/SLA pack; for 24 V coils use
  a 24 V rail and a buck for fans/ESP (BTS B+ = 24 V, logic still 5 V).

## 4. Physical deployment checklist

- [ ] Electronics box outside the hot zone, shaded, ventilated
- [ ] Sensors: #1 under the lid mid-chamber, #2 low, away from coil radiant path
- [ ] Fans: intake low / exhaust high, mesh against insects, rain caps
- [ ] All wire entries glanded; no cables touching the coil former
- [ ] Battery + BMS in their own ventilated, shaded box; SLA/LiFePO4 preferred outdoors
- [ ] Star ground; panel frame + chamber earthed if you use grid bypass
- [ ] Fuse accessible; spare fuses taped inside the lid
- [ ] Bimetal thermostat fitted in series with the coil (independent of ESP)
- [ ] First week: download every cycle CSV and check curves before trusting it

## 5. Website / online-mode deployment

**Embedded (default, zero setup):** hotspot `AgarbattiDryer` → 192.168.4.1.

**Optional GitHub-hosted UI:**
1. Push the repo to GitHub (the generated site is `SMART-DEHUMIDIFIER/docs/index.html`).
2. Repo → Settings → Pages → Source: **GitHub Actions** (workflow included).
3. Check `ONLINE_UI_URL` in `src/config.h` (and `variants/esp32-s3/config-s3.h`)
   equals your Pages URL — default `https://pavan-nikhil-993.github.io/SIH_2026_022/`.
4. Phone (on hotspot + mobile data) opens **http://192.168.4.1/online** —
   bookmark it. No internet → auto-fallback to the embedded page.
5. Cycle backup: in the online UI fill repo/branch/folder + a **fine-grained
   PAT** with Contents: Read and write on that one repo. Token stays in the
   phone's localStorage; the ESP never sees the internet.

**Editing the website:** change `src/webui.h` → run
`node tools/online/sync-online.js` → commit both files. ESP gets changes on
next flash; Pages gets them on next push.

## 6. Operating rhythm (daily use)

1. Load sticks, close door.
2. Site → Parameters: confirm thresholds + time → **Save & Start** (or
   **↳ Reset all to defaults** first, then adjust).
   (Fully manual by default. `AUTO_START true` in config.h makes it
   plug-and-play: one cycle per power-up.)
3. Two short beeps. Walk away — phone shows live data while in range.
4. Done: 3 beeps, relay cuts power, site shows DONE; download/backup.
5. Unload; press **Power On** for the next batch.

## 7. Maintenance

| Every | Do |
|---|---|
| batch | glance at fault field, wipe sensor dust |
| week | clear fan grilles, check door seal |
| month | battery V vs multimeter (recalibrate offset), tighten power screws |
| season | download + archive all cycle CSVs, check fuse holder heat marks |
