# SPEC 05 · PWA — installable app

Make the dashboard install like an app (icon on the home screen, full-screen,
offline cache) — served from the ESP itself, still no internet needed.

## Firmware
- Serve `manifest.webmanifest` + a 192×192 icon (littlefs or PROGMEM) from
  `/`; add `<link rel="manifest">` to the page head. Service worker is
  NOT usable from the hotspot origin for offline caching of the ESP page —
  instead cache the ONLINE UI (GitHub Pages) as the PWA, which talks to
  whichever hotspot it is on (espFetchBridge already does this).

## Website (online UI)
- `manifest.json` + service worker on GitHub Pages: the online page caches
  itself; on the phone it installs as "Dryer Pillar" and works offline for
  viewing the last data; live data needs the hotspot range.

## Effort & risks
- Effort: days. Risks: none; purely additive.
