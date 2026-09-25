#!/bin/sh
# Host-side regression test for the DS1302 driver (src/rtc.cpp).
# Compiles the REAL driver source against a datasheet-faithful fake chip
# and checks the full protocol: address/R-W bits, CE, burst auto-increment,
# BCD, 12 h mode, CH/WP flags, tz conversion, day-of-week, RAM probe.
#   needs: g++ (any linux/mac host)
set -e
cd "$(dirname "$0")"
cp ../../src/rtc.cpp rtc-under-test.cpp
g++ -std=c++17 -Wall -Wextra -I. -I../../src \
  -DRTC_ENABLED=1 -DPIN_RTC_RST=40 -DPIN_RTC_SCLK=42 -DPIN_RTC_IO=47 \
  rtc-under-test.cpp test_main.cpp mock_chip.cpp -o rtctest-build
./rtctest-build
