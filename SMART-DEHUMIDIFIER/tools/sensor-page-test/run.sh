#!/bin/sh
# Host test: every sensor-test sketch's phone page gets VALID JSON from /data.
# Compiles each arduino-ide/sensor-tests/*/*.ino against a small Arduino mock
# (mock/), for the S3 and the classic ESP32, with nothing connected and with
# every I2C device present, runs setup() + ~20 s of loop(), then parses the
# JSON the page polls. (Until v2.0.23 every page with 2+ rows sent invalid
# JSON and sat on "loading..." forever.)   needs: g++, python3
set -e
cd "$(dirname "$0")"
out="$(mktemp -d)"
fail=0
for ino in ../../arduino-ide/sensor-tests/*/*.ino; do
  name=$(basename "$ino" .ino)
  for tgt in s3 classic; do
    def=""; [ "$tgt" = s3 ] && def="-DCONFIG_IDF_TARGET_ESP32S3=1"
    bin="$out/$name-$tgt"
    if ! g++ -std=gnu++17 -w -Imock $def -DSKETCH="\"$ino\"" harness.cpp -o "$bin" 2> "$bin.log"; then
      echo "FAIL  $name [$tgt]: does not compile against the mock"; head -20 "$bin.log"; fail=1; continue
    fi
    for sc in 0 1; do
      if ! timeout 20 "$bin" $sc 2>/dev/null > "$bin.$sc.json"; then
        echo "FAIL  $name [$tgt, scenario $sc]: crashed or hung"; fail=1; continue
      fi
      python3 validate.py "$name [$tgt, $( [ $sc = 0 ] && echo 'nothing connected' || echo 'devices present')]" < "$bin.$sc.json" || fail=1
    done
  done
done
rm -rf "$out"
[ "$fail" = 0 ] && echo "ALL SENSOR PAGES SEND VALID JSON" || { echo "SENSOR PAGE TEST FAILED"; exit 1; }
