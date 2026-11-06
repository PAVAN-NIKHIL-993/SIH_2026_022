#!/bin/sh
# Host test of arduino-ide/esp32_T1 (minimal ESP32 Dev + 2x DHT22 + BTS7960):
# the real sketch against a mock core with bit-exact DHT22 emulators and a
# 500 W chamber model - boot safety, DHT22 protocol (20-200 us response),
# full cycle, PID over 72 plant/target combinations, hard cut, sensor loss,
# heater watchdog, settings + reboot, /api JSON schema. Built for the core
# 3.x LEDC API (CI) and the core 2.x one.   needs: g++, python3
set -e
cd "$(dirname "$0")"
sketch="../../arduino-ide/esp32_T1/esp32_T1.ino"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
fail=0
for core in 3 2; do
  bin="$out/t1-core$core"
  if ! g++ -std=gnu++17 -O2 -Wall -Wextra -Wno-unused-parameter -Werror=format \
       -Imock -DESP_ARDUINO_VERSION_MAJOR=$core -DSKETCH="\"$sketch\"" harness.cpp -o "$bin" 2> "$bin.log"; then
    echo "FAIL  esp32_T1 [core $core.x]: does not compile against the mock"; head -30 "$bin.log"; exit 1
  fi
  if grep -q "esp32_T1.ino.*warning" "$bin.log"; then
    echo "FAIL  esp32_T1 [core $core.x]: compiler warnings in the sketch"; grep -A3 "esp32_T1.ino.*warning" "$bin.log"; fail=1
  fi
  if [ "$core" = 3 ]; then
    timeout 300 "$bin" all 2>/dev/null | python3 validate.py "esp32_T1 [core 3.x]" || fail=1
  else   # core 2.x: the LEDC channel API must drive the same heater
    timeout 300 "$bin" all 2>/dev/null | grep -E "^(JSON|FAIL|HARNESS)|cycle 60 C|invariants" \
      | python3 validate.py "esp32_T1 [core 2.x]" || fail=1
  fi
  T1_PREFS="$out/nvs$core" "$bin" save > /dev/null 2>&1
  T1_PREFS="$out/nvs$core" timeout 60 "$bin" load 2>/dev/null | python3 validate.py "esp32_T1 reboot [core $core.x]" || fail=1
done
[ "$fail" = 0 ] && echo "ALL ESP32_T1 TESTS PASSED" || { echo "ESP32_T1 TEST FAILED"; exit 1; }
