/*
 * ============================================================================
 *  SMART DEHUMIDIFIER - DISPLAY BRIDGE for Arduino UNO / Nano / Mega 2560
 * ============================================================================
 *  Drives the PARALLEL "UNO-shield" 3.5" TFT (the one with D0-D7 + WR + RD
 *  pins - it plugs straight onto an Arduino UNO) and shows the dryer's live
 *  status, which the ESP32-S3 streams over ONE wire.
 *
 *  THE DEAL:
 *    ESP32-S3 (the dryer brain, runs SMART-DEHUMIDIFIER firmware)
 *       |  GPIO3 TX @ 9600 baud - tiny text packets, 1 per second
 *       v
 *    this Arduino (the display driver - that's ALL it does)
 *       |
 *       v
 *    the parallel TFT (plugged on top / wired per the shield's labels)
 *
 *  WIRING (only 2 wires + power):
 *    S3 GPIO3  ->  UNO/Nano pin 4  (SoftwareSerial RX)   * Mega: pin 19 (RX1)
 *    S3 GND    ->  Arduino GND    (common ground - MANDATORY)
 *    Arduino 5V/GND from the dryer's 5 V buck (UNO "5V" pin, stable supply)
 *    !! NEVER connect the Arduino's TX (5 V) to the ESP32 - it will damage it.
 *       The bridge is one-directional; leave the Arduino TX unconnected.
 *
 *  LIBRARIES (Arduino IDE -> Tools -> Manage Libraries, install BOTH):
 *    1. "Adafruit GFX Library"   (by Adafruit)
 *    2. "mcufriend_kbv"          (by David Prentice - drives these shields)
 *    The ESP32-S3 firmware needs NO libraries; only this Arduino does.
 *
 *  FLASH: Board = "Arduino UNO" (or your board) -> Upload. The S3 wire on
 *  pin 4 can stay connected - it does not interfere with uploading.
 *
 *  PACKET (from the S3, one burst per second, lines end with \\n):
 *    $ST,DRYING            state
 *    $T,60.5,58.2          T1, T2 (deg C)
 *    $H,45,50              RH1, RH2 (%)
 *    $SET,60,120           target temp, drying minutes
 *    $E,3600,3600          elapsed s, remaining s
 *    $P,34,100,12.4,78     heat %, fan %, battery V, battery %
 *    $W,1234,1000,234      weight g, target g, difference g
 *    $D,READY,1,1234       door phase, closed(1/0), batch g
 *    $S,SOLAR MODE,1,0     supply name, feed live(1/0), mode(0 agarbatti)
 *    $F,<text>             fault text (empty = none)
 *    $V,2.0.3              firmware version
 * ============================================================================
 */

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
  #define BR Serial1          // Mega: hardware UART1, RX = pin 19
  #define BR_RX_PIN 19
#else
  #include <SoftwareSerial.h>
  SoftwareSerial BR(4, 5);    // UNO/Nano: RX = pin 4 (S3 TX wire), TX unused
  #define BR_RX_PIN 4
#endif

MCUFRIEND_kbv tft;

// ---- palette (RGB565, same theme as the dryer's website) ----
#define C_NAVY  0x0042
#define C_NAVY2 0x0145
#define C_CARD  0x0142
#define C_GOLD  0xD566
#define C_BLUE  0x4E1F
#define C_GREEN 0x3693
#define C_RED   0xFB8E
#define C_WHITE 0xFFFF
#define C_DIM   0xADB9

// ---- latest received values ----
char   f_st[12] = "--", f_door[12] = "--", f_sup[16] = "--";
char   f_flt[56] = "",  f_ver[10] = "";
float  f_t1 = NAN, f_t2 = NAN, f_h1 = NAN, f_h2 = NAN;
float  f_setT = 0, f_batV = 0, f_wt = 0, f_tgt = 0, f_dif = 0, f_batch = 0;
unsigned long f_el = 0, f_rem = 0;
int    f_min = 0, f_heat = 0, f_fan = 0, f_batP = 0, f_dcl = 0,
       f_live = 0, f_mode = 0;
uint32_t lastPacket = 0;

// ---- tiny helpers -------------------------------------------------------
void txt(int x, int y, int size, uint16_t c, const char *s) {
  tft.setTextColor(c, C_NAVY);
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(s);
}
void txtf(int x, int y, int size, uint16_t c, const char *fmt, ...) {
  char b[48];
  va_list ap; va_start(ap, fmt);
  vsnprintf(b, sizeof(b), fmt, ap);
  va_end(ap);
  txt(x, y, size, c, b);
}
void bar(int x, int y, int w, int pct, uint16_t c) {
  tft.fillRect(x, y, w, 12, C_CARD);
  if (pct > 100) pct = 100;
  tft.fillRect(x + 1, y + 1, (w - 2) * (long)pct / 100, 10, c);
}
const char *modeName() {
  return f_mode == 0 ? "AGARBATTI" : f_mode == 2 ? "SILICAGEL" : "USER";
}

char *split2(char *s) {
  char *p = strchr(s, ',');
  if (!p) return (char *)"";
  *p = 0;
  return p + 1;
}

