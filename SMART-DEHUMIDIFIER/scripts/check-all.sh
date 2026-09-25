#!/usr/bin/env bash
# One-command release gate: regenerate everything, run every check.
# Usage:  bash scripts/check-all.sh     (from anywhere in the repo)
# The C++ compile of the sketches is separate (needs arduino-cli + ~1 GB of
# cores): bash scripts/compile-sketches.sh  - CI runs both on every push.
set -euo pipefail
cd "$(dirname "$0")/.."

echo "== 1/6 regenerate sketches (both variants) + online UI =="
node tools/single-file/assemble.js
node tools/single-file/assemble.js s3
node tools/online/sync-online.js

echo "== 2/6 string + printf scans =="
python3 tools/verify_strings.py arduino-ide/SMART-DEHUMIDIFIER-single-file/SMART-DEHUMIDIFIER-single-file.ino arduino-ide/SMART-DEHUMIDIFIER-s3-single-file/SMART-DEHUMIDIFIER-s3-single-file.ino src/*.cpp src/*.h variants/esp32-s3/config-s3.h
python3 tools/verify_strings.py --printf arduino-ide/SMART-DEHUMIDIFIER-single-file/SMART-DEHUMIDIFIER-single-file.ino arduino-ide/SMART-DEHUMIDIFIER-s3-single-file/SMART-DEHUMIDIFIER-s3-single-file.ino src/*.cpp

echo "== 3/6 symbol audit (lost-edit detector) =="
python3 tools/audit-symbols.py

echo "== 4/6 DS1302 driver host test (real src/rtc.cpp vs fake chip) =="
if command -v g++ >/dev/null 2>&1; then
  sh tools/host-rtc-test/run.sh
elif [ "${CI:-}" = true ]; then
  echo "g++ is required in CI"; exit 1
else
  echo "SKIP - g++ not installed (CI runs this step)"
fi

echo "== 5/6 UI test suites =="
cd tools/ui-test
[ -d node_modules ] || npm install --no-audit --no-fund
node ui-test.js
node online-test.js
cd ../..

echo "== 6/6 generated files in sync with sources (both variants) =="
git diff --exit-code -- arduino-ide docs/index.html

echo "ALL CHECKS PASSED"
