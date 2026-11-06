#!/usr/bin/env python3
"""Reads harness output on stdin: every "JSON {...}" line must be valid JSON
with the exact /api schema the web page relies on; "FAIL" lines and a
missing "HARNESS DONE" (crash) fail the run. Usage: harness | validate.py NAME"""
import json
import sys

name = sys.argv[1] if len(sys.argv) > 1 else "esp32_T1"
NUM_OR_NULL = ("t", "h", "t1", "h1", "t2", "h2")
bad, n_json, done = 0, 0, False


def problem(text):
    global bad
    bad += 1
    print(f"FAIL  {name}: {text}")


for line in sys.stdin:
    line = line.rstrip("\n")
    if line.startswith("JSON "):
        n_json += 1
        try:
            d = json.loads(line[5:])
        except ValueError as e:
            problem(f"invalid JSON ({e}): {line[5:120]}")
            continue
        if d.get("state") not in ("IDLE", "RUNNING", "DONE", "FAULT"):
            problem(f"bad state {d.get('state')!r}")
        if d.get("phase") not in ("", "HEAT-UP", "HOLD") or (d["phase"] and d["state"] != "RUNNING"):
            problem(f"bad phase {d.get('phase')!r} in {d.get('state')}")
        for k in NUM_OR_NULL:
            if k not in d or not (d[k] is None or isinstance(d[k], (int, float))):
                problem(f"{k} must be a number or null: {d.get(k)!r}")
        for k, lo, hi in (("set", 40, 80), ("min", 1, 1440), ("heat", 0, 100),
                          ("el", 0, 86400), ("left", 0, 86400)):
            if not isinstance(d.get(k), int) or not lo <= d[k] <= hi:
                problem(f"{k}={d.get(k)!r} outside {lo}..{hi}")
        if not isinstance(d.get("msg"), str) or not isinstance(d.get("fw"), str):
            problem("msg / fw must be strings")
        if d.get("state") != "RUNNING" and d.get("heat") != 0:
            problem(f"heater {d.get('heat')} % while {d.get('state')}")
    elif line.startswith("FAIL"):
        bad += 1
        print(line)
    elif line.startswith("OK"):
        print(line)
    elif line.startswith("HARNESS DONE"):
        done = True

if not done:
    problem("harness did not finish (crash or hang)")
if n_json == 0 and "reboot" not in name:
    problem("no /api snapshots were checked")
print(f"{'OK  ' if not bad else 'FAIL'}  {name}: {n_json} /api snapshots valid" if not bad
      else f"FAIL  {name}: {bad} problem(s)")
sys.exit(1 if bad else 0)