void apply(char *line) {
  if (line[0] != '$') return;
  char *k = line + 1;
  char *p = strchr(k, ',');
  if (!p) return;
  *p = 0;
  char *a = p + 1;
  if      (!strcmp(k, "ST")) strncpy(f_st,  a, sizeof(f_st) - 1);
  else if (!strcmp(k, "T"))  { f_t1 = atof(a); f_t2 = atof(split2(a)); }
  else if (!strcmp(k, "H"))  { f_h1 = atof(a); f_h2 = atof(split2(a)); }
  else if (!strcmp(k, "SET")){ f_setT = atof(a); f_min = atoi(split2(a)); }
  else if (!strcmp(k, "E"))  { f_el = strtoul(a, 0, 10); f_rem = strtoul(split2(a), 0, 10); }
  else if (!strcmp(k, "P"))  { f_heat = atoi(a); char *b = split2(a);
                               f_fan = atoi(b); char *c = split2(b);
                               f_batV = atof(c); f_batP = atoi(split2(c)); }
  else if (!strcmp(k, "W"))  { f_wt = atof(a); char *b = split2(a);
                               f_tgt = atof(b); f_dif = atof(split2(b)); }
  else if (!strcmp(k, "D"))  { strncpy(f_door, a, sizeof(f_door) - 1);
                               char *b = split2(a); f_dcl = atoi(b);
                               f_batch = atof(split2(b)); }
  else if (!strcmp(k, "S"))  { strncpy(f_sup, a, sizeof(f_sup) - 1);
                               char *b = split2(a); f_live = atoi(b);
                               f_mode = atoi(split2(b)); }
  else if (!strcmp(k, "F"))  { strncpy(f_flt, a, sizeof(f_flt) - 1);
                               f_flt[sizeof(f_flt) - 1] = 0; }
  else if (!strcmp(k, "V"))  { strncpy(f_ver, a, sizeof(f_ver) - 1); }
  lastPacket = millis();
}

// ---- the screen ---------------------------------------------------------
void drawScreen() {
  // title bar
  tft.fillRect(0, 0, 480, 30, C_NAVY2);
  txt(8, 8, 2, C_GOLD, "SMART DEHUMIDIFIER");
  txt(200, 8, 2, f_live ? C_GREEN : C_RED, f_sup);
  txt(384, 8, 2, C_WHITE, modeName());

  // state + times
  uint16_t stc = !strcmp(f_st, "FAULT") ? C_RED :
                 !strcmp(f_st, "RUNNING") ? C_GREEN :
                 !strcmp(f_st, "DONE") ? C_BLUE : C_DIM;
  txt(16, 44, 4, stc, f_st);
  txtf(16, 84, 2, C_WHITE, "ELAPSED %lu:%02lu:%02lu  LEFT %lu:%02lu",
       f_el / 3600, (f_el / 60) % 60, f_el % 60, f_rem / 60, f_rem % 60);

  // climate column
  txtf(16, 120, 2, C_GOLD, "T1 %.1fC   T2 %.1fC", (double)f_t1, (double)f_t2);
  txtf(16, 144, 2, C_WHITE, "RH1 %.0f%%  RH2 %.0f%%", (double)f_h1, (double)f_h2);
  txtf(16, 168, 2, C_DIM, "SET %.0fC  %d min", (double)f_setT, f_min);

  // power column
  txtf(260, 120, 2, C_WHITE, "BAT %.2fV %d%%", (double)f_batV, f_batP);
  bar(260, 142, 210, f_batP, C_GREEN);
  txtf(260, 164, 2, C_GOLD, "HEAT %d%%", f_heat);
  bar(260, 186, 210, f_heat, C_GOLD);
  txtf(260, 208, 2, C_BLUE, "FAN %s %d%%", f_fan ? "ON " : "OFF", f_fan);
  bar(260, 230, 210, f_fan, C_BLUE);

  // weight
  if (f_tgt > 0)
    txtf(16, 200, 2, C_WHITE, "WT %.0fg  TGT %.0fg  DIFF %+.0fg",
         (double)f_wt, (double)f_tgt, (double)f_dif);
  else
    txtf(16, 200, 2, C_WHITE, "WT %.0fg (no target)", (double)f_wt);

  // door + fault/footer
  txtf(16, 226, 2, C_GOLD, "DOOR %s - %s", f_dcl ? "CLOSED" : "OPEN", f_door);
  if (f_flt[0]) {
    tft.fillRect(0, 252, 480, 40, C_NAVY);
    txt(16, 258, 2, C_RED, f_flt);
  } else {
    txt(16, 258, 2, C_DIM, "WiFi AgarbattiDryer - 192.168.4.1");
  }
  txtf(400, 304, 1, C_DIM, "v%s", f_ver);
}

void drawWaiting() {
  tft.fillScreen(C_NAVY);
  txt(60, 120, 3, C_GOLD, "SMART DEHUMIDIFIER DISPLAY");
  txt(60, 160, 2, C_DIM, "waiting for the dryer (S3)...");
  char b[60];
  snprintf(b, sizeof(b), "check wire: S3 GPIO3 -> Arduino pin %d + GND", BR_RX_PIN);
  txt(60, 184, 2, C_DIM, b);
}

// ---- Arduino boot -------------------------------------------------------
void setup() {
  BR.begin(9600);                 // matches TXDISP_BAUD on the S3
  uint16_t id = tft.readID();
  if (id == 0xD3D3) id = 0x9486;  // readID() fallback for some clones
  tft.begin(id);
  tft.setRotation(1);             // landscape 480x320
  tft.fillScreen(C_NAVY);
  drawWaiting();
}

char rxBuf[96];
uint8_t rxLen = 0;

void loop() {
  while (BR.available()) {
    char c = BR.read();
    if (c == '\n') {
      rxBuf[rxLen] = 0;
      apply(rxBuf);
      rxLen = 0;
    } else if (rxLen < sizeof(rxBuf) - 1 && c != '\r') {
      rxBuf[rxLen++] = c;
    }
  }
  static uint32_t lastDraw = 0;
  uint32_t now = millis();
  if (now - lastDraw < 1000) return;          // redraw 1 Hz
  lastDraw = now;
  if (now - lastPacket > 10000) { drawWaiting(); return; }
  drawScreen();
}
