# SPEC 04 · Solar / MPPT telemetry

Make the 550 W advantage visible: live panel watts, battery current, Ah
counted, and "can I dry today?" forecasting from accumulated sun.

## Hardware
- Option A (zero wiring): the battery divider already gives volts — extend
  the existing model with coulomb counting (%/h while running vs charging).
- Option B (accurate): INA226 I2C power monitor (₹250) on the battery lead,
  address 0x40 (no conflict: AHT10s are 0x38 on their own buses). Wire to
  **GPIO21/22** (shared Wire) — same bus, different address.

## Firmware
- Option B: `solar.{h,cpp}` minimal INA226 driver (register pokes, no
  library); 1 Hz V/A/W into the data log + `[stat]`.
- "Solar forecast for drying": today's Wh harvested vs Wh a cycle needs
  (from history) → website badge **"Drying possible: YES / after 2 h"**.

## Website
- Solar card: W now, Wh today, battery A in/out, forecast badge.
- Graph gains the W series.

## Effort & risks
- Effort: weekend (A), weekend+ (B). Risks: shared-bus mutex already in
  place (RTOS mode); keep polls short.