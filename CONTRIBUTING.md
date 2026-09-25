# Contributing

- One rule above all: **the firmware stays library-free** (single-file
  Arduino IDE build is a product feature) and **everything works offline**
  (hotspot first; internet only via the owner's phone).
- Before opening a PR: `bash SMART-DEHUMIDIFIER/scripts/check-all.sh` must pass —
  it regenerates both single-file sketches + the online UI, runs the
  string/printf scanners, the symbol audit, the host DS1307 driver test, the
  sensor-test page JSON check and
  both UI test suites, and fails if generated files are stale.
- CI also **really compiles** every sketch (both firmware variants, the S3
  bench sketches, the UNO display sketches) with arduino-cli. Same thing
  locally: `bash SMART-DEHUMIDIFIER/scripts/compile-sketches.sh` (needs
  arduino-cli; first run downloads the pinned ESP32/AVR cores).
- Pin map is frozen; new hardware uses the free-pin pool only (S3: GPIO 3,
  11, 40, 42, 47 — the DS1307 RTC sits on the I2C bus since v2.0.23 — see
  `SMART-DEHUMIDIFIER/docs/PARAMETERS.md`).
- Touching `src/webui.h` (strings!) means the scanners run twice — on src
  AND the regenerated `.ino`. They catch real crashes; trust them.
- Releases: bump `FW_VERSION` in `src/config.h` **and**
  `variants/esp32-s3/config-s3.h`, update `CHANGELOG.md`,
  export the `.bin`, attach it to a GitHub Release — owners install via
  the website's **Firmware update** page.
