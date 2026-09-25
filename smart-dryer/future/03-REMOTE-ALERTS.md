# SPEC 03 · Remote alerts (Telegram) — OPTIONAL, needs phone data

**Rural-first note:** this is the ONLY upgrade that needs internet (on
the owner's PHONE, not the dryer). Everything else in `future/` works with
zero connectivity. Build it last / only if wanted.

The dryer is offline by design (hotspot only) — but the OWNER'S PHONE has
internet while connected to the hotspot (mobile data + WiFi, the same trick
the weather card already uses, in reverse).

## Design (no ESP32 internet, ever)
- The website dashboard already polls `/api/data`; add: when the ESP raises
  FAULT/DONE/BYPASS or a `[warn]` event, the page (still open on the phone)
  sends a Telegram message via `https://api.telegram.org` using a bot token
  + chat id stored in the browser's localStorage.
- The ESP only exposes the event in `/api/data` (`events: [...]` queue);
  the browser does the internet part. Works with the built-in page AND the
  online UI.

## Firmware
- Small ring buffer of recent events (type, timestamp, message) in
  `/api/data`; cleared on read. ~40 lines.

## Website
- Settings dialog (badge 🔔): bot token, chat id, "send test". Persists in
  localStorage. Messages: cycle done, fault + reason, battery low, sensor
  missing, OTA done.

## Effort & risks
- Effort: a weekend. Risks: page must be open (document it); token lives in
  the phone's localStorage (documented, revocable, phone-local).