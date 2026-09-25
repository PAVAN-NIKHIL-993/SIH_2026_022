/**
 * @file rtc.cpp
 * @brief DS1302 real-time clock driver - 3-wire bit-bang, library-free.
 * See rtc.h for wiring + behaviour. Pins come from the variant config
 * (S3: RST 40 / SCLK 42 / I-O 47; classic: -1 = not fitted).
 */
#include "rtc.h"
#include "control.h"          // cfg.tzMinutes (local = UTC + offset)

#if RTC_ENABLED

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

// DS1302 day-of-week register value for a civil date: 1 = Sunday .. 7.
// 1970-01-01 was a Thursday (=5), hence the +4.
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

// ---- DS1302 bit-bang ------------------------------------------------------
// Frame: RST low -> high (chip select), START bit 0, 8-bit command byte
// LSB-first, data bytes (in read mode the chip drives I/O, updating on
// each SCLK falling edge), STOP bit 1, RST low.
// Command byte: bit7=1, bit6=0 (clock data), bits5-1 = register,
// bit0 = R/W (0 write / 1 read). Time: 0x80 write / 0x81 read; control
// register: 0x8E write / 0x8F read; scratch RAM byte 0: 0xC0 (write) /
// 0xC1 (read); registers auto-increment in burst.
// Time registers 0-6 BCD: sec min hr dow date month year. CH (halt)
// flag = bit7 of the SECONDS register; WP (write-protect) = bit7 of the
// control register, power-on state UNDEFINED - always cleared before a
// write (datasheet requirement).

static inline void ceHiLo(bool hi) { digitalWrite(PIN_RTC_RST, hi ? HIGH : LOW); }
static inline void clkHiLo(bool hi) { digitalWrite(PIN_RTC_SCLK, hi ? HIGH : LOW); }

static void rtcBitWrite(bool b) {
  digitalWrite(PIN_RTC_IO, b ? HIGH : LOW);
  clkHiLo(true);
  clkHiLo(false);
}

static bool rtcBitRead() {
  clkHiLo(true);
  clkHiLo(false);                              // falling edge: chip updates
  return (digitalRead(PIN_RTC_IO) == HIGH);    // bit, valid during the low
}                                              // phase until the next fall

static void byteWrite(uint8_t v) {
  pinMode(PIN_RTC_IO, OUTPUT);
  for (int i = 0; i < 8; i++) rtcBitWrite((v >> i) & 1);
}

static uint8_t byteRead() {
  pinMode(PIN_RTC_IO, INPUT);
  uint8_t v = 0;
  for (int i = 0; i < 8; i++) if (rtcBitRead()) v |= (uint8_t)(1u << i);
  return v;
}

static void xfer(uint8_t addr, const uint8_t *data, uint8_t n, uint8_t *out) {
  ceHiLo(false);
  ceHiLo(true);                                   // chip select (CE = RST)
  rtcBitWrite(false);                                // START
  byteWrite(addr);
  if (addr & 0x01) {                              // R/W bit (bit0): 1 = read
    for (uint8_t i = 0; i < n; i++) out[i] = byteRead();
  } else {
    for (uint8_t i = 0; i < n; i++) byteWrite(data[i]);
  }
  rtcBitWrite(true);                                 // STOP
  ceHiLo(false);
}

// sane = a time a real module would hold (year 2019-2099 keeps this
// forgiving for modules that arrive with a seller-set date)
static bool sane(const uint8_t *t) {
  return bcd2bin(t[0]) < 60 && bcd2bin(t[1]) < 60
      && bcd2bin(t[2] & 0x1F) <= 23                      // bits7-5 = mode/PM
      && bcd2bin(t[4]) >= 1 && bcd2bin(t[4]) <= 31
      && bcd2bin(t[5]) >= 1 && bcd2bin(t[5]) <= 12
      && bcd2bin(t[6]) >= 19 && bcd2bin(t[6]) <= 99;
}

static bool sPresent = false;
static bool sTrusted = false;

