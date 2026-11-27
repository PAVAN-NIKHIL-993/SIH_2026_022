# USER SPECIFICATION EXTRACT — from the uploaded PDFs (2026-09-23)

> Implementation status: **stick/paste batch calculator live (v2.0.16 —
> stickCount × stickWetG × paste water % → computed target + D water total;
> the wet-bulb RH shorthand documented as reference only)** ·
> **E-code fault system E01–E20 live (v2.0.14)** ·
> keypad 5 = ENTER/OK alongside A (v2.0.14) · 37-pattern buzzer engine and
> the on-board RGB status pixel: planned next.

Distilled from `AGARBATTI DE-HUMIDIFIER - COMPLETE DOCUMENTATION.pdf` and
`INDICATION AND BEEPS.pdf` (= `buzzers.pdf`). These are the OWNER's spec
documents; where they conflict with the build, the owner decides.

## Workflow (Steps 0–12)

0. Power source select: toggle + opto must AGREE → relay CH1 (solar) /
   CH2 (bypass), never both; change refused under load = E02 + 5 s beep;
   3 s beep on successful change-over.
1. Power ON (Button-1 3 s): all pins LOW (safe-start), 10 s init
   (tare, load-cell check, AHT10 test, door read, relay/opto read,
   fan+heater verified LOW), splash "ARCHITECTS OF SOLUTIONS /
   SMART DEHUMIDIFIER", then HOME. Power-on beep 2/s × 3 s.
2. Door open (1 s beep) → load trays → TRAY DETECTION: auto by weight,
   else manual YES/NO per tray (1/2/3). E05 if empty/overload.
3. Door close (1 s beep) → capture + freeze W_initial (only weighed
   when closed).
4. Mode + parameters (C cycles AGARBATTI → USER → SILICA GEL):
   setpoint °C, target finished weight, cycle time hh:mm, tray enables.
   Summary shows W_initial − W_target = moisture to remove.
5. START: D = RUN (or Button-2 = default automation). 3 s beep.
   Battery < 20 % at start → 3 short beeps warning.
6. Dehumidified air intake: blower pulls air through the SILICA GEL
   box (physical desiccant stage) — enables low-temperature drying.
7. Closed-loop heat: BTS7960/L298N + current controller hold setpoint;
   E01 coil overheat, E03 heater failure (no temp rise in 3 min), E04
   sensor failure.
8. Multi-tier convection through 3 metal mesh trays — no flipping.
9. RH ≥ 60 % for 1 min → outlet fan ON 1 min → re-evaluate (repeat).
   RH > 60 % for 5 min despite fan = E06. Fan shown ON/OFF with RPM.
10. Telemetry: display + Wi-Fi dashboard (AgarbattiDryer AP, no
    internet). Log = time, temp, RH, weight, fan status, RPM, power
    mode, cycle mode, events, errors + graphs T/RH/W vs time.
11. Termination: W_current ≤ W_target AND RH ≤ RH_target (gravimetric
    + humidity, not purely timer); or time-out / operator stop.
    Heater OFF → blower run-down → exhaust run-down → log closed →
    5 s completion beep + COMPLETION LED → COMPLETE screen:
    INITIAL / FINAL / TARGET WEIGHT, MOISTURE REMOVED, DURATION.
12. Unload (door-open 1 s beep) → next batch or Button-1 3 s = power
    off, 10 s = full restart (re-runs 10 s init).

## Keypad (doc mapping)

2/8/4/6 = UP/DOWN/LEFT/RIGHT · **5 = ENTER/OK** · # = BACK · * = HOME ·
A = EXPAND · B = NAVIGATION · C = MODE SWITCH · D = RUN.
(Our build today: A = OK, B = menu — CONFLICT to resolve.)

## Error codes (cleaned; doc's own table had offset display-texts)

| Code | Error | Detect | Action |
|---|---|---|---|
| E01 | Heater overheat | coil > T_HEATER_MAX / thermal trip | heater OFF, blower cools, stop, manual reset |
| E02 | Source change-over error | toggle ≠ opto / both live / relay mismatch | refuse change, keep safe source, manual reset |
| E03 | Heater failure | ON but no temp rise in response period | heater OFF, blower cools, stop, manual reset |
| E04 | Temp sensor failure | AHT10 repeated fails / implausible | heater OFF, stop, manual reset |
| E05 | Load-cell error | invalid / unstable / under-range | no start; if RUNNING heater OFF, manual reset |
| E06 | Fan/RH evacuation error | RH > 60 % for 5 min despite fan | heat to safe state, fan keeps running, stop if persists |
| E07 | Door open during cycle | limit = OPEN while RUNNING | heater OFF, pause/stop, blower cools |
| E08 | Door sensor failure | stuck / inconsistent input | heater disabled, no start, manual reset |
| E09 | Blower failure | ON but no RPM/current/airflow proof | heater OFF immediately, stop |
| E10 | Heater current fault | ON with I < I_MIN, or OFF with leakage | heater OFF, stop, manual reset |
| E11 | Relay fault | commanded ≠ feedback after settling | disable output, refuse operation, manual reset |
| E12 | Low battery | below warning threshold | warn; critical → shutdown protection; auto-clear |
| E13 | Low supply voltage | below minimum for debounce | heater OFF, protect MCU, restart when stable |
| E14 | High supply voltage | above maximum for debounce | outputs OFF, stop, manual reset |
| E15 | Load-cell comms error | HX711 unresponsive | heater OFF, stop (weight termination untrusted) |
| E16 | Overload | weight > max permissible | no start / heater OFF + stop, remove load |
| E17 | Invalid target weight | target ≥ initial / below minimum | prevent RUN (no heater) |
| E18 | Invalid parameters | any parameter out of range | prevent start, back to parameter screen |
| E19 | Cycle timeout | time ≥ max while W > target | heater OFF, cooldown, close log as TIMEOUT |
| E20 | Temp out of range | outside safe band | heater OFF, cooling if safe, stop, manual reset |

