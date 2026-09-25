# 02 — Build Step by Step (from scratch)

Work in this order — every stage is testable before the next, so you never
build on a broken stage.

## 1. Prepare the chamber

1. Box: plywood/steel, insulated (25 mm wool/foam). Size example: 60 × 40 × 40 cm
   holds ~500–800 sticks on 2 racks.
2. Two vent holes: **intake LOW** (bottom, one side), **exhaust HIGH** (top,
   other side) — air travels diagonally across the racks. 50–63 mm PVC stubs,
   rain caps outside, mosquito mesh.
3. Coil mounted on mica/ceramic at the bottom, aimed at the intake airflow.
   **Bimetal 70 °C thermostat in series with the coil** (hardware backstop).
4. Racks: wire mesh, ≥ 10 cm above the coil, ≥ 8 cm below the lid.
5. Electronics box mounted OUTSIDE the chamber (it stays < 50 °C). Only
   sensors and fans live inside.

## 2. Power wiring (NO electronics yet)

```
panel ──> charge controller ──> battery (+ BMS) ──> fuse ──> LOAD RELAY COM
LOAD RELAY NO ──> BTS7960 B+  and  L298N 12V
BMS out (before relay) ──> 5 V buck ──> ESP32 VIN + GND   (ESP always powered)
```

- Wire gauge: coil circuit ≥ 2.5 mm² (or per current); fuse = 1.25 × coil current.
- Common **star ground**: battery −, BTS GND, L298N GND, buck −, ESP GND all
  meet at ONE point.
- **Test with multimeter before continuing:** buck outputs 5.0–5.2 V; relay
  off = no voltage at the relay NO terminal.

## 3. Flash the ESP32 (before it touches the machine)

1. Arduino IDE → Boards Manager → install **esp32 by Espressif**.
2. Library Manager → **ArduinoJson by Benoit Blanchon (v7)**.
3. Open `arduino-ide/smart-dehumidifier-single-file/smart-dehumidifier-single-file.ino` → board **ESP32 Dev
   Module** → select Port → **Upload**. (PlatformIO: `pio run -t upload`.)
4. Serial Monitor @115200. You should see the banner, `[sens]`, `[batt]`,
   `[hist]`, `[web] AP 'AgarbattiDryer' up -> http://192.168.4.1`.

## 4. Module wiring, one module at a time — test after each

After each sub-step, re-flash is NOT needed: press EN (reset) and watch the
boot log line for that module.

### 4.1 Chamber sensors: AHT10 (top) + DHT22 (cool return) — v2.0.9

The AHT10 address is **fixed 0x38** — max ONE per bus. So the chamber
runs **one AHT10 at the top** (the precision sensor on the hottest
point) and **one DHT22 on the cool-return path** as source #2. Averages,
RH-peak tracking and the safety cut work exactly as before.

| Sensor | Position | Wiring |
|---|---|---|
| AHT10 | TOP of chamber, under lid | SDA → GPIO8 · SCL → GPIO9 (I2C0, shared with the PCFs) |
| DHT22 | COOL RETURN path (air coming back to the coil) | VCC → 3V3 · GND → GND · DATA → **GPIO10** + 10 kΩ pull-up DATA→3V3 |

- The DHT22's 80 °C ceiling is fine there — the return path is the
  coolest point in the chamber. Keep both sensors out of the coil's
  direct radiated path (they measure AIR).
- I2C1 is now **free** (GPIO 11 spare) — room for a future I2C part.
- Classic variant keeps its 2× AHT10 (I2C1 32/33): `SENS2_DHT 0`.
- Boot log: `[diag] AHT10#1 OK  DHT22#2 OK` (a 3-strike absence detect
  drops a dead sensor from the averages automatically).

### 4.2 Buzzer (optional) — GPIO17, NOT FITTED by default

Skip this step for the current build (`BUZZER_ENABLED 0` in config.h).
To fit later: `+` → 3V3, `−` → GND, `S` → GPIO17, then set the flag to 1.

### 4.3 L298N outlet fan — ONE wire, fixed direction (v2.0.10)

| L298N pin | Connects to |
|---|---|
| 12 V | 12 V bus (fused) |
| GND | star ground |
| ENB jumper | **REMOVE the cap** → wire to GPIO17 (S3) / GPIO14 (classic) |
| ENB | GPIO17 + **10 kΩ ENB→GND** (fan stays off during boot) |
| IN3 | the module's own **5 V header pin** (tied — fixed direction) |
| IN4 | the module's **GND** (tied) |
| ENA + its jumper | unused — leave the jumper ON |
| OUT3/OUT4 | outlet fan (+/−) |
| 5 V regulator jumper | leave ON (powers the logic side) |

- **ENB LOW is a REAL off** — the enable gates the whole bridge, so with
  ENB low both output switches are open: zero current, the fan coasts to
  a stop. PWM duty 0 % = that same off, exactly like a digital pin.
