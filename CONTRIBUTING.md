# Contributing

- One rule above all: **the firmware stays library-free** (single-file
  Arduino IDE build is a product feature) and **everything works offline**
  (hotspot first; internet only via the owner's phone).
- Before opening a PR: `bash SMART-DEHUMIDIFIER/scripts/check-all.sh` must pass —
  it regenerates the single-file sketch + online UI, runs the string/printf
  scanners and both UI test suites, and fails if generated files are stale.
- Pin map is frozen; new hardware uses the free-pin pool only (S3: GPIO 3,
  11 — 40/42/47 = DS1302 RTC since v2.0.21 — see `SMART-DEHUMIDIFIER/docs/PARAMETERS.md`).
- Touching `src/webui.h` (strings!) means the scanners run twice — on src
  AND the regenerated `.ino`. They catch real crashes; trust them.
- Releases: bump `FW_VERSION` in `src/config.h`, update `CHANGELOG.md`,
  export the `.bin`, attach it to a GitHub Release — owners install via
  the website's **Firmware update** page.