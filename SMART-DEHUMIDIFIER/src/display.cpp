#include "display.h"

#if DISPLAY_ENABLED
#include <SPI.h>
#include "config.h"
#include "control.h"
#include "sensors.h"
#include "battery.h"
#include "door.h"
#include "scale.h"
#include "supply.h"
#include "dht.h"
#include "menu.h"

// ---------------------------------------------------------------------
//  ILI9488 480x320 SPI driver - no libraries, direct register commands
// ---------------------------------------------------------------------
namespace display {

static SPIClass spi(HSPI);                 // remapped pins, see config.h

// RGB565 palette (matches the website theme)
#define C_NAVY   0x0042
#define C_NAVY2  0x0145    // title bar
#define C_CARD   0x0142
#define C_GOLD   0xD566
#define C_BLUE   0x4E1F
#define C_GREEN  0x3693
#define C_RED    0xFB8E
#define C_WHITE  0xFFFF
#define C_DIM    0xADB9
#define C_BLACK  0x0000

static inline void cs(bool low) { digitalWrite(PIN_TFT_CS, low ? LOW : HIGH); }

static void cmd(uint8_t c) {
  cs(true); digitalWrite(PIN_TFT_DC, LOW);
  spi.transfer(c);
  cs(false);
}
static void data8(uint8_t d) {
  cs(true); digitalWrite(PIN_TFT_DC, HIGH);
  spi.transfer(d);
  cs(false);
}
static void cmdData(uint8_t c, uint8_t d) { cmd(c); data8(d); }

// 5x7 font (printable ASCII 0x20-0x7E), column bits, MSB = top row
static const uint8_t FONT5x7[95][5] PROGMEM = {
  {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
  {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
  {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
  {0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
  {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
  {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
  {0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
  {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
  {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
  {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
  {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
  {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},
  {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
  {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
  {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
  {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
  {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
  {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0x7C,0x14,0x14,0x14,0x08},
  {0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
  {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
  {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
  {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
  {0x00,0x41,0x36,0x08,0x00},{0x08,0x08,0x2A,0x1C,0x08}
};

static void setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  cmd(0x2A);                               // CASET
  data8(x >> 8); data8(x & 0xFF);
  data8((x + w - 1) >> 8); data8((x + w - 1) & 0xFF);
  cmd(0x2B);                               // PASET
  data8(y >> 8); data8(y & 0xFF);
  data8((y + h - 1) >> 8); data8((y + h - 1) & 0xFF);
  cmd(0x2C);                               // RAMWR
}

static void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c) {
  setWindow(x, y, w, h);
  cs(true); digitalWrite(PIN_TFT_DC, HIGH);
  uint32_t n = (uint32_t)w * h;
  while (n--) spi.transfer16(c);
  cs(false);
}

static void drawChar(uint16_t x, uint16_t y, char ch, uint8_t s, uint16_t fg, uint16_t bg) {
  if (ch < 0x20 || ch > 0x7E) ch = '?';
  const uint8_t *g = FONT5x7[(uint8_t)ch - 0x20];
  setWindow(x, y, (uint16_t)(5 * s), (uint16_t)(7 * s));
  cs(true); digitalWrite(PIN_TFT_DC, HIGH);
  for (uint8_t r = 0; r < 7; r++) {
    uint8_t bits = 0;
    for (uint8_t c = 0; c < 5; c++) if (pgm_read_byte(&g[c]) & (0x40 >> r)) bits |= 1 << c;
    for (uint8_t vs = 0; vs < s; vs++)
      for (uint8_t c = 0; c < 5; c++)
        for (uint8_t hs = 0; hs < s; hs++) spi.transfer16((bits & (1 << c)) ? fg : bg);
  }
  cs(false);
}

static void drawText(uint16_t x, uint16_t y, const char *str, uint8_t s, uint16_t fg, uint16_t bg) {
  while (*str) { drawChar(x, y, *str++, s, fg, bg); x += (uint16_t)(6 * s); }
}

static void bar(uint16_t x, uint16_t y, uint16_t w, uint8_t h, uint8_t pct, uint16_t c) {
  fillRect(x, y, w, h, C_CARD);
  fillRect(x + 1, y + 1, (uint16_t)((w - 2) * pct / 100), (uint8_t)(h - 2), c);
}

void begin() {
  pinMode(PIN_TFT_CS, OUTPUT);  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_TFT_DC, OUTPUT);
  pinMode(PIN_TFT_RST, OUTPUT); digitalWrite(PIN_TFT_RST, HIGH);
  delay(50);
  spi.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, -1);
  spi.beginTransaction(SPISettings(TFT_SPI_HZ, MSBFIRST, SPI_MODE0));

  digitalWrite(PIN_TFT_RST, LOW); delay(20);   // hardware reset
  digitalWrite(PIN_TFT_RST, HIGH); delay(150);
  cmd(0x01); delay(150);                       // SWRESET
  cmd(0x11); delay(50);                        // SLPOUT
  cmdData(0x3A, 0x55); delay(10);              // 16-bit colour
  cmdData(0x36, 0x48);                         // landscape + BGR
  cmd(0x21);                                   // INVON (typical 3.5" boards)
  cmd(0x29); delay(20);                        // DISPON
  drawSplash();                     // v2.0: boot = ARCHITECTS OF SOLUTIONS
  Serial.printf("[disp] ILI9488 480x320 up (SCK%d MOSI%d CS%d DC%d RST%d)\n",
                PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
}

// ---------------------------------------------------------------------
//  v2.0 screens: splash / menu / status - 1 Hz
// ---------------------------------------------------------------------
static bool sSplash = true;            // boot starts on the splash

void splash(bool on) { sSplash = on; }

static void fmtC(char *dst, size_t n, float v) {   // "--.-C" when NaN
  if (isnan(v)) snprintf(dst, n, "--.-C");
  else          snprintf(dst, n, "%.1fC", v);
}

// ---- boot splash: the brand, held for the 10 s initialisation window ----
static void drawSplash() {
  fillRect(0, 0, 480, 320, C_NAVY);
  drawText(8, 84, BRAND_NAME, 3, C_GOLD, C_NAVY);      // ARCHITECTS OF
  drawText(64, 140, "S O L U T I O N S", 3, C_GOLD, C_NAVY);
  fillRect(60, 184, 360, 2, C_DIM);
  drawText(96, 204, PRODUCT_NAME, 4, C_WHITE, C_NAVY); // SMART DEHUMIDIFIER
  char b[48];
  snprintf(b, sizeof(b), "v%s  -  initialising, outputs LOW", FW_VERSION);
  drawText(84, 262, b, 2, C_DIM, C_NAVY);
}

// ---- on-device menu (keypad-driven, see menu.cpp) -----------------------
static void drawMenu() {
  fillRect(0, 0, 480, 320, C_NAVY);
  fillRect(0, 0, 480, 34, C_NAVY2);
  drawText(8, 10, menu::editing() ? "ENTER VALUE" : "MENU", 2, C_GOLD, C_NAVY2);
  drawText(300, 10, "2/8 move  A ok  # back", 2, C_DIM, C_NAVY2);

  if (menu::info()) {                       // the OTA firmware-update screen
    drawText(16, 56, "FIRMWARE UPDATE", 4, C_GOLD, C_NAVY);
    drawText(16, 108, "phone/PC -> WiFi:", 2, C_WHITE, C_NAVY);
    drawText(16, 130, "  AgarbattiDryer / dryer1234", 2, C_WHITE, C_NAVY);
    drawText(16, 162, "browser (phone):", 2, C_WHITE, C_NAVY);
    drawText(16, 184, "  http://192.168.4.1", 2, C_GOLD, C_NAVY);
    drawText(16, 206, "  menu: Firmware update,", 2, C_WHITE, C_NAVY);
    drawText(16, 228, "  pick the .bin, Update", 2, C_WHITE, C_NAVY);
    drawText(16, 260, "Arduino IDE (PC):", 2, C_WHITE, C_NAVY);
    drawText(16, 282, "  Port -> SMART-DEHUMIDIFIER", 2, C_GOLD, C_NAVY);
    drawText(16, 302, "  at 192.168.4.1", 2, C_GOLD, C_NAVY);
    drawText(400, 302, "# back", 1, C_DIM, C_NAVY);
    return;
  }
  if (menu::editing()) {
    drawText(16, 80, menu::itemName(menu::cursor()), 3, C_WHITE, C_NAVY);
    char b[32];
    snprintf(b, sizeof(b), "now: %s", menu::itemValue(menu::cursor()));
    drawText(16, 130, b, 3, C_GOLD, C_NAVY);
    snprintf(b, sizeof(b), "new: %s_", menu::editBuffer());
    drawText(16, 180, b, 4, C_GREEN, C_NAVY);
    drawText(16, 250, menu::editHint(), 2, C_DIM, C_NAVY);
    drawText(16, 276, "type digits, A = save, # = cancel", 2, C_DIM, C_NAVY);
    return;
  }
  uint16_t y = 60;
  for (uint8_t i = 0; i < menu::itemCount(); i++) {
    bool sel = (i == menu::cursor());
    if (sel) fillRect(0, y - 6, 480, 30, C_CARD);
    drawText(24, y, menu::itemName(i), 3, sel ? C_GOLD : C_WHITE,
             sel ? C_CARD : C_NAVY);
    drawText(280, y, menu::itemValue(i), 3, sel ? C_GOLD : C_DIM,
             sel ? C_CARD : C_NAVY);
    y += 40;
  }
}

// ---- compact temperature spark-line (cycle log) ---------------------------
static void drawGraph(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  fillRect(x, y, w, h, C_CARD);
  uint16_t n = dryer.logCount();
  if (n < 2) { drawText(x + 4, y + h / 2 - 7, "graph: start a cycle", 1, C_DIM, C_CARD); return; }
  int16_t lo = 3000, hi = -3000;
  for (uint16_t i = 0; i < n; i++) {          // scale to the observed range
    int16_t v = dryer.logAt(i).tAvg10;
    if (v < lo) lo = v;  if (v > hi) hi = v;
  }
  if (hi - lo < 20) { lo -= 10; hi += 10; }   // avoid a flat zero-range
  uint16_t step = n > w ? n / w : 1;
  for (uint16_t px = 0; px < w && px * step < n; px++) {
    int16_t v = dryer.logAt(px * step).tAvg10;
    uint16_t yy = y + h - 2 - (uint16_t)((long)(v - lo) * (h - 4) / (hi - lo));
    if (yy < y + 1) yy = y + 1;
    if (yy > y + h - 2) yy = y + h - 2;
    fillRect(x + px, yy, 1, 2, C_GOLD);       // 2 px thick point
  }
}

void update() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (last != 0 && now - last < 1000) return;
  last = now ? now : 1;

  if (sSplash)  { drawSplash(); return; }
  if (menu::active()) { drawMenu(); return; }

  char buf[52], b1[10], b2[10];
  float t1 = sensors.t1(), t2 = sensors.t2();
  float tA = sensors.tAvg(), hA = sensors.hAvg();
  DState st = dryer.state();

  fillRect(0, 0, 480, 320, C_NAVY);

  // title bar: product + supply mode + parameter mode
  fillRect(0, 0, 480, 34, C_NAVY2);
  snprintf(buf, sizeof(buf), "%s v%s", PRODUCT_NAME, FW_VERSION);
  drawText(8, 10, buf, 2, C_GOLD, C_NAVY2);
  drawText(230, 10, supply::modeName(), 2,
           supply::optoLive() ? C_GREEN : C_RED, C_NAVY2);
  snprintf(buf, sizeof(buf), "%s",
           cfg.mode == 0 ? "AGARBATTI" : cfg.mode == 2 ? "SILICAGEL" : "USER");
  drawText(400, 10, buf, 2, C_WHITE, C_NAVY2);

  // state + times (elapsed since start, remaining of the set time)
  uint16_t stCol = (st == DState::FAULT) ? C_RED :
                   (st == DState::RUNNING) ? C_GREEN :
                   (st == DState::DONE) ? C_BLUE : C_DIM;
  drawText(16, 48, stateName(st), 4, stCol, C_NAVY);
  if (st == DState::RUNNING || st == DState::COOLDOWN) {
    uint32_t el = dryer.elapsedS(), rem = dryer.remainingS();
    snprintf(buf, sizeof(buf), "ELAPSED %lu:%02lu:%02lu  LEFT %lu:%02lu",
             (unsigned long)(el / 3600), (unsigned long)((el / 60) % 60),
             (unsigned long)(el % 60),
             (unsigned long)(rem / 60), (unsigned long)(rem % 60));
    drawText(16, 92, buf, 2, C_WHITE, C_NAVY);
    if (dryer.boosting()) drawText(400, 48, "MAX", 3, C_GOLD, C_NAVY);
  }

  // left column: climate + external temperature
  uint16_t y = 124;
  drawText(16, y, "T1", 2, C_DIM, C_NAVY);
  fmtC(b1, sizeof(b1), t1);  drawText(52, y, b1, 2, C_GOLD, C_NAVY);
  drawText(16, y + 26, "T2", 2, C_DIM, C_NAVY);
  fmtC(b2, sizeof(b2), t2);  drawText(52, y + 26, b2, 2, C_GOLD, C_NAVY);
  if (isnan(tA)) snprintf(buf, sizeof(buf), "AVG --.-C  RH --%%");
  else           snprintf(buf, sizeof(buf), "AVG %.1fC  RH %.0f%%",
                         tA, isnan(hA) ? 0.0f : hA);
  drawText(16, y + 52, buf, 2, C_WHITE, C_NAVY);
  snprintf(buf, sizeof(buf), "SET %.0fC  RH>%.0f%% FAN 1min", cfg.setTemp, cfg.humHigh);
  drawText(16, y + 78, buf, 2, C_DIM, C_NAVY);
  { // external temperature (phone-relayed weather / forecast)
    float oT, oH;
    if (getOutdoor(oT, oH))
      snprintf(buf, sizeof(buf), "OUT %.0fC %.0f%%", oT, oH);
    else
      snprintf(buf, sizeof(buf), "OUT -- (no DHT / phone)");
    drawText(16, y + 104, buf, 2, C_BLUE, C_NAVY);
  }

  // right column: power
  uint16_t x2 = 260;
  snprintf(buf, sizeof(buf), "BAT %.2fV %u%%", battery.volts(), battery.percent());
  drawText(x2, y, buf, 2, battery.valid() ? C_WHITE : C_DIM, C_NAVY);
  bar(x2, y + 24, 210, 12, battery.percent(), C_GREEN);
  drawText(x2, y + 44, "HEAT", 2, C_DIM, C_NAVY);
  snprintf(buf, sizeof(buf), "%u%%", dryer.heatDuty());
  drawText(x2 + 60, y + 44, buf, 2, C_GOLD, C_NAVY);
  bar(x2, y + 68, 210, 12, dryer.heatDuty(), C_GOLD);
  drawText(x2, y + 88, "FAN", 2, C_DIM, C_NAVY);
  const char *fanSt = dryer.fanOutDuty() ? "ON " : "OFF";   // no tach: ON/OFF
  snprintf(buf, sizeof(buf), "%s %u%%", fanSt, dryer.fanOutDuty());
  drawText(x2 + 60, y + 88, buf, 2, C_BLUE, C_NAVY);
  bar(x2, y + 112, 210, 12, dryer.fanOutDuty(), C_BLUE);

  // weight line: current / target / difference (spec v2.0)
  {
    char wb[52];
    if (scale.ok() && !isnan(scale.grams())) {
      float cur = scale.grams();
      if (cfg.targetG > 0)
        snprintf(wb, sizeof(wb), "WT %.0fg  TGT %.0fg  DIFF %+.0fg",
                 (double)cur, (double)cfg.targetG, (double)(cur - cfg.targetG));
      else
        snprintf(wb, sizeof(wb), "WT %.0fg (rate %.1f g/min)",
                 (double)cur, (double)(scale.ok() ? scale.rate() : 0.0f));
    } else {
      snprintf(wb, sizeof(wb), "WT -- (no scale)");
    }
    drawText(16, 232, wb, 2, C_WHITE, C_NAVY);
    if (st == DState::DONE && dryer.suggestMin() > 0) {  // unreachable target
      snprintf(buf, sizeof(buf), "target was +%.0f min away", (double)dryer.suggestMin());
      drawText(16, 254, buf, 2, C_RED, C_NAVY);
    }
  }

  // door / workflow line
  {
    const char *ph = door::phase();
    char db[52];
    if (!door::calibrated())
      snprintf(db, sizeof(db), "START LOCKED - CALIBRATE THE SCALE");
    else if (strcmp(ph, "READY") == 0)
      snprintf(db, sizeof(db), "DOOR %s - BATCH %.0f g - %s START",
               door::closed() ? "CLOSED" : "OPEN", (double)door::batchG(),
               door::locked() ? "UNLOCK," : "PRESS");
    else
      snprintf(db, sizeof(db), "DOOR %s - %s",
               door::closed() ? "CLOSED" : "OPEN", ph);
    drawText(16, 210, db, 2, C_GOLD, C_NAVY);
  }

  // cycle graph (spark-line) + footer
  drawGraph(16, 258, 224, 54);
  const char *why = dryer.faultWhy();
  if (why && why[0]) {
    drawText(252, 266, why, 2, C_RED, C_NAVY);
  } else {
    drawText(252, 266, "WiFi AgarbattiDryer", 2, C_DIM, C_NAVY);
    drawText(252, 288, "192.168.4.1", 2, C_DIM, C_NAVY);
  }
}

}  // namespace display
#else
// DISPLAY_ENABLED 0: nothing to build
#endif