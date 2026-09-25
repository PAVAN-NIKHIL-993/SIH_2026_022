#!/usr/bin/env bash
# One-command release gate: regenerate everything, run every check.
# Usage:  bash scripts/check-all.sh     (from anywhere in the repo)
set -euo pipefail
cd "$(dirname "$0")/.."

echo "== 1/4 regenerate sketches (both variants) + online UI =="
node tools/single-file/assemble.js
node tools/single-file/assemble.js s3
node tools/online/sync-online.js

echo "== 2/4 string + printf scans =="
python3 tools/verify_strings.py arduino-ide/SMART-DEHUMIDIFIER-single-file/SMART-DEHUMIDIFIER-single-file.ino arduino-ide/SMART-DEHUMIDIFIER-s3-single-file/SMART-DEHUMIDIFIER-s3-single-file.ino src/*.cpp src/*.h variants/esp32-s3/config-s3.h
python3 tools/verify_strings.py --printf arduino-ide/SMART-DEHUMIDIFIER-single-file/SMART-DEHUMIDIFIER-single-file.ino arduino-ide/SMART-DEHUMIDIFIER-s3-single-file/SMART-DEHUMIDIFIER-s3-single-file.ino src/*.cpp

echo "== 3/4 UI test suites =="
cd tools/ui-test
[ -d node_modules ] || npm install --no-audit --no-fund
node ui-test.js
node online-test.js
cd ../..

echo "== 4/4 generated files in sync with sources (both variants) =="
git diff --exit-code -- arduino-ide docs/index.html

echo "ALL CHECKS PASSED"
