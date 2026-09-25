# SPEC 01 · Load cells — dry to weight   ·   ✅ BUILT (v1.6)

Shipped in the main firmware: `src/scale.*`, dry-to-weight completion
gate (Settings v6), website Tare/Calibrate + weight tile + graph series,
`wt_g` CSV column, S3 pins CLK=1/DOUT=2. This file stays as the design
record — hardware guidance (cell mounting outside the hot zone) below
is still current.

**The #1 recommended upgrade.** Drying by time+RH is a proxy; weight is the
truth. When the batch stops losing weight, it is dry — regardless of
weather, batch size or load density.

## Hardware
- 2 × 5 kg half-bridge load cells + HX711 24-bit ADC board (~₹300 total),
  under the two tray rails of the BOTTOM tray (the heaviest, most stable
  signal). Total batch limit 10 kg.
- Wiring — **S3 (built in, enabled by default): CLK→GPIO1, DOUT→GPIO2**,
  VCC→3V3, GND→GND (see `docs/PARAMETERS.md` → Dry to weight).
  Classic option: **CLK→GPIO12, DOUT→GPIO34** with `DISPLAY_ENABLED 0`
  (12 is the display SCK when a screen is fitted). E+ always-on is fine.

## Firmware
- New `scale.{h,cpp}`: bit-banged HX711 read (no library), 10 Hz averaged to
  0.1 Hz; tare button on the website; NVS-stored calibration factor; weight
  logged into the cycle record (CSV gains a `wt` column, versioned bump).
- **Dry-to-weight completion rule**: cycle ends when d(weight)/dt < X g/min
  for Y minutes (defaults X=2, Y=10) — added as a third completion gate
  beside time and RH (`requireWeight` in Settings, default off).
- `[stat]` line gains `wt=1234g dwdt=1.2g/min`.

## Website
- Weight tile + grams-per-minute trend line (6th graph series, silver).
- Parameters page: enable dry-to-weight, X/Y thresholds, tare button.

## Effort & risks
- Effort: a weekend (half firmware, half calibration UI).
- Risks: thermal drift of the cells (mount them OUTSIDE the hot chamber, on
  the tray rail bushings that pass through the wall); HX711 rated 85 °C —
  keep the ADC board in the control box.