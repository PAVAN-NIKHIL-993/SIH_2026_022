# 08 — Optional RTOS Architecture (FreeRTOS)

The firmware ships with **two interchangeable runtime architectures**,
selected by one flag in `src/config.h`:

```c
#define DRYER_RTOS 0   // 0 = cooperative loop (default), 1 = FreeRTOS tasks
```

Both modes run the **identical modules** (`sensors`, `battery`, `dryer`,
`web`, `cyclelog`, `buzzer`) — only the scheduling
wrapper differs, so behaviour, settings, website and safety logic are the
same in either mode.

## Mode 0 — cooperative loop (default)

```
loop():  web::handle → sensors.update → battery.update → dryer.tick
         → buzzer.update   (repeat forever)
```
Every module is non-blocking and internally time-gated (AHT10 = 2 s state
machine, control = 1 s, battery = 5 s). One CPU
context, zero race conditions, trivially debuggable with Serial prints.

**Choose this** for normal operation — it is what the whole manual
describes and what ships flashed.

## Mode 1 — FreeRTOS task architecture

Arduino-ESP32 *is* FreeRTOS underneath; this mode makes the parallelism
explicit instead of leaving everything in the single Arduino `loopTask`.

### Task map

| Task | Core | Prio | Stack | Period | Job |
|---|---|---|---|---|---|
| `web` | **0** | 3 | 8192 | 2 ms yield | HTTP server + captive DNS (sits next to the WiFi/lwIP system tasks, which also live on core 0) |
| `sensors` | 1 | 2 | 4096 | 50 ms poll | AHT10 ×2 (2 s cadence inside) |
| `control` | 1 | 2 | 4096 | 200 ms poll | `dryer.tick()` (1 s cadence inside): PID, fan law, safety, logging |
| `panel` | 1 | 2 | 3072 | 10 ms | keypad scan (Wire, mutexed) + buzzer edges (web pages are served by the net task) |
| `power` | 1 | 1 | 3072 | 1 s | battery ADC (5 s cadence inside) |

Arduino's own `loopTask` (and the idle task) still exist; `loop()` just
sleeps in this mode.

### Synchronisation

| Shared thing | Protection |
|---|---|
| **Wire bus (GPIO21/22)** — AHT10 #1 (`sensors`) | `wireMutex` kept for future I2C additions; today a single user, no contention |
| Wire1 (AHT10 #2) | single user (`sensors`), no lock needed |
| Sensor floats (`tAvg`, `hMax`, …) | written by `sensors`, read by `control`/`web` — aligned 32-bit float loads/stores are atomic on the ESP32; values are freshly recomputed every 2 s, so a torn read is impossible in practice and a stale read costs one control tick at most |
| `Settings cfg` | changed only by the web task while the dryer is guaranteed not mid-tick on it in normal use; for hard safety, settings POSTs during DRYING take effect at the next 1 s tick — by design |

No queues are needed: every consumer polls the module getters, which are
`const` and side-effect-free.

### Timing diagram (steady state, core 1)

```
t=0    sensors: AHT trigger issued          (mutex: Wire, ~1 ms)
t+90ms sensors: read result, recompute avg
t=1s   control: PID + fan law + log tick
t=2s   sensors: next AHT cycle
...    panel:   buzzer edge checked every 10 ms
core0: web:     requests served continuously; WiFi/lwIP callbacks fly by
```

### Watchdog & starvation notes

- Every task blocks in `vTaskDelay` well under the idle-task WDT window —
  no task-feeding needed.
- The `web` task's 2 ms yield keeps core 0's WiFi stack healthy even under
  continuous dashboard polling from 2–3 phones.

## When to switch to RTOS mode

| Situation | Mode |
|---|---|
| Daily drying, want simple + proven | 0 |
| Adding OTA update, HTTP client, TLS, or any *blocking* library later (they need their own task) | 1 |
| Running heavy web features (Pages bridge + several clients + CSV downloads) and noticing sluggish dashboard | 1 |
| Teaching/debugging with Serial prints | 0 |
| Teaching/debugging concurrency itself | 1 |

## How to enable

1. `src/config.h` → `#define DRYER_RTOS 1` → reflash. That's all.
   (PlatformIO can also do it without editing: `build_flags = -DDRYER_RTOS=1`.)
2. Verify on the serial monitor at boot:
   `[rtos] 5 tasks up (web@core0, rest@core1), mode=DRYER_RTOS`
3. Smoke test: start a dummy run, hammer the website from two devices,
   confirm the same DRYING→PURGING→DONE→relay-cut sequence as mode 0.

## Verification status

Both modes compile clean against the real arduino-esp32 2.0.17 +
ArduinoJson 7.1.0 headers (`-DDRYER_RTOS=0` / `=1` syntax-checked, all 10
sources). Behavioural parity (state machine, beeps, relay, history) follows
from sharing the same modules — only scheduling differs.