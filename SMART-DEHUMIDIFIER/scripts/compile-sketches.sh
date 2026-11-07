#!/usr/bin/env bash
# REAL C++ compile of every Arduino sketch in arduino-ide/ with arduino-cli.
# check-all.sh never runs a compiler (string scans + UI tests only); this
# does - it is what would have caught the v2.0.21 import corruption.
# CI runs it on every push (job "compile"); it runs the same locally.
#
# Usage (from anywhere in the repo; needs arduino-cli on PATH -
# https://arduino.github.io/arduino-cli/latest/installation/):
#   bash scripts/compile-sketches.sh esp32  # firmware classic + S3 (+ S3 in DRYER_RTOS=1
#                                           # mode), S3 bench sketches
#   bash scripts/compile-sketches.sh avr    # UNO display bridge + display test
#   bash scripts/compile-sketches.sh        # both groups
#   bash scripts/compile-sketches.sh esp32 --setup-only   # install, no build
#
# First run installs the pinned cores + libraries into ~/.arduino15 and
# ~/Arduino/libraries (~1 GB for the ESP32 core). Board options mirror the
# documented Arduino IDE settings (variants/esp32-s3/README.md).
set -euo pipefail
cd "$(dirname "$0")/.."

ESP32_CORE="esp32:esp32@${ESP32_CORE_VERSION:-3.3.12}"
AVR_CORE="arduino:avr@${AVR_CORE_VERSION:-1.8.8}"
ESP32_INDEX="https://espressif.github.io/arduino-esp32/package_esp32_index.json"
AVR_LIBS=("Adafruit GFX Library@1.12.6" "MCUFRIEND_kbv@3.0.0-Release")

FQBN_CLASSIC="esp32:esp32:esp32"                                       # ESP32 Dev Module
FQBN_S3="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=disabled" # ESP32S3 Dev Module
FQBN_UNO="arduino:avr:uno"

group="${1:-all}"
setup_only=false; [ "${2:-}" = --setup-only ] && setup_only=true
case "$group" in esp32|avr|all) ;; *) echo "usage: $0 [esp32|avr|all] [--setup-only]"; exit 2 ;; esac
command -v arduino-cli >/dev/null || { echo "arduino-cli not found on PATH"; exit 2; }
acli() { arduino-cli --no-color "$@"; }   # plain logs (no ANSI in annotations)

in_ci() { [ "${GITHUB_ACTIONS:-}" = true ]; }
gh_group()    { if in_ci; then echo "::group::$*"; else echo "== $*"; fi; }
gh_endgroup() { if in_ci; then echo "::endgroup::"; fi; }

# CI: ONE annotation per failed sketch, pinned to its first compiler error
# (when that is inside the repo) and carrying the key lines + the log tail.
# Linker / size / install errors have no file:line:col, and a plain gcc
# problem matcher would burn the 10-per-step annotation budget on the first
# broken sketch - so the script reports instead.
annotate_fail() {   # annotate_fail <short-title> <what> <logfile>
  in_ci || return 0
  local loc file="" line="" props="title=$1"
  loc="$(grep -m1 -oE '^[^ :][^:]*:[0-9]+:[0-9]+: (fatal )?error:' "$3" || true)"
  if [ -n "$loc" ]; then
    file="${loc%%:*}"; line="$(echo "$loc" | cut -d: -f2)"
    file="${file#"${GITHUB_WORKSPACE:-}/"}"
    case "$file" in /*) ;; *) props="file=$file,line=$line,$props" ;; esac
  fi
  { echo "$2"
    grep -E -m 12 'error|undefined reference|multiple definition|too big|overflow|exceeds' "$3" || true
    echo "..."; tail -n 6 "$3"; } \
    | sed -e 's/%/%25/g' -e 's/\r//g' \
    | sed -e ':a' -e 'N' -e '$!ba' -e 's/\n/%0A/g' -e "s|^|::error ${props}::|"
}

step() {    # step <title> <cmd...> : run a setup command, stop on failure
  local title="$1"; shift
  local log; log="$(mktemp)"
  if "$@" 2>&1 | tee "$log"; then rm -f "$log"; return 0; fi
  annotate_fail "setup failed" "$title" "$log"; rm -f "$log"
  echo "FAILED: $title"; exit 1
}

passed=(); failed=()
build() {   # build <fqbn> <sketch-dir> [build-property]
  local prop="${3:-}" label="$2" log; log="$(mktemp)"
  if [ -n "$prop" ]; then prop="${prop#*=}"; label="$2  (${prop#-D})"; fi   # "(DRYER_RTOS=1)"
  gh_group "compile ${label}  [${1}]"
  if acli compile --fqbn "$1" --warnings default ${3:+--build-property "$3"} "$2" 2>&1 | tee "$log"; then
    gh_endgroup; passed+=("$label")
  else
    gh_endgroup; failed+=("$label")
    annotate_fail "compile failed" "$label  [$1]" "$log"
  fi
  rm -f "$log"
}

if [ "$group" != avr ]; then
  gh_group "install ${ESP32_CORE}"
  step "update ESP32 index" acli core update-index --additional-urls "$ESP32_INDEX"
  step "install $ESP32_CORE" acli core install "$ESP32_CORE" --additional-urls "$ESP32_INDEX"
  gh_endgroup
fi
if [ "$group" != avr ] && ! $setup_only; then
  build "$FQBN_CLASSIC" arduino-ide/SMART-DEHUMIDIFIER-single-file
  build "$FQBN_S3"      arduino-ide/SMART-DEHUMIDIFIER-s3-single-file
  # the optional FreeRTOS build mode (config.h: DRYER_RTOS, #ifndef-guarded).
  # compiler.cpp.extra_flags is empty in the core; build.extra_flags is not
  # (it carries the S3 USB settings), so it must not be overridden
  build "$FQBN_S3"      arduino-ide/SMART-DEHUMIDIFIER-s3-single-file "compiler.cpp.extra_flags=-DDRYER_RTOS=1"
  build "$FQBN_S3"      arduino-ide/board-test-s3
  for d in arduino-ide/sensor-tests/*/; do build "$FQBN_S3" "${d%/}"; done
fi

if [ "$group" != esp32 ]; then
  gh_group "install ${AVR_CORE} + display libraries"
  step "update index" acli core update-index
  step "install $AVR_CORE" acli core install "$AVR_CORE"
  step "update library index" acli lib update-index
  step "install ${AVR_LIBS[*]}" acli lib install "${AVR_LIBS[@]}"
  gh_endgroup
fi
if [ "$group" != esp32 ] && ! $setup_only; then
  build "$FQBN_UNO" arduino-ide/display-bridge-uno
  build "$FQBN_UNO" arduino-ide/display-test-uno
fi

if $setup_only; then echo "setup done ($group)"; exit 0; fi
echo
echo "compiled OK: ${#passed[@]}   failed: ${#failed[@]}"
for s in "${failed[@]}"; do echo "  FAIL  $s"; done
if [ "${#failed[@]}" -ne 0 ]; then exit 1; fi
echo "ALL SKETCHES COMPILE"
