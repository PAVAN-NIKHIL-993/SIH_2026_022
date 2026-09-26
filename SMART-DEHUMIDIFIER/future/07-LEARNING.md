# SPEC 07 · Learn from every batch

The dryer already archives every cycle (CSV in flash + optional GitHub
backup). Add the loop that turns archives into better defaults.

## Design
- Offline (on the phone/PC): a small script (or the online UI's "Insights"
  tab) compares batches: same recipe, weather, weight-loss curves, final
  quality rating (the owner taps 1–5 stars after unloading — one new field
  in the cycle log).
- Output: suggested recipe tweaks ("Sandal at 46 °C for 3.2 h dried 4 %
  faster with equal rating — apply?") — the owner applies, the pillar runs.
- NO on-device ML; the ESP stays simple, fast and verifiable.

## Firmware
- Cycle record gains `rating` (set via website after DONE) + weather summary
  of the run. ~30 lines.

## Website
- After DONE: star prompt. Insights tab (online UI): table + simple curves.

## Effort & risks
- Effort: 2 weekends. Risks: suggestion quality depends on honest ratings;
  keep suggestions advisory only.
