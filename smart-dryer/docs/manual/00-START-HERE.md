# 00 · START HERE — the documentation map

One page to find everything in the repo.

## I just want to flash it
→ classic ESP32: `arduino-ide/smart-dryer-single-file/smart-dryer-single-file.ino`
→ **ESP32-S3 (recommended)**: `arduino-ide/smart-dryer-s3-single-file/smart-dryer-s3-single-file.ino`
   (board "ESP32S3 Dev Module", USB CDC On Boot: Enabled — see `variants/esp32-s3/README.md`)
Zero libraries either way. (After this, updates go over the air:
website → **⬆ Firmware update**; serial console: type `help` at 115200.)

## I'm building the pillar
| Read | For |
|---|---|
| `docs/datasheet.md` | what the product is (power, temperature, safety) |
| **`docs/manual/10-FULL-BUILD-EVERY-INCH.md`** | dimensions, cut list, coil, every wire |
| `docs/manual/01-PARTS-AND-TOOLS.md` | shopping list with costs |
| `docs/manual/02-BUILD-STEP-BY-STEP.md` | electrical assembly quick guide |
| `docs/wiring-diagram.svg` | the one-glance diagram |

## I'm commissioning / operating it
| Read | For |
|---|---|
| `docs/manual/04-DEPLOYMENT-AND-COMMISSIONING.md` | first boot, drills, daily rhythm |
| `docs/PARAMETERS.md` | **every** setting explained |
| `README.md` | wiring tables, pin map, control logic, website guide |

## Something is wrong
→ `docs/manual/05-ERRORS-AND-FIXES.md` (symptom → cause → fix table).

## I want to understand / change the code
| Read | For |
|---|---|
| `docs/manual/03-SOFTWARE-ARCHITECTURE.md` | modules, control laws, data flow |
| `docs/manual/08-RTOS-ARCHITECTURE.md` | the optional FreeRTOS mode |
| `docs/manual/09-FLASHING-WITH-PLATFORMIO.md` | pro flashing + OTA |
| `tools/` | assembler, verifiers, UI tests — run before shipping a change |

## I'm building units to sell / demoing it
| Read | For |
|---|---|
| `docs/PIN-MAP.md` | every component × every pin, both variants (the wiring source of truth) |
| `docs/PIN-CONFIG.txt` | the same pin map as a plain printable .txt (bench-friendly, no markdown) |
| `docs/WIRING-S3-PCF.txt` | **full wire-by-wire build sheet** — S3 + 3× PCF8574 + HX711 with load-cell colours, power, wire-count checklist |
| `docs/design-workflow.drawio` | **the full design as diagrams** (3 pages, 9:16): system architecture + operating workflow + **code workflow** (boot → loop → control core, mapped to components) — open in [app.diagrams.net](https://app.diagrams.net); SVG previews: `docs/design-system.svg`, `docs/design-workflow.svg`, `docs/design-code.svg` |
| `production/01-BOM-FULL.csv` | full bill of materials with costs |
| `production/06-COMPONENTS-CHECKLIST.md` | printable tick-box components checklist (v2.0, phase order) |
| `production/02-ASSEMBLY-CHECKLIST.md` | printable sign-off sheet |
| `production/03-QC-TEST-PROCEDURE.md` | 14-point test, pass criteria |
| `production/04` / `05` | serials & labels / warranty card |
| `demo/DEMO-SCRIPT.md` | the 10-minute demonstration flow |
| `compliance/` | safety checklist + certification roadmap |

## What's next for this product
→ `future/` — the roadmap (`future/README.md`) and one spec per upgrade:
load cells · recipe presets · remote alerts · solar telemetry · PWA ·
fleet · learning. Free-pin pool and ground rules live in the roadmap.

## Known limits, honestly stated
→ `docs/manual/06-LIMITATIONS-SOLVED-AND-UNSOLVED.md`.