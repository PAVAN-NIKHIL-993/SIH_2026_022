# S3 + PCF #2 — pin connections

## ESP32-S3

| GPIO | Connection |
|---|---|
| 0 | BOOT button (on the board — nothing to wire) |
| 1 | HX711 SCK |
| 2 | HX711 DT |
| 4 | battery divider midpoint |
| 5 | 2N7000 gate (divider low side) |
| 8 | I2C0 SDA → AHT10 + PCF #1 + PCF #2 + AT24C256 (0x50) SDA |
| 9 | I2C0 SCL → AHT10 + PCF #1 + PCF #2 + AT24C256 (0x50) SCL |
| 10 | DHT22 DATA (chamber cool-return) |
| 11 | spare |
| 12 | 555 shifter pin 4 (555 pin 3 → BTS7960 RPWM) |
| 13 | BTS7960 R_EN + L_EN (jumpered) |
| 14 | P-MOSFET latch hold |
| 17 | L298N ENB (fan PWM) |
| 18 | supply optocoupler OUT |
| 21 | door limit switch (other leg GND, closed = LOW) |
| 41 | DHT11 DATA (outdoor) |
| 48 | on-board RGB status pixel (WS2812 data, SPI-driven) |
| 3, 11 | spare — **parked pull-down at boot** (v2.0.18) — 40/42/47 went to the DS1302 RTC in v2.0.21 |

Unused pads (the spare set above) are set `INPUT_PULLDOWN` at boot —
disabled, never floating, lower power. Never parked: 35/36/37 (R8 PSRAM
lines), 0/45/46 (boot straps), 19/20 (USB), 43/44 (UART0), 26–32 (flash).
BLE is disabled and its RAM released at boot (`[pwr] BLE OFF`).
| 35, 36, 37 | PSRAM (N16R8) — never wired |
| 45, 46 | strapping — keep free |

## PCF #2 @ 0x21 (front panel)

| Pin | Connection |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |
| A0 | 3V3 |
| A1 | GND |
| A2 | GND |
| P0 | relay IN1 (CH1 = solar feed) |
| P1 | relay IN2 (CH2 = bypass feed) |
| P2 | BUTTON-1 (other leg GND) |
| P3 | BUTTON-2 (other leg GND) |
| P4 | SOLAR toggle common (solar throw → 3V3, bypass throw → GND) |
| P5 | optocoupler OUT |
| P6 | door limit switch (other leg GND) |
| P7 | buzzer − (buzzer + → 3V3) |
| P0–P7 | 10 kΩ pull-up each → 3V3 |