Dashboard fault card: code, name, timestamp, severity, detection value,
limit, current state, action taken, reset type (ACTIVE/CLEARED views).

## Buzzer language — 37 alerts, 13-frequency palette

Palette F1–F13: 440 (danger) · 587 (low warn) · 740 (invalid) · 880
(cancel) · 1046 (caution) · 1175 (key click) · 1318 (tick) · 1480
(positive) · 1760 (info) · 2093 (attention) · 2637 (urgent) · 3136
(success) · 3520 Hz (emergency, ISO alarm freq).

Full assignment (pattern N×M = N beeps of M ms; ↔ warble; → sweep):

| # | Event | Freq | Pattern |
|---|---|---|---|
| 1 | key-press ack | F6 | 1×50 ms |
| 2 | invalid press | F3 | 2×80 ms |
| 3 | long-press tick | F7 | 1×50 ms per 500 ms while holding |
| 4 | mode cycle | F8 | 1×100 ms |
| 5 | parameter saved | F8→F9 | 2×80 ms ascending |
| 6 | BACK/HOME discard | F4 | 1×100 ms |
| 7 | door ajar at START | F10 | 3×150 ms, repeat/2 s |
| 8 | no trays at START | F2 | 2×100 + 1×300 ms, repeat/3 s |
| 9 | weight unstable at close | F5 | 2×100 ms per 3 s until stable |
| 10 | tray-prompt idle nudge | F7 | 1×80 ms per 5 s (30 s max) |
| 11 | summary ready | F8 | 1×500 ms |
| 12 | battery < 20 % at start | F2→F1 | 3×150 ms descending |
| 13 | battery < 10 % in cycle | F1 | 4×100 ms per 30 s |
| 14 | setpoint reached | F8→F9→F10 | 1×100 ms each, ascending |
| 15 | exhaust fan ON | F9 | 1×80 ms |
| 16 | door open mid-cycle | F13 | 100 ms ON/50 ms OFF until closed |
| 17 | 50 % moisture removed | F9 | 2×100 ms, once |
| 18 | target approaching (105 %) | F10 | 2×80 ms per 60 s |
| 19 | 5 min to timeout | F2 | 3×100 ms per 30 s |
| 20 | auto-paused (degraded) | F2↔F5 | 2×200 ms warble per 10 s |
| 21 | brownout / resume | F11→F1 / F8→F9 | stutter / 2×120 ms ascending |
| 22 | weight anomaly | F11 | 3×80 ms, once |
| 23 | warning fault (non-halt) | F2 | 2×150 ms per 15 s |
| 24 | critical fault (halted) | F1↔F13 | 5 s warble, then 2×250 ms per 10 s |
| 25 | error cleared/recovered | F9→F10→F11 | 2×50 ms rising |
| 26 | load-cell E05 at loading | F3 | 1×2000 ms + chirps per 2 s |
| 27 | Wi-Fi AP ready | F9 | 1×80 ms |
| 28 | client connected | F8→F9 | 1×120 ms |
| 29 | log saved | F10→F11 | 2×80 ms |
| 30 | storage ≥ 90 % full | F2 | 3×100 ms at power-on |
| 31 | charging connected | F7→F8 | 1×100 ms |
| 32 | charging complete | F9→F10 | 2×100 ms |
| 33 | maintenance / silica regen (50 cycles) | F5 | 3×120 ms at power-on, any key dismisses |
| 34 | init sub-test failure | F3 | 1×2000 ms on the failing test |
| 35 | cool-down complete (safe to open) | F8→F9 | 1×200 ms |
| 36 | shutdown confirm (3 s hold) | F9→F1 | 1×1000 ms descending |
| 37 | factory reset (10 s hold) | F12→F1 + F4 | 1500 ms sweep + 3×100 ms |

Priority-10 (doc's own shortlist): #1, #2, #7, #16, #12/#13, #14, #18,
#24, #25, #35.

## Hardware the spec assumes (vs our build)

- Completion LED (batch-ready indicator) — NOT in our build.
- Fan RPM display/logging — needs a 3-wire fan tach (we have none).
- Heater current monitoring (E01/E10) — needs shunt/ACS712 (we have none).
- Per-tray load cells / tray detection — we have ONE batch scale.
- Silica-gel intake box (physical desiccant stage) — mechanical; our
  SILICAGEL mode is the firmware counterpart; regen reminder = #33.
- 3.5" TFT — REMOVED in our build by owner decision (website display).
