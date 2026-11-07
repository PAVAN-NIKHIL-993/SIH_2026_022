# SPEC 02 · Recipe presets

Named drying profiles, selectable in one tap on the website. **Built-in
starter library compiled into the firmware by incense type** (offline — no
internet needed), plus user recipes saved from the Parameters page.

**Built-in library by type (starting values — tune with experience):**

| Type | setTemp | Time | Fans | Notes |
|---|---|---|---|---|
| Sandal (premium) | 45 °C | 6 h | gentle 40/60 | slow = fragrance survives |
| Masala | 80 °C | 2.5 h | 60/80 | fast cure |
| Charcoal pre-dry | 95 °C | 90 min | 50/100 | dry before dip; sensors in return path |
| Flower / herbal | 50 °C | 5 h | 30/50 | delicate, colour retention |
| Durbar (wet paste) | 70 °C | 4 h | 60/70 | ends by RH gate 30 % |
| Agarbatti general | 60 °C | 3 h | 50/70 | the safe default |

## Hardware
- None. Pure firmware/UI.

## Firmware
- Settings already versioned in NVS; add a recipe store (name + Settings
  blob × 8 slots) in `control.cpp`; `POST /api/recipe` {save|apply|delete}.
- Applying a recipe = `applySettings()` live mid-cycle (already supported).

## Website
- Dashboard: recipe chips row (tap = whole configuration + optional Start).
- Parameters: "save current as recipe" input.

## Effort & risks
- Effort: a weekend. Risks: none beyond NVS wear (8 slots, rare writes).
