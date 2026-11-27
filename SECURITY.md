# Security & safety reporting

**Hardware safety first:** if a unit overheats, smokes, or the battery
swells — disconnect the battery fuse and do not re-run it.

Report security/safety issues privately to the maintainer (do not open a
public issue for safety-critical faults). Include: serial number, FW
version (website header), the fault text, and the `[stat]` serial lines.

Scope notes (by design, documented): the hotspot is open to anyone in WiFi
range with the printed password (rural-first trade-off — change
`AP_PASS` in config.h for sensitive sites); the website has no
authentication; OTA is refused while a cycle runs but is not
password-protected beyond the WiFi password.
