#!/usr/bin/env bash
# REAL C++ compile of every Arduino sketch in arduino-ide/ with arduino-cli.
# check-all.sh never runs a compiler (string scans + UI tests only); this
# does - it is what would have caught the v2.0.21 import corruption.
# CI runs it on every push (job "compile"); it runs the same locally.
#
# Usage (from anywhere in the repo; needs arduino-cli on PATH -
# https://arduino.github.io/arduino-cli/latest/installation/):
#   bash scripts/compile-sketches.sh esp32  # firmware classic + S3, S3 bench sketches
#   bash scripts/compile-sketches.sh avr    # UNO display bridge + display test
#   bash scripts/compile-sketches.sh        # both groups
#
# First run installs the pinned cores + libraries into ~/.arduino15 and
# ~/Arduino/libraries (~1 GB for the ESP32 core). Board options mirror the
# documented Arduino IDE settings (variants/esp32-s3/README.md).
set -euo pipefail
cd "$(dirname "$0")/.."

ESP32_CORE="esp32:esp32@${ESP32_CORE_VERSION:-3.3.12}"
AVR_CORE="arduino:avr@${AVR_CORE_VERSION:-1.8.8}"
ESP32_INDEX="https://espressif.github.io/arduino-esp32/package_esp32_index.json"
AVR_LIBS=("Adafruit GFX Library@1.12.6" "MCUFRIEND_kbv@3.0.0")

FQBN_CLASSIC="esp32:esp32:esp32"                                       # ESP32 Dev Module
FQBN_S3="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=disabled" # ESP32S3 Dev Module
FQBN_UNO="arduino:avr:uno"

group="${1:-all}"
case "$group" in esp32|avr|all) ;; *) echo "usage: $0 [esp32|avr|all]"; exit 2 ;; esac
command -v arduino-cli >/dev/null || { echo "arduino-cli not found on PATH"; exit 2; }

in_ci() { [ "${GITHUB_ACTIONS:-}" = true ]; }
gh_group()    { if in_ci; then echo "::group::$*"; else echo "== $*"; fi; }
gh_endgroup() { if in_ci; then echo "::endgroup::"; fi; }

passed=(); failed=()
build() {   # build <fqbn> <sketch-dir>
  gh_group "compile ${2}  [${1}]"
  if arduino-cli compile --fqbn "$1" --warnings default "$2"; then
    gh_endgroup; passed+=("$2")
  else
    gh_endgroup; failed+=("$2")
    if in_ci; then echo "::error title=compile failed::$2 ($1)"; fi
  fi
}

if [ "$group" != avr ]; then
  gh_group "install ${ESP32_CORE}"
  arduino-cli core update-index --additional-urls "$ESP32_INDEX"
  arduino-cli core install "$ESP32_CORE" --additional-urls "$ESP32_INDEX"
  gh_endgroup
  build "$FQBN_CLASSIC" arduino-ide/SMART-DEHUMIDIFIER-single-file
  build "$FQBN_S3"      arduino-ide/SMART-DEHUMIDIFIER-s3-single-file
  build "$FQBN_S3"      arduino-ide/board-test-s3
  for d in arduino-ide/sensor-tests/*/; do build "$FQBN_S3" "${d%/}"; done
fi

if [ "$group" != esp32 ]; then
  gh_group "install ${AVR_CORE} + display libraries"
  arduino-cli core update-index
  arduino-cli core install "$AVR_CORE"
  arduino-cli lib install "${AVR_LIBS[@]}"
  gh_endgroup
  build "$FQBN_UNO" arduino-ide/display-bridge-uno
  build "$FQBN_UNO" arduino-ide/display-test-uno
fi

echo
echo "compiled OK: ${#passed[@]}   failed: ${#failed[@]}"
for s in "${failed[@]}"; do echo "  FAIL  $s"; done
if [ "${#failed[@]}" -ne 0 ]; then exit 1; fi
echo "ALL SKETCHES COMPILE"