- Enable-PWM has weak low-speed torque: below ~40 % duty a 12 V fan just
  hums — the firmware clamps a floor (`FAN_PWM_FLOOR 40`) and kick-starts
  the rotor from standstill.
- Test: website → start a 1-min dummy run → the fan bursts at 100 %.
  Dead fan → swap its OUT3/OUT4 pair (polarity).

### 4.4 BTS7960 heater driver

| BTS7960 pin | Connects to |
|---|---|
| B+ / B− | relay rail + / − (fused) |
| M+ / M− | heating coil |
| VCC | 5 V (buck), GND → star ground |
| RPWM | GPIO25 **via the 555 level shifter** (GPIO25 → 555 pin 4 RESET; 555 pin 3 OUT → RPWM; 555 pin 8 → 5 V; pins 1/2/6 → GND) |
| LPWM | tie directly to GND at the module |
| R_EN + L_EN (jumpered together) | GPIO26 |

Test with a 12 V **bulb** instead of the coil first: start a run → bulb
glows at varying brightness = PID working. Then wire the real coil.

### 4.5 Battery sense divider (transistor pull-down)

```
battery(+) ──[100k]──┬──> GPIO36
                     |
                   [15k]
                     |
  drain ─────────────┘
  2N7000: gate ──[1k]── GPIO19, source ── GND
```

- Multimeter across battery vs website "V" reading → trim
  `VBAT_CAL_OFFSET` in `src/config.h` until they match (±0.1 V).
- No transistor yet? Connect divider bottom straight to GND and set
  `PIN_VBAT_ENABLE` to `-1` (costs ~0.15 mA constant drain).

### 4.6 Load relay — GPIO23, and Bypass relay — GPIO2

- Load relay IN → GPIO23. `RELAY_ACTIVE_LOW` in config.h must match your
  module (most green = active-HIGH, blue = active-LOW). Test: relay must be
  OFF at boot, ON in IDLE, OFF at DONE.
- Bypass relay contact wired across your bypass source per your electrician's
  plan. GPIO2 also drives the on-board LED — it doubles as the bypass lamp.

### 4.7 Display — none in this build: the website is the screen

- No display hardware. `/` = the full-control dashboard, **`/display`**
  = a read-only kiosk page (huge type, 1 Hz refresh, wake-lock) — mount
  a phone or tablet on the pillar as the screen. S3 pins 3 and 11 stay
  free; 40/42/47 carry the DS1302 RTC (v2.0.21 — see 4.14); 41 = DHT11,
  48 = on-board pixel.
- If a physical screen is ever wanted again: `DISPLAY_ENABLED 1` (SPI
  ILI9488) — re-pick SCK/MOSI/CS/DC/RST first: 40/42/47 now carry the
  DS1302 RTC (v2.0.21) and 41 the DHT11, so only 3/11 (plus the PCF #2
  plan) are free. Re-add the wiring then.

### 4.8 Optional: hex keypad (PCF8574 @ 0x20 on GPIO21/22)

- **PCF8574 (0x20–0x26) only — never PCF8574A at 0x38** (AHT10 collision).
- Boot log `[keypad] PCF8574 @0x20 found`; keys: A start · B stop ·
  C power-on · D +15 min.

### 4.9 Optional: weigh scale (HX711 + 2 × 5 kg half-bridge load cells)

- **ESP32-S3 variant (recommended)**: HX711 CLK → GPIO1, DOUT → GPIO2,
  VCC → 3V3, GND → GND. Enabled by default on the S3.
- **Classic ESP32**: no free output pin while the display is fitted —
  either set `DISPLAY_ENABLED 0` (then CLK → GPIO12, DOUT → GPIO34,
  `SCALE_ENABLED 1`) or move to the S3.
- Mount the cells under the bottom-tray rails **outside the hot chamber**;
  the HX711 board lives in the control bay (rated 85 °C, hates heat).
- Commission: Tare empty → load a known 1000 g → Calibrate 1000 →
  the dashboard shows grams. Then Parameters → **Dry to weight: ON**.

### 4.10 Door limit switch (S3 — the calibrate→load→ready workflow)

- **This build has NO door lock** — just a limit switch for open/closed
  status: one leg → **GPIO21**, other → **GND** (closed = LOW; the
  internal pull-up is used, no resistor needed).
- Workflow enforced in SOFTWARE by the MCU: **START is refused until the
  scale is calibrated** (tare + known weight); closing the door with the
  trays loaded weighs the batch (diff vs tare) → READY; **opening the
  door mid-cycle = instant FAULT** (heat stops, purge, power off).
- Optional later: a 12 V solenoid lock — set `DOOR_LOCK_ENABLED 1`,
  wire the driver to GPIO41 (free), flyback diode across the coil.
- Classic ESP32: same software workflow (`DOOR_ENABLED 0` = no switch
  input either; start still gated on calibration).

### 4.11 v2.0 power hardware: latch, buttons, toggle, opto, supply relay

