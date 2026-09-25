# 09 — Flashing: quick paths

**Absolute easiest (Arduino IDE 2, one file, zero libraries):**
open `arduino-ide/SMART-DEHUMIDIFIER-single-file/SMART-DEHUMIDIFIER-single-file.ino` →
Boards Manager → install *esp32 by Espressif Systems* → board *ESP32 Dev
Module* → Upload. Done. (Hold BOOT if it hangs on "Connecting...".)

The rest of this document is the **VS Code + PlatformIO** procedure.

## What you need (once)

- A laptop/PC with internet (only for the first build — toolchain download)
- **USB DATA cable** (most "not connecting" problems are charge-only cables)
- ESP32 DevKit V1
- The `SMART-DEHUMIDIFIER` folder from this repo (the one containing `platformio.ini`)

---

## PART A — One-time setup (~10 min)

**1. Install VS Code** — https://code.visualstudio.com → run, open once.

**2. Install the PlatformIO IDE extension**
- VS Code → left sidebar → Extensions (4 squares icon, `Ctrl+Shift+X`)
- Search **PlatformIO IDE** → Install (publisher: PlatformIO)
- A little **alien/ant icon** appears at the bottom of the left sidebar when done.

**3. Open the project folder — the RIGHT folder**
- File → Open Folder… → select **`SMART-DEHUMIDIFIER`** (the folder that directly
  contains `platformio.ini`).
- ⚠ Do NOT open the repo root (`arena`) — PlatformIO only sees a project
  when `platformio.ini` is in the folder you opened.

**4. Let it download the toolchain (first time only)**
- The moment the folder opens, PlatformIO starts installing the
  `espressif32` platform + xtensa compiler + ArduinoJson library
  (auto-read from `lib_deps`). Watch the little terminal at the bottom
  ("PlatformIO: Initializing" / "Installing").
- Takes 3–10 minutes depending on internet. Needs to finish before
  uploading. You'll know it's healthy when the blue status bar at the
  bottom shows **`env:esp32dev`** with no spinner.

> You do NOT need to install ArduinoJson manually — `lib_deps` in
> `platformio.ini` fetches it. You do NOT need the Arduino IDE at all.

---

## PART B — Flash the ESP32

**5. Plug the ESP32** into a USB port with the data cable.

**6. Pick the port** (usually automatic)
- Bottom blue bar → click the port area (e.g. `COM3` on Windows,
  `/dev/ttyUSB0` on Linux, `/dev/cu.usbserial*` on Mac).
- No port appearing? See the driver note in the trouble table below.

**7. Build first (optional but reassuring)**
- Bottom bar → the **✓ (checkmark)** button → "PlatformIO: Build".
- First build takes 1–3 min. Success ends with `=== [SUCCESS] Took ... ===`
  and a RAM/Flash usage table.

**8. Upload**
- Bottom bar → the **→ (right arrow)** button.
- It builds (if needed) → prints `Connecting...` → `Writing at 0x...` →
- `=== [SUCCESS] === Hard resetting via RTS pin...` → your ESP32 reboots
  into the dryer firmware. Done!

> **Stuck on `Connecting........____`?** Press-and-HOLD the **BOOT**
> button on the ESP32 while it says Connecting, release once
> `Writing at 0x...` starts. (Needed on many clone boards.)

**9. Watch it boot (Serial Monitor)**
- Bottom bar → the **plug (🔌)** icon. Baud is auto (115200).
- Expected output:

```
=========================================
  SMART DEHUMIDIFIER  v1.0
  ESP32 + BTS7960 + L298N + AHT10 + DHT22 + DHT11
=========================================
[cfg] setTemp=45.0C max=60C RH 40-60% time=120min
[sens] S1(top)=OK  S2(bottom)=OK        ← OK if wired; MISSING if bare board (normal on the desk)
[batt] 12.34 V (78%)
[hist] cycle storage ready
[keypad] PCF8574 @0x20 found
[scale] HX711 found (CLK1 DOUT2)   (S3 variant)
[supply] SOLAR MODE (toggle), opto fitted, relay auto
[dht] DHT22 chamber sensor on GPIO10 (cool return)
[dht] DHT11 outdoor sensor on GPIO41 (shade!)
[door] lock+reed fitted, scale NOT calibrated - door LOCKED until calibration
[diag] ---------- self-test ----------
[diag] AHT10#1 OK  DHT22#2 OK  keypad OK  scale OK (NOT calibrated)  DHT11-out OK
[diag] battery OK (12.86V)  door fitted, OPEN  display up  heap 247 kB
[diag] --------------------------------
[ota] network OTA ready - Arduino IDE: Tools > Port > SMART-DEHUMIDIFIER at 192.168.4.1
[clock] no saved time yet - history files use
        sequence numbers until a phone syncs   (or: restored last-saved time ... STALE)
[disp] ILI9488 480x320 up (SCK12 MOSI0 CS2 DC17 RST23)
[web] AP 'AgarbattiDryer' up -> http://192.168.4.1
[main] ready - connect to AP and press Start
```

Bare ESP32 on the desk: sensors MISSING is expected — the website,
clock, and battery tile still work.

**10. Use it**
- Phone → WiFi **AgarbattiDryer**, password **dryer1234**
- Open **http://192.168.4.1** (captive portal usually pops it by itself)
- Dashboard live → Parameters → set thresholds + time → **Save & Start** ▶

---

## Command-line alternative (same thing, no mouse)

```bash
cd SMART-DEHUMIDIFIER
pio run                 # build
pio run -t upload       # flash
pio device monitor      # serial console (Ctrl+C to exit)
```

---

## Trouble table (PlatformIO-specific)

| Symptom | Fix |
|---|---|
| No `COM`/`ttyUSB` port at all | Charge-only cable → use a data cable; or missing driver: install **CP210x** (Silabs) or **CH340** driver depending on the small black chip near your USB socket; replug after install |
| `Permission denied: '/dev/ttyUSB0'` (Linux) | `sudo usermod -aG dialout $USER` then **log out and back in** |
| Stuck at `Connecting...` | Hold **BOOT** during connecting (see step 8) |
| `Error: pip …` or red deprecation notes during install | Warnings only — ignore; only `=== [FAILED] ===` matters |
| Build error on `ArduinoJson.h: No such file` | Let the first auto-install finish, or run `pio pkg install` once with internet |
| Antivirus makes builds crawl (Windows) | Add the `.pio` folder to exclusions |
| `Serial port busy` when uploading | Close any open Serial Monitor / other program using the port, then retry |
| Port disappears after flash | Normal for a moment (USB re-enumerates) — replug if it doesn't return |
| Want a clean rebuild | Trash-can icon (PlatformIO: Clean), then ✓ |

---

## Which copy of the code do you have?

- **Downloaded from this workspace / newest handoff** → full version
  (online mode `/online`, RTOS option, manual, ideas vault).
- **Cloned from GitHub `main` right now** → the merged core (everything
  through "buzzer mandatory"). Fully flashable and usable; the extras
  arrive when the remaining local commits get pushed.

Flashing procedure is identical for both.