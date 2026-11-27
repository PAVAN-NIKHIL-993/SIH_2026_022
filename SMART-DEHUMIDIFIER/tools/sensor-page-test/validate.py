#!/usr/bin/env python3
"""Validate the /data JSON a sensor-test page polls (stdin = one JSON doc)."""
import json, sys
name = sys.argv[1] if len(sys.argv) > 1 else '?'
raw = sys.stdin.read().strip()
try:
    d = json.loads(raw)
except Exception as e:
    print(f"FAIL  {name}: invalid JSON ({e})\n      {raw[:300]}")
    sys.exit(1)
errs = []
if not isinstance(d.get('verdict'), str) or not d['verdict']:
    errs.append('verdict missing')
cards = d.get('cards')
if not isinstance(cards, list) or not cards:
    errs.append('cards missing/empty')
else:
    for i, c in enumerate(cards):
        for k in ('t', 'big'):
            if not isinstance(c.get(k), str):
                errs.append(f'card {i}: "{k}" not a string')
        rows = c.get('rows')
        if not isinstance(rows, list) or not rows:
            errs.append(f'card {i}: rows missing/empty')
        elif not all(isinstance(r, list) and len(r) == 2 and all(isinstance(x, str) for x in r) for r in rows):
            errs.append(f'card {i}: rows must be [["key","value"],...]')
if 'grid' in d:
    g = d['grid']
    if not (isinstance(g, list) and len(g) == 5 and all(isinstance(r, list) and len(r) == 4 for r in g)):
        errs.append('grid must be 5 rows x 4')
    if not (isinstance(d.get('seen'), dict) and len(d['seen']) == 16):
        errs.append('seen must map the 16 keys')
if errs:
    print(f"FAIL  {name}: " + '; '.join(errs)); sys.exit(1)
nrows = sum(len(c['rows']) for c in cards)
print(f"OK    {name}: verdict {d['verdict']}, {len(cards)} card(s), {nrows} row(s)")
