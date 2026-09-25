#include "Arduino.h"
#include "control.h"
#include "rtc.h"
void rtc_test_set_now(time_t t);
#include <assert.h>
#include <cstdlib>

Settings cfg;

static time_t refEpoch(int y, int mo, int d, int h, int mi, int s) {
  struct tm t; memset(&t, 0, sizeof t);
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
  t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
  return mktime(&t);   // TZ=UTC0 set in main
}

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

int main() {
  setenv("TZ", "UTC0", 1);
  tzset();
  cfg.tzMinutes = 330;                    // IST = UTC+5:30

  // ---- 1) readTime: chip holds LOCAL 2026-09-25 15:04:33 = 09:34:33 UTC
  gChip.present = 1;
  gChip.regs[0] = 0x33; gChip.regs[1] = 0x04; gChip.regs[2] = 0x0F;  // 15:04:33
  gChip.regs[3] = 0x06; gChip.regs[4] = 0x25; gChip.regs[5] = 0x09; gChip.regs[6] = 0x26;
  gChip.regs[7] = 0x00;
  CHECK(rtc::begin());
  CHECK(rtc::present());
  time_t ep;
  CHECK(rtc::readTime(&ep));
  time_t want = refEpoch(2026, 9, 25, 9, 34, 33);
  CHECK(ep == want);
  printf("1) readTime: %ld == %ld  OK\n", (long)ep, (long)want);

  // ---- 2) writeNow round-trip, WP set on the chip (must be cleared)
  gChip.regs[7] = 0x80;                   // WP = 1 (mock enforces it!)
  time_t t0 = refEpoch(2026, 9, 25, 10, 0, 0);   // UTC
  rtc_test_set_now(t0);
  rtc::writeNow();
  // local = 15:30:00 IST, Friday(6), 25-09-2026
  { bool ok2 = (gChip.regs[0] == 0x00 && gChip.regs[1] == 0x30 && gChip.regs[2] == 0x15);
    if (!ok2) { for (int i = 0; i < 8; i++) fprintf(stderr, "reg[%d]=0x%02X ", i, gChip.regs[i]);
                fprintf(stderr, "ctrl=0x%02X\n", gChip.regs[7]); }
    CHECK(ok2); }
  CHECK(gChip.regs[3] == 0x06);           // Friday
  CHECK(gChip.regs[4] == 0x25 && gChip.regs[5] == 0x09 && gChip.regs[6] == 0x26);
  CHECK(gChip.regs[7] == 0x00);           // WP cleared
  CHECK(rtc::readTime(&ep) && ep == t0);
  printf("2) writeNow round-trip (WP cleared)  OK\n");

  // ---- 3) date crossing: UTC 2026-12-31 23:30 -> local 2027-01-01 05:00
  time_t t1 = refEpoch(2026, 12, 31, 23, 30, 0);
  rtc_test_set_now(t1);
  rtc::writeNow();
  { bool ok3 = (gChip.regs[0] == 0x00 && gChip.regs[1] == 0x00 && gChip.regs[2] == 0x05);
    if (!ok3) for (int i = 0; i < 8; i++) fprintf(stderr, "reg[%d]=0x%02X ", i, gChip.regs[i]);
    if (!ok3) fprintf(stderr, "t1=%ld\n", (long)t1);
    CHECK(ok3); }
  CHECK(gChip.regs[4] == 0x01 && gChip.regs[5] == 0x01 && gChip.regs[6] == 0x27);
  CHECK(gChip.regs[3] == 0x06);           // 2027-01-01 is also Friday
  CHECK(rtc::readTime(&ep) && ep == t1);
  printf("3) date-crossing  OK\n");

  // ---- 4) factory-fresh: CH + WP set, garbage regs -> untrusted, then set
  for (int i = 0; i < 8; i++) gChip.regs[i] = 0;
  gChip.regs[0] = 0x80;                   // CH flag (seconds bit7)
  gChip.regs[7] = 0x80;                   // WP
  CHECK(rtc::begin());
  CHECK(rtc::present());
  CHECK(!rtc::readTime(&ep));             // untrusted
  CHECK(gChip.regs[7] == 0x00);           // begin() cleared WP
  rtc_test_set_now(t0);
  rtc::writeNow();
  CHECK(gChip.regs[0] == 0x00);           // CH cleared by the burst write
  CHECK(rtc::readTime(&ep) && ep == t0);
  printf("4) factory-fresh module  OK\n");

  // ---- 5) 12-hour mode on the chip: bit7 = mode, bit5 = AM/PM
  gChip.regs[0] = 0x00; gChip.regs[1] = 0x30; gChip.regs[2] = 0xA3;  // 12h: 3:30 PM
  gChip.regs[4] = 0x25; gChip.regs[5] = 0x09; gChip.regs[6] = 0x26;
  gChip.regs[7] = 0x00;
  CHECK(rtc::begin());
  CHECK(rtc::readTime(&ep));
  time_t want12 = refEpoch(2026, 9, 25, 10, 0, 0);  // 15:30 IST = 10:00 UTC
  CHECK(ep == want12);
  gChip.regs[2] = 0x8C;                             // 12h: 12:30 AM (midnight)
  CHECK(rtc::readTime(&ep));
  CHECK(ep == refEpoch(2026, 9, 24, 19, 0, 0));     // 00:30 IST = 19:00 UTC prev day
  printf("5) 12-hour mode (PM + midnight)  OK\n");

  // ---- 6) no chip
  gChip.present = 0;
  CHECK(!rtc::begin());
  CHECK(!rtc::present());
  CHECK(!rtc::readTime(&ep));
  rtc::writeNow();                        // no-op
  printf("6) no chip  OK  [%s]\n", rtc::statusText());

  // ---- 7) RAM byte 0 must survive begin()'s probe untouched
  gChip.present = 1;
  for (int i = 0; i < 8; i++) gChip.regs[i] = 0;
  gChip.regs[0] = 0x30; gChip.regs[1] = 0x00; gChip.regs[2] = 0x00;
  gChip.regs[4] = 0x25; gChip.regs[5] = 0x09; gChip.regs[6] = 0x26;
  gChip.ram[0] = 0x5A;                            // pre-existing user data
  CHECK(rtc::begin());
  CHECK(rtc::present());
  CHECK(gChip.ram[0] == 0x5A);                    // restored after the probe
  printf("7) RAM byte preserved through probe  OK\n");

  printf("\nALL RTC TESTS PASSED\n");
  return 0;
}