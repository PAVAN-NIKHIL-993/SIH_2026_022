#include "eelog.h"

#if ELOG_ENABLED
#include <Wire.h>

namespace eelog {

// ---- chip geometry ------------------------------------------------------
static const uint16_t CHIP_B  = 32768;      // AT24C256
static const uint16_t PAGE    = 64;
static const uint16_t R_SIZE  = 40;
static const uint16_t R_BASE  = 64;         // page 0 = header
static const uint16_t R_MAX   = (CHIP_B - R_BASE) / R_SIZE;   // 817
static const uint32_t MAGIC   = 0x454C4F47; // "GOLE" LE = "ELOG"

struct Hdr {
  uint32_t magic;
  uint16_t ver;
  uint16_t slots;
  uint16_t count;
  uint16_t head;       // next write index
  uint32_t seq;        // last sequence number handed out
};

static Hdr   s_h = {};
static bool  s_ok = false;

// ---- low-level: random/sequential reads + page-safe writes -------------
static bool rd(uint16_t a, uint8_t *b, uint16_t n) {
  while (n) {
    uint8_t chunk = n > 28 ? 28 : n;
    Wire.beginTransmission(ELOG_ADDR);
    Wire.write((uint8_t)(a >> 8));  Wire.write((uint8_t)a);
    if (Wire.endTransmission(false) != 0) return false;   // rep start
    if (Wire.requestFrom((int)ELOG_ADDR, (int)chunk) != chunk) return false;
    for (uint8_t i = 0; i < chunk; i++) b[i] = (uint8_t)Wire.read();
    a += chunk;  b += chunk;  n -= chunk;
  }
  return true;
}

static bool wrPage(uint16_t a, const uint8_t *b, uint16_t n) {
  // n never crosses a page boundary (callers guarantee it)
  Wire.beginTransmission(ELOG_ADDR);
  Wire.write((uint8_t)(a >> 8));  Wire.write((uint8_t)a);
  for (uint16_t i = 0; i < n; i++) Wire.write(b[i]);
  if (Wire.endTransmission() != 0) return false;
  delay(5);                                // AT24 twr write cycle
  return true;
}

static bool wr(uint16_t a, const uint8_t *b, uint16_t n) {
  while (n) {
    uint16_t inPage = PAGE - (a % PAGE);
    uint16_t chunk = n < inPage ? n : inPage;
    if (!wrPage(a, b, chunk)) return false;
    a += chunk;  b += chunk;  n -= chunk;
  }
  return true;
}

// ---- v2.0.23: is it REALLY 32 kB? ----------------------------------------
// DS1307 "Tiny RTC" boards carry an AT24C32 (4 kB, 32-byte pages) at the
// same 0x50. Taken for an AT24C256 it would wrap addresses and page writes
// and corrupt its own header. A smaller 24Cxx mirrors high addresses onto
// low ones: write a marker to a spare byte above the last record (0x7FF0 -
// never used by the layout) and see whether it shows up at the 4/8/16 kB
// alias. The original byte is put back either way.
static bool isFull32k() {
  const uint16_t HI = 0x7FF0;
  const uint16_t LO[3] = {0x0FF0, 0x1FF0, 0x3FF0};  // aliases on 4/8/16 kB
  uint8_t hi0, lo0[3];
  if (!rd(HI, &hi0, 1)) return false;
  for (uint8_t i = 0; i < 3; i++) if (!rd(LO[i], &lo0[i], 1)) return false;
  uint8_t m = 0x5A;                             // marker unlike every byte now
  while (m == hi0 || m == lo0[0] || m == lo0[1] || m == lo0[2]) m++;
  if (!wr(HI, &m, 1)) return false;
  bool aliased = false;
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t v = 0;
    if (!rd(LO[i], &v, 1) || v == m) aliased = true;
  }
  wr(HI, &hi0, 1);                              // give the byte back
  return !aliased;
}

// ---- record pack/unpack (explicit, endian/padding-safe) ----------------
static uint8_t crc8(const uint8_t *b, uint16_t n) {
  uint8_t c = 0x5A;
  while (n--) { c ^= *b++; for (uint8_t i = 0; i < 8; i++) c = (c & 0x80) ? (c << 1) ^ 0x07 : (c << 1); }
  return c;
}

