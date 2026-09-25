# Standards & certification roadmap (before commercial sale)

Not legal advice — talk to a certification agency before selling. Plan:

1. **Electrical safety** — the product is extra-low-voltage DC (12.8 V
   domain) end to end except the optional AC bypass relay contactor. Keep
   the AC side OUT of the pillar if possible; if AC bypass is fitted, that
   side needs wiring to local mains code by a licensed electrician.
2. **Battery compliance** — ship with a LiFePO4 pack from a reputable
   maker that carries its own certification (UN38.3 for transport, IEC
   62133); do not self-build packs for sale.
3. **EMC** — PWM heater + fans: a CE/FCC-class EMC pre-scan on a prototype
   (local labs, ~1–2 days) before mass production.
4. **Documentation pack** — this repo already is most of it: datasheet,
   PARAMETERS, every-inch build, QC procedure, safety checklist, BOM.
   A tech file folder per serial number (QC sheet + FW version) = traceability.
5. **Warranty & service** — card template in `production/05`; log serials.
6. **Govt scheme demos** — the demo script (`demo/DEMO-SCRIPT.md`) is
   structured for evaluation committees: evidence (CSV logs) over claims.
