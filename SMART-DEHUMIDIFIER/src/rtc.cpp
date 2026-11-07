/**
 * @file rtc.cpp
 * @brief DS1307 real-time clock driver - I2C, library-free (Wire only).
 * See rtc.h for wiring + behaviour. Uses the I2C0 bus (Wire) that
 * sensors.begin() starts: S3 SDA 8 / SCL 9, classic SDA 21 / SCL 22.
 */
#include "rtc.h"
#include "control.h"          // cfg.tzMinutes (local = UTC + offset)

#if RTC_ENABLED
#include <Wire.h>

#ifndef RTC_I2C_ADDR
#define RTC_I2C_ADDR 0x68     // DS1307 / DS3231 - fixed address
#endif

namespace rtc {

// ---- calendar math (proleptic Gregorian, no TZ env involved) ------------
// Days between 1970-01-01 and the civil date (Howard Hinnant's algorithm).
static long daysSinceEpoch(int y, int mo, int d) {
  int yy = y - (mo <= 2 ? 1 : 0);
  int era = (yy >= 0 ? yy : yy - 399) / 400;
  int yoe = yy - era * 400;                                   // [0,399]
  int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0,365]
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;            // [0,146096]
  return (long)era * 146097L + doe - 719468;
}

static time_t civilToEpoch(int y, int mo, int d, int h, int mi, int s) {
  return daysSinceEpoch(y, mo, d) * 86400L + h * 3600L + mi * 60L + s;
}

// Day-of-week register value for a civil date: 1 = Sunday .. 7 (the DS1307
// only needs the values to be sequential). 1970-01-01 was a Thursday (=5).
static uint8_t dowFromCivil(int y, int mo, int d) {
  long days = daysSinceEpoch(y, mo, d);
  return (uint8_t)(((days % 7) + 7 + 4) % 7 + 1);
}

static void epochToCivil(time_t ep, int *y, int *mo, int *d,
                         int *h, int *mi, int *s) {
  long days = ep / 86400;                       // ep is 2016+ here
  int rem = (int)(ep - days * 86400);
  *h = rem / 3600;  rem %= 3600;
  *mi = rem / 60;   *s = rem % 60;
  days += 719468;
  int era = (days >= 0 ? days : days - 146096) / 146097;
  int doe = (int)(days - era * 146097);                    // [0,146096]
  int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0,399]
  int y_ = yoe + era * 400;
  int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);       // [0,365]
  int mp = (5 * doy + 2) / 153;                            // [0,11]
  *d = doy - (153 * mp + 2) / 5 + 1;                       // [1,31]
  *mo = mp + (mp < 10 ? 3 : -9);                           // [1,12]
  *y = y_ + (*mo <= 2 ? 1 : 0);                            // Jan/Feb belong to the next yy
}

static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
static bool bcdOk(uint8_t v) { return (v & 0x0F) <= 9 && (v >> 4) <= 9; }

// ---- DS1307 registers (all BCD) -------------------------------------------
// 0x00 seconds  bit7 = CH (clock halt: 1 = oscillator stopped)
// 0x01 minutes
// 0x02 hours    bit6 = 12/24 select (1 = 12 h mode)
//               12 h: bit5 = PM, bits4-0 = 1..12 · 24 h: bits5-0 = 0..23
// 0x03 day of week 1..7 · 0x04 date 1..31 · 0x05 month 1..12 · 0x06 year 00..99
// 0x07 control (SQW/OUT) · 0x08-0x3F 56 bytes battery-backed RAM (untouched)
// A burst read from 0x00 returns one consistent snapshot: the chip copies
// its counters into a secondary buffer on the I2C START.
// (DS3231: same 0x00-0x06 layout; seconds bit7 always 0, month bit7 =
//  century flag - masked below.)

static bool readRegs(uint8_t *t) {             // the 7 time registers
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write((uint8_t)0x00);                     // register pointer
  if (Wire.endTransmission(false) != 0) return false;        // repeated START
  if (Wire.requestFrom((int)RTC_I2C_ADDR, 7) != 7) return false;
  for (uint8_t i = 0; i < 7; i++) t[i] = (uint8_t)Wire.read();
  return true;
}

static bool writeRegs(const uint8_t *t) {
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write((uint8_t)0x00);
  for (uint8_t i = 0; i < 7; i++) Wire.write(t[i]);
  return Wire.endTransmission() == 0;
}

// hours register -> 0..23. In 24 h mode bit5 is the "20s" digit: masking
// with 0x1F (as the old DS1302 driver did) turns 20:00-23:59 into 00-03.
static int hour24(uint8_t hr) {
  if (hr & 0x40) {                               // 12-hour mode
    int h = bcd2bin(hr & 0x1F);                  // 1..12
    if (hr & 0x20) return (h == 12) ? 12 : h + 12;   // PM
    return (h == 12) ? 0 : h;                          // AM (12 AM = 00)
  }
  return bcd2bin(hr & 0x3F);                     // 24-hour mode
}