static void putU32(uint8_t *p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
static uint32_t getU32(const uint8_t *p)   { return p[0] | (p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static void putU16(uint8_t *p, uint16_t v) { p[0]=v; p[1]=v>>8; }
static uint16_t getU16(const uint8_t *p)   { return p[0] | (p[1]<<8); }

static void pack(const EeRec &r, uint8_t *b) {
  memset(b, 0, R_SIZE);
  putU32(b + 0,  r.seq);        putU32(b + 4,  r.startEpoch);
  putU32(b + 8,  r.durS);
  b[12]=r.mode; b[13]=r.endR; b[14]=r.ecode; b[15]=r.flags;
  putU16(b + 16, (uint16_t)r.setT10);  putU16(b + 18, (uint16_t)r.tAvg10);
  putU16(b + 20, (uint16_t)r.tMax10);  putU16(b + 22, (uint16_t)r.hMax10);
  putU16(b + 24, (uint16_t)r.outT10);  putU16(b + 26, (uint16_t)r.wtS10);
  putU16(b + 28, (uint16_t)r.wtE10);   putU16(b + 30, (uint16_t)r.wtT10);
  putU16(b + 32, (uint16_t)r.vbS10);   putU16(b + 34, (uint16_t)r.vbE10);
  b[38] = crc8(b, 38);
}

static bool unpack(EeRec &r, const uint8_t *b) {
  if (b[38] != crc8(b, 38)) return false;   // torn/garbage record
  r.seq = getU32(b + 0);   r.startEpoch = getU32(b + 4);
  r.durS = getU32(b + 8);
  r.mode=b[12]; r.endR=b[13]; r.ecode=b[14]; r.flags=b[15];
  r.setT10=(int16_t)getU16(b+16); r.tAvg10=(int16_t)getU16(b+18);
  r.tMax10=(int16_t)getU16(b+20); r.hMax10=(int16_t)getU16(b+22);
  r.outT10=(int16_t)getU16(b+24); r.wtS10=(int16_t)getU16(b+26);
  r.wtE10=(int16_t)getU16(b+28);  r.wtT10=(int16_t)getU16(b+30);
  r.vbS10=(int16_t)getU16(b+32);  r.vbE10=(int16_t)getU16(b+34);
  return true;
}

static bool writeHdr() {
  uint8_t b[16];
  putU32(b + 0, s_h.magic);  putU16(b + 4, s_h.ver);  putU16(b + 6, s_h.slots);
  putU16(b + 8, s_h.count);  putU16(b + 10, s_h.head);
  putU32(b + 12, s_h.seq);
  return wr(0, b, 16);
}

// ---- public API ---------------------------------------------------------
void begin() {
  Wire.beginTransmission(ELOG_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println(F("[eelog] AT24C256 not found - long-term registry off"));
    return;
  }
  if (!isFull32k()) {
    Serial.println(F("[eelog] the EEPROM at 0x50 is smaller than 32 kB (the AT24C32 on a "
                     "DS1307 board?) - long-term registry off; fit an AT24C256"));
    return;
  }
  uint8_t b[16];
  if (!rd(0, b, 16)) return;
  bool fresh = (getU32(b + 0) != MAGIC);
  if (!fresh) {
    s_h.magic = getU32(b + 0);  s_h.ver   = getU16(b + 4);
    s_h.slots = getU16(b + 6);  s_h.count = getU16(b + 8);
    s_h.head  = getU16(b + 10); s_h.seq   = getU32(b + 12);
    if (s_h.slots != R_MAX || s_h.count > R_MAX || s_h.head >= R_MAX)
      fresh = true;                              // junk header -> reformat
  }
  if (fresh) {
    s_h = { MAGIC, 1, R_MAX, 0, 0, 0 };
    if (!writeHdr()) return;
    Serial.println(F("[eelog] AT24C256 32 kB: registry formatted (817 slots)"));
  } else {
    // torn-write rollback: validate the record HEAD points at
    uint8_t rb[R_SIZE];
    EeRec tmp;
    if (s_h.count > 0) {
      uint16_t last = (uint16_t)((s_h.head + R_MAX - 1) % R_MAX);
      if (rd(R_BASE + (uint16_t)last * R_SIZE, rb, R_SIZE) &&
          !unpack(tmp, rb)) {
        // last record is corrupt -> drop it
        s_h.head = last;
        if (s_h.count > 0) s_h.count--;
        if (s_h.seq > 0) s_h.seq--;
        writeHdr();
      }
    }
    Serial.printf("[eelog] AT24C256 32 kB cycle registry: %u/%u summaries "
                  "(seq %u)\n", s_h.count, R_MAX, (unsigned)s_h.seq);
  }
  s_ok = true;
}

bool ok()                { return s_ok; }
uint16_t count()         { return s_h.count; }
uint16_t slots()         { return R_MAX; }

void append(EeRec &r) {
  if (!s_ok) return;
  r.seq = ++s_h.seq;
  uint8_t b[R_SIZE];
  pack(r, b);
  if (!wr(R_BASE + s_h.head * R_SIZE, b, R_SIZE)) return;
  s_h.head = (s_h.head + 1) % R_MAX;
  if (s_h.count < R_MAX) s_h.count++;
  writeHdr();
}

bool get(uint16_t i, EeRec &r) {
  if (!s_ok || i >= s_h.count) return false;
  uint16_t idx = (uint16_t)((s_h.head + R_MAX - s_h.count + i) % R_MAX);
  uint8_t b[R_SIZE];
  if (!rd(R_BASE + idx * R_SIZE, b, R_SIZE)) return false;
  return unpack(r, b);
}

void clear() {
  if (!s_ok) return;
  s_h.count = 0;  s_h.head = 0;  s_h.seq = 0;
  writeHdr();
}

}  // namespace eelog

#endif
