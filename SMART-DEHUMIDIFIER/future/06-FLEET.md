# SPEC 06 · Multi-pillar fleet

One phone, many dryers (family / small production): each pillar gets an
instance number (config.h `PILLAR_ID`), the AP SSID becomes
`AgarbattiDryer-2` etc., and the online UI stores a pillar list with names.

## Firmware
- `PILLAR_ID` define; SSID/AP report it; `/api/data` gains `id`+`name`.
- No other changes — each pillar stays fully standalone (fleet is a UI
  concept, not a protocol).

## Website
- Pillar switcher in the header (per-pillar last-IP/name in localStorage);
  the phone connects to one pillar hotspot at a time and the UI remembers
  the data of the others from their last syncs + GitHub CSV backups.

## Effort & risks
- Effort: 2 weekends (mostly UI state). Risks: keep the single-pillar
  experience untouched (default PILLAR_ID 1 = today's behaviour).