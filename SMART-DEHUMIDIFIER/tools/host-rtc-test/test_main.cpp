// Host test of src/rtc.cpp (DS1307 on I2C) against the fake chip in
// mock_chip.cpp. Build + run: sh run.sh
#include "Arduino.h"
#include "Wire.h"
#include "control.h"
#include "rtc.h"
void rtc_test_set_now(time_t t);

Settings cfg;

static time_t refEpoch(int y, int mo, int d, int h, int mi, int s) {
  struct tm t; memset(&t, 0, sizeof t);
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
  t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
  return mktime(&t);   // TZ=UTC0 set in main
}

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
  for (int i_ = 0; i_ < 8; i_++) fprintf(stderr, "reg[%d]=0x%02X ", i_, gRtc.reg[i_]); \
  fprintf(stderr, "\n"); exit(1); } } while (0)

static void setTime(uint8_t s, uint8_t mi, uint8_t h, uint8_t dow, uint8_t d, uint8_t mo, uint8_t y) {
  gRtc.reg[0] = s; gRtc.reg[1] = mi; gRtc.reg[2] = h; gRtc.reg[3] = dow;
  gRtc.reg[4] = d; gRtc.reg[5] = mo; gRtc.reg[6] = y;
}
static void clearWriteLog() { memset(gRtc.writes, 0, sizeof gRtc.writes); }
static int writesOutsideTime() { int n = 0; for (int r = 7; r < 64; r++) n += gRtc.writes[r]; return n; }

