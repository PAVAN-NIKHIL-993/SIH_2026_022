#!/bin/sh
# Host-side regression test for the DS1307 driver (src/rtc.cpp).
# Compiles the REAL driver source against a datasheet-faithful fake chip on
# a mock Wire bus and checks: detection (ACK), burst read, BCD, 12/24 h
# decoding incl. 20:00-23:59, CH (clock halt) handling, tz conversion,
# day-of-week, junk rejection, NACK/absent chip, RAM + control untouched.
#   needs: g++ (any linux/mac host)
set -e
cd "$(dirname "$0")"
cp ../../src/rtc.cpp rtc-under-test.cpp
g++ -std=c++17 -Wall -Wextra -I. -I../../src \
  rtc-under-test.cpp test_main.cpp mock_chip.cpp -o rtctest-build
./rtctest-build
