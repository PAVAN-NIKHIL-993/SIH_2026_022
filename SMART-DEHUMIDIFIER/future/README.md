# FUTURE — the upgrade roadmap

Future-ready plans for the Smart Dehumidifier Pillar. One spec per upgrade, each
with hardware, firmware touchpoints, UI, effort and risks — so any of them
can be picked up and built without re-discovery. Specs that need new pins
reuse the FREE pin pool — S3: **GPIO 3, 11** (40/42/47 = DS1302 RTC
since v2.0.21; 35/36/37 = PSRAM, never wired); classic: see
`docs/PARAMETERS.md`.

## Phases

**This product is rural-first: no internet, ever, in the dryer itself.**
Every upgrade below works on the hotspot alone. The only optional internet
feature (03 Telegram) runs on the owner's phone, not the dryer.

| Phase | Upgrade | Spec | Effort | Why it matters |
|---|---|---|---|---|
| 1 | **Load cells — dry to weight** | `01-LOAD-CELLS.md` | weekend | ends time-guessing; exact residual moisture |
| 1 | **Recipe presets by incense type** | `02-RECIPE-PRESETS.md` | weekend | "Sandal 45 °C slow" / "Masala 80 °C fast" in one tap |
| 2 | **Solar/MPPT telemetry** | `04-SOLAR-TELEMETRY.md` | weekend | see panel W, battery Ah, "can I dry today?" — no internet needed |
| 2 | **Multi-pillar fleet** | `06-FLEET.md` | 2 weekends | one phone, many dryers (demo day ready) |
| 3 | **PWA installable app** | `05-PWA-APP.md` | days | app-icon feel, offline dashboard |
| 3 | **Learn from every batch** | `07-LEARNING.md` | 2 weekends | the dryer improves its own recipes |
| opt | Telegram alerts *(needs phone data — rural-optional)* | `03-REMOTE-ALERTS.md` | weekend | fault/done pings when the owner IS online |

## Ground rules (apply to every upgrade)

1. **No new libraries** in the firmware path — dependency-free is a product
   feature (single-file sketch keeps building in Arduino IDE 2).
2. **Website stays 100 % offline-capable** — every new feature must work on
   the hotspot alone; internet features (Telegram, live weather) are
   phone/browser-relayed, exactly like the weather card today.
3. **OTA-first** — every upgrade ships as a `.bin` the owner uploads from
   the website; no box-opening, no USB.
4. **Safety chain is untouchable** — 95 °C cut, sensor-death grace, purge,
   battery cut stay above every new feature.
5. **Pins are frozen** — new hardware only on the free-pin pool.