int main() {
  setenv("TZ", "UTC0", 1);
  tzset();
  cfg.tzMinutes = 330;                    // IST = UTC+5:30
  memset(&gRtc, 0, sizeof gRtc);
  for (int r = 8; r < 64; r++) gRtc.reg[r] = (uint8_t)(0xA0 + r);   // user RAM
  gRtc.reg[7] = 0x10;                     // control: SQWE set by someone else
  time_t ep;

  // ---- 1) detect + readTime: chip holds LOCAL 2026-09-25 15:04:33 (Fri)
  gRtc.present = 1;
  setTime(0x33, 0x04, 0x15, 0x06, 0x25, 0x09, 0x26);   // 24 h mode
  CHECK(rtc::begin());
  CHECK(rtc::present());
  CHECK(rtc::readTime(&ep));
  CHECK(ep == refEpoch(2026, 9, 25, 9, 34, 33));
  printf("1) detect + readTime (15:04:33 IST = 09:34:33 UTC)  OK\n");

  // ---- 2) writeNow: BCD, 24 h mode, CH clear, day-of-week, round trip
  clearWriteLog();
  time_t t0 = refEpoch(2026, 9, 25, 10, 0, 0);          // UTC
  rtc_test_set_now(t0);
  rtc::writeNow();                                      // local 15:30:00 Fri
  CHECK(gRtc.reg[0] == 0x00 && gRtc.reg[1] == 0x30 && gRtc.reg[2] == 0x15);
  CHECK((gRtc.reg[2] & 0x40) == 0);                     // 24-hour mode
  CHECK(gRtc.reg[3] == 0x06);                           // Friday (1 = Sunday)
  CHECK(gRtc.reg[4] == 0x25 && gRtc.reg[5] == 0x09 && gRtc.reg[6] == 0x26);
  CHECK(rtc::readTime(&ep) && ep == t0);
  printf("2) writeNow round-trip (24 h, CH clear, Friday)  OK\n");

  // ---- 3) 20:00-23:59 - the hours the old DS1302 driver misread as 00-03
  time_t t2 = refEpoch(2026, 9, 25, 18, 29, 59);        // local 23:59:59
  rtc_test_set_now(t2);
  rtc::writeNow();
  CHECK(gRtc.reg[2] == 0x23 && gRtc.reg[1] == 0x59 && gRtc.reg[0] == 0x59);
  CHECK(rtc::readTime(&ep) && ep == t2);
  setTime(0x00, 0x15, 0x20, 0x06, 0x25, 0x09, 0x26);   // chip: 20:15:00
  CHECK(rtc::readTime(&ep) && ep == refEpoch(2026, 9, 25, 14, 45, 0));
  printf("3) late evening 23:59:59 + 20:15 read back exactly  OK\n");

  // ---- 4) date + year crossing: UTC 2026-12-31 23:30 -> local 2027-01-01 05:00
  time_t t1 = refEpoch(2026, 12, 31, 23, 30, 0);
  rtc_test_set_now(t1);
  rtc::writeNow();
  CHECK(gRtc.reg[0] == 0x00 && gRtc.reg[1] == 0x00 && gRtc.reg[2] == 0x05);
  CHECK(gRtc.reg[4] == 0x01 && gRtc.reg[5] == 0x01 && gRtc.reg[6] == 0x27);
  CHECK(gRtc.reg[3] == 0x06);                           // 2027-01-01 = Friday
  CHECK(rtc::readTime(&ep) && ep == t1);
  printf("4) date + year crossing  OK\n");

  // ---- 5) factory-fresh: CH set (oscillator halted) -> untrusted until set
  setTime(0x80 | 0x12, 0x34, 0x10, 0x03, 0x14, 0x05, 0x24);  // sane date, CH=1
  CHECK(rtc::begin());
  CHECK(rtc::present());
  CHECK(!rtc::readTime(&ep));                           // halted = stale
  CHECK(strstr(rtc::statusText(), "not set") != nullptr);
  rtc_test_set_now(t0);
  rtc::writeNow();
  CHECK((gRtc.reg[0] & 0x80) == 0);                     // CH cleared: ticking
  CHECK(rtc::readTime(&ep) && ep == t0);
  CHECK(strstr(rtc::statusText(), "DS1307 OK") != nullptr);
  printf("5) factory-fresh module (CH set)  OK  [%s]\n", rtc::statusText());

  // ---- 6) junk registers (CH clear) are rejected
  setTime(0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00);    // 2000-01-01 (reset state)
  CHECK(rtc::begin());
  CHECK(!rtc::readTime(&ep));                           // year 2000 = not a real time
  setTime(0x4F, 0x30, 0x10, 0x02, 0x10, 0x10, 0x26);    // 0x4F = not BCD
  CHECK(!rtc::readTime(&ep));
  setTime(0x10, 0x30, 0x10, 0x02, 0x10, 0x13, 0x26);    // month 13
  CHECK(!rtc::readTime(&ep));
  setTime(0x10, 0x30, 0x24, 0x02, 0x10, 0x10, 0x26);    // 24:30
  CHECK(!rtc::readTime(&ep));
  printf("6) junk / reset-state registers rejected  OK\n");

  // ---- 7) 12-hour mode on the chip: bit6 = 12 h, bit5 = PM
  setTime(0x00, 0x30, 0x40 | 0x20 | 0x03, 0x06, 0x25, 0x09, 0x26);   // 3:30 PM
  CHECK(rtc::begin());
  CHECK(rtc::readTime(&ep) && ep == refEpoch(2026, 9, 25, 10, 0, 0));
  gRtc.reg[2] = 0x40 | 0x12;                            // 12:30 AM = 00:30
  CHECK(rtc::readTime(&ep) && ep == refEpoch(2026, 9, 24, 19, 0, 0));
  gRtc.reg[2] = 0x40 | 0x20 | 0x12;                     // 12:30 PM = noon
  CHECK(rtc::readTime(&ep) && ep == refEpoch(2026, 9, 25, 7, 0, 0));
  gRtc.reg[2] = 0x40 | 0x13;                            // "13" in 12 h mode = junk
  CHECK(!rtc::readTime(&ep));
  printf("7) 12-hour mode (PM, midnight, noon, junk)  OK\n");

  // ---- 8) DS3231-style month register (bit7 = century flag) still reads
  setTime(0x05, 0x04, 0x09, 0x06, 0x25, 0x80 | 0x09, 0x26);
  CHECK(rtc::readTime(&ep) && ep == refEpoch(2026, 9, 25, 3, 34, 5));
  printf("8) DS3231 century bit masked  OK\n");

  // ---- 9) unset system clock is never pushed into the chip
  clearWriteLog();
  rtc_test_set_now(1000);
  rtc::writeNow();
  int total = 0; for (int r = 0; r < 64; r++) total += gRtc.writes[r];
  CHECK(total == 0);
  printf("9) unset system clock not written  OK\n");

  // ---- 10) RAM (0x08-0x3F) + control (0x07) never touched
  CHECK(gRtc.reg[7] == 0x10);
  for (int r = 8; r < 64; r++) CHECK(gRtc.reg[r] == (uint8_t)(0xA0 + r));
  CHECK(writesOutsideTime() == 0);
  printf("10) control register + 56-byte RAM untouched  OK\n");

  // ---- 11) no chip / 3.3 V-fed chip: NACK -> clean no-op
  gRtc.present = 0;
  CHECK(!rtc::begin());
  CHECK(!rtc::present());
  CHECK(!rtc::readTime(&ep));
  rtc_test_set_now(t0);
  rtc::writeNow();                                      // must not crash / write
  CHECK(strstr(rtc::statusText(), "no DS1307 at 0x68") != nullptr);
  printf("11) no chip  OK  [%s]\n", rtc::statusText());

  printf("\nALL RTC TESTS PASSED\n");
  return 0;
}