**Hard power-off latch (P-MOSFET soft-latch):**
- P-channel MOSFET (IRF9540/SUP53P06) in the battery→buck 12 V feed:
  source → battery +, drain → buck input, gate → battery + via 100 kΩ.
- **BUTTON-1** (momentary, front panel) from gate to GND: press → the
  MOSFET conducts → the MCU boots → firmware asserts the **latch-hold
  pin** (S3 GPIO14, classic GPIO16) which keeps the gate low.
- Hold **3 s** → firmware stops the loads and drops the hold → the whole
  pillar loses power. Hold **10 s** → reboot. USB flashing still works
  (the latch only gates the 12 V rail).

**BUTTON-2** (momentary → GND): press = AGARBATTI preset + Start
(door-gated). **SOLAR toggle** (SPDT): one way = solar mode requested,
other = bypass. **Optocoupler** (PC817): LED side across the selected
supply feed, transistor side → GPIO (S3 **18**, classic 35) — HIGH = live.

**2-channel relay (S3 only — GPIO 6/7):** CH1 enables the solar/battery
feed to the chamber, CH2 the bypass feed. Never both. The MCU switches
only with the loads quiet; a change under load = refused + 5 s error
beep. Classic: no free relay pins — the toggle changes the mode name
and the opto still verifies, switching is manual.

**Buzzer (mandatory v2.0):** classic GPIO13, S3 GPIO38 (KY-012).

### 4.12 Display bridge — removed (no display in this build)

The UNO display bridge is gone together with the TFT (v2.0.8): the
website is the screen (`/` full control + `/display` kiosk). To revive
a parallel UNO-shield display someday, set `TXDISP_ENABLED 1` and
re-flash `arduino-ide/display-bridge-uno/` (S3 GPIO3 → UNO pin 4 +
GND; never the UNO's 5 V TX back to the S3).

### 4.13 DHT sensors: DHT22 chamber (return) + DHT11 outdoor — v2.0.9

**DHT22 — chamber cool-return path (source #2):** VCC → 3V3 · GND → GND ·
DATA → **GPIO10** + 10 kΩ pull-up DATA→3V3 (resistor at the sensor end of
long cables). See §4.1.

**DHT11 — outdoor:** VCC → 3V3 · GND → GND · DATA → **GPIO41** + 10 kΩ
pull-up DATA→3V3.

- **Pin note (GPIO 41):** free in this build (no lock, no display) — the
  DHT11 can be wired right away. v2.0.20: moved off GPIO 33, which is
  not broken out on many S3 devkit headers. Works on N8, N16 and N16R8 boards
  (GPIO 35 is PSRAM on R8 — never wired by this build; the optocoupler
  sits on GPIO 18).
- Mount the DHT11 **outside the pillar, in permanent shade (north side
  in India)**, under a small overhang — direct sun reads 10–20 °C high.
- What they do: the DHT22 feeds the chamber averages/RH-peak; the DHT11
  feeds the web OUT line + weather card ("measured by the dryer"),
  **real-time smart venting** (vents only when outdoor air is actually
  drier than the chamber) and `out_t_C,out_rh_RH` columns in every cycle
  CSV. No DHT11 → outdoor data comes from your phone as before.
- DHT11 reads whole degrees / ±5 %RH — coarse but fine outdoors; the
  chamber precision comes from the AHT10 + DHT22.

### 4.14 DS1302 RTC — date & time that survive a full power-down (S3, v2.0.21)

**Optional.** The module has its own CR2032 coin cell, so the calendar
keeps running even when the whole pillar is off.

**Wiring (S3 only):** VCC → **3V3** · GND → **GND** · RST → **GPIO40** ·
SCLK → **GPIO42** · I/O → **GPIO47** · BZ → unused. Nothing else —
the firmware bit-bangs the 3-wire bus (no library).

**Behaviour:**
- Auto-detected at boot (scratch-RAM magic probe). No chip fitted? The
  dryer runs exactly as before: phone sync + NVS clock — no config, no
  error, no pin must be free.
- The web page and serial log show the RTC status (present / time valid /
  last sync); the web "SET TIME FROM PHONE" and the keypad clock menu
  mirror the real time **into the chip** too, so the next boot reads the
  calendar from the coin cell.
- Serial: `rtc` = status, `rtcset` = copy the running clock into the
  chip (for bench use).
- Classic ESP32: `RTC_ENABLED 0`, the pins are −1 — this is an S3 part.

## 5. First full boot checklist

1. Serial log shows every module OK (or "not found (optional)").
2. Phone → WiFi **AgarbattiDryer** / `dryer1234` → 192.168.4.1 loads.
3. All tiles show live numbers; clock syncs automatically.
4. Start a 5-min dummy run with the bulb: state DRYING → PURGING → DONE →
   relay clicks OFF → buzzer 3 beeps → cycle appears in **Past cycles**.

Now go to doc 04 for real commissioning (coil, load, solar).