// sane = a time a real module would hold (year 2019-2099 keeps this
// forgiving for modules that arrive with a seller-set date)
static bool sane(const uint8_t *t) {
  uint8_t sec = t[0] & 0x7F, min = t[1] & 0x7F, mon = t[5] & 0x1F;
  uint8_t hr = (t[2] & 0x40) ? (t[2] & 0x1F) : (t[2] & 0x3F);
  if (!bcdOk(sec) || !bcdOk(min) || !bcdOk(hr) || !bcdOk(t[4]) ||
      !bcdOk(mon) || !bcdOk(t[6])) return false;
  bool hourOk = (t[2] & 0x40) ? (bcd2bin(hr) >= 1 && bcd2bin(hr) <= 12)
                              : (bcd2bin(hr) <= 23);
  return bcd2bin(sec) < 60 && bcd2bin(min) < 60 && hourOk
      && bcd2bin(t[4]) >= 1 && bcd2bin(t[4]) <= 31
      && bcd2bin(mon) >= 1 && bcd2bin(mon) <= 12
      && bcd2bin(t[6]) >= 19 && bcd2bin(t[6]) <= 99;
}

static bool sPresent = false;
static bool sTrusted = false;

bool begin() {
  sPresent = false;
  sTrusted = false;
  Wire.beginTransmission(RTC_I2C_ADDR);
  if (Wire.endTransmission() != 0) return false;     // nobody at 0x68
  uint8_t t[7];
  if (!readRegs(t)) return false;
  sPresent = true;
  // CH set = oscillator halted: factory-fresh modules (and dead coin cells)
  // come up this way, often with junk registers - untrusted until the first
  // real set (site sync, keypad menu or 'rtcset').
  sTrusted = !(t[0] & 0x80) && sane(t);
  return true;
}

bool present() { return sPresent; }

bool readTime(time_t *outEp) {
  if (!sPresent) return false;
  uint8_t t[7];
  if (!readRegs(t) || (t[0] & 0x80) || !sane(t)) return false;  // halted/junk
  time_t ep = civilToEpoch(2000 + bcd2bin(t[6]), bcd2bin(t[5] & 0x1F),
                           bcd2bin(t[4]), hour24(t[2]),
                           bcd2bin(t[1] & 0x7F), bcd2bin(t[0] & 0x7F));
  ep -= (time_t)cfg.tzMinutes * 60;              // chip holds LOCAL time
  *outEp = ep;
  return true;
}

void writeNow() {
  if (!sPresent) return;
  time_t ep = time(nullptr);
  if (ep <= (time_t)1700000000) return;          // don't push an unset clock
  int y, mo, d, h, mi, s;
  epochToCivil(ep + (time_t)cfg.tzMinutes * 60, &y, &mo, &d, &h, &mi, &s);
  uint8_t t[7] = {
    bin2bcd((uint8_t)s),                 // bit7 CH = 0 -> oscillator runs
    bin2bcd((uint8_t)mi),
    bin2bcd((uint8_t)h),                 // bit6 = 0 -> 24-hour mode
    dowFromCivil(y, mo, d),              // 1..7 (from the LOCAL civil date)
    bin2bcd((uint8_t)d), bin2bcd((uint8_t)mo),
    bin2bcd((uint8_t)((y >= 2000 ? y - 2000 : y) % 100)),
  };
  if (writeRegs(t)) sTrusted = true;
}

const char *statusText() {
  static char b[80];
  if (!sPresent)
    snprintf(b, sizeof(b), "no DS1307 at 0x%02X (SDA %d / SCL %d, VCC 5 V) - clock = phone sync + NVS",
             (unsigned)RTC_I2C_ADDR, (int)PIN_I2C0_SDA, (int)PIN_I2C0_SCL);
  else if (sTrusted) {
    time_t ep;
    if (readTime(&ep)) {
      time_t local = ep + (time_t)cfg.tzMinutes * 60;
      int y, mo, d, h, mi, s;
      epochToCivil(local, &y, &mo, &d, &h, &mi, &s);
      snprintf(b, sizeof(b), "DS1307 OK %04d-%02d-%02d %02d:%02d:%02d (coin-cell)",
               y, mo, d, h, mi, s);
    } else snprintf(b, sizeof(b), "DS1307 found but its registers read garbage");
  } else
    snprintf(b, sizeof(b), "DS1307 found, clock not set yet (open the site or type 'rtcset')");
  return b;
}

}  // namespace rtc

#else   // !RTC_ENABLED - no RTC on this build

// Same API as no-ops - the contract rtc.h documents - so callers such as
// the serial console's `rtc` / `rtcset` commands need no #if of their own
// (they failed to link on the classic build before v2.0.22).
namespace rtc {
bool begin() { return false; }
bool present() { return false; }
bool readTime(time_t *outEp) { (void)outEp; return false; }
void writeNow() {}
const char *statusText() { return "no RTC on this build (RTC_ENABLED 0 - clock = phone sync + NVS)"; }
}  // namespace rtc

#endif  // RTC_ENABLED