bool begin() {
  sPresent = false;
  sTrusted = false;
  if (PIN_RTC_RST < 0 || PIN_RTC_SCLK < 0 || PIN_RTC_IO < 0) return false;
  pinMode(PIN_RTC_RST, OUTPUT);  ceHiLo(false);
  pinMode(PIN_RTC_SCLK, OUTPUT); clkHiLo(false);
  pinMode(PIN_RTC_IO, OUTPUT);   digitalWrite(PIN_RTC_IO, LOW);

  {
    uint8_t zero = 0x00;
    xfer(0x8E, &zero, 1, nullptr);               // clear WP (power-on state
  }                                              // is undefined per datasheet)
  // Deterministic presence probe on scratch RAM byte 0: read it, write a
  // magic value, read it back, restore it. A dangling bus echoes nothing,
  // so only a real chip can return the magic.
  uint8_t orig = 0, magic = 0xA5, back = 0;
  xfer(0xC1, nullptr, 1, &orig);
  xfer(0xC0, &magic, 1, nullptr);
  xfer(0xC1, nullptr, 1, &back);
  xfer(0xC0, &orig, 1, nullptr);                 // give the byte back
  sPresent = (back == magic);
  if (!sPresent) return false;
  {
    uint8_t a[7];
    xfer(0x81, nullptr, 7, a);
    // CH flag (bit7 of seconds) set = clock halted: factory-fresh modules
    // ship this way with garbage registers, so the time is untrusted until
    // the first real set (site sync or 'rtcset').
    sTrusted = sane(a) && !(a[0] & 0x80);
  }
  return true;
}

bool present() { return sPresent; }

bool readTime(time_t *outEp) {
  if (!sPresent) return false;
  uint8_t t[7];
  xfer(0x81, nullptr, 7, t);
  if (!sane(t)) return false;
  int year = 2000 + bcd2bin(t[6]);
  // Hours register: bit7 = 12 h mode select, bit5 = AM/PM (12 h mode),
  // bits4-0 = hour BCD (0-23 in 24 h mode, 1-12 in 12 h mode).
  int hour;
  if (t[2] & 0x80) {                              // 12-hour mode
    hour = bcd2bin(t[2] & 0x1F);                  // 1-12
    if (t[2] & 0x20) hour = (hour == 12) ? 12 : hour + 12;   // PM
    else            hour = (hour == 12) ? 0  : hour;         // AM
  } else                                          // 24-hour mode
    hour = bcd2bin(t[2] & 0x1F);
  time_t ep = civilToEpoch(year, bcd2bin(t[5]), bcd2bin(t[4]),
                           hour, bcd2bin(t[1]), bcd2bin(t[0]));
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
  uint8_t dow = dowFromCivil(y, mo, d);    // from the LOCAL civil date
  uint8_t t[7] = {
    bin2bcd((uint8_t)s), bin2bcd((uint8_t)mi),
    bin2bcd((uint8_t)(h & 0x1F)),        // 24 h (bit5 = 12-h flag = 0)
    bin2bcd(dow), bin2bcd((uint8_t)d), bin2bcd((uint8_t)mo),
    bin2bcd((uint8_t)((y >= 2000 ? y - 2000 : y) % 100)),
  };
  {
    uint8_t zero = 0x00;
    xfer(0x8E, &zero, 1, nullptr);               // clear WP before the write
    xfer(0x80, t, 7, nullptr);                   // burst; seconds bit7=0
  }                                              // clears CH: now ticking
  sTrusted = true;
}

const char *statusText() {
  static char b[64];
  if (!sPresent)
    snprintf(b, sizeof(b), "no DS1302 (clock = phone sync + NVS)");
  else if (sTrusted) {
    time_t ep;
    if (readTime(&ep)) {
      time_t local = ep + (time_t)cfg.tzMinutes * 60;
      int y, mo, d, h, mi, s;
      epochToCivil(local, &y, &mo, &d, &h, &mi, &s);
      snprintf(b, sizeof(b), "DS1302 OK %04d-%02d-%02d %02d:%02d:%02d (coin-cell)",
               y, mo, d, h, mi, s);
    } else snprintf(b, sizeof(b), "DS1302 OK but registers read garbage");
  } else
    snprintf(b, sizeof(b), "DS1302 present, no valid time yet (open site or 'rtcset')");
  return b;
}

}  // namespace rtc

#else   // !RTC_ENABLED - no DS1302 on this build (classic variant)

// Same API as no-ops - the contract rtc.h documents - so callers such as
// the serial console's `rtc` / `rtcset` commands need no #if of their own
// (they failed to link on the classic build before v2.0.22).
namespace rtc {
bool begin() { return false; }
bool present() { return false; }
bool readTime(time_t *outEp) { (void)outEp; return false; }
void writeNow() {}
const char *statusText() { return "no DS1302 on this build (clock = phone sync + NVS)"; }
}  // namespace rtc

#endif  // RTC_ENABLED
