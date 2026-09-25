/*
 * ============================================================================
 *  SMART DEHUMIDIFIER - DISPLAY TEST (UNO only, no S3, nothing else connected)
 * ============================================================================
 *  Plug the 3.5" parallel shield (D0-D7 + WR + RD pins) onto the Arduino
 *  UNO, USB cable in, upload this. That's the whole setup.
 *
 *  Libraries (Tools -> Manage Libraries, install BOTH):
 *    1. "Adafruit GFX Library"  (by Adafruit)
 *    2. "mcufriend_kbv"         (by David Prentice)
 *
 *  WHAT IT SHOWS:
 *    - Serial Monitor (9600): the display CONTROLLER ID (e.g. 0x9486)
 *      >>> WRITE THAT NUMBER DOWN AND TELL ME - the S3 direct-wire driver
 *          (see below) needs it to pick the right init sequence.
 *    - Screen: colour bars, colour blocks, text, a running counter and
 *      a moving box - proves the panel, backlight and wiring are good.
 *
 *  WHY THIS MATTERS FOR THE S3: this shield can also be wired DIRECTLY to
 *  the ESP32-S3 (no UNO needed in the final product) by moving the door
 *  lock + supply relays + buzzer to a second PCF8574 - see manual 02
 *  sect.4.13. This test confirms your panel works and identifies its
 *  controller before that wiring.
 * ============================================================================
 */

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

MCUFRIEND_kbv tft;

// the same theme colours as the dryer's website
#define C_NAVY  0x0042
#define C_GOLD  0xD566
#define C_BLUE  0x4E1F
#define C_GREEN 0x3693
#define C_RED   0xFB8E
#define C_WHITE 0xFFFF
#define C_DIM   0xADB9

const char *colorName(uint16_t c) {
  if (c == C_RED)   return "RED";
  if (c == C_GREEN) return "GREEN";
  if (c == C_BLUE)  return "BLUE";
  if (c == C_GOLD)  return "GOLD";
  return "OTHER";
}

void bars() {                       // classic 8-colour bar strip
  const uint16_t cs[8] = { C_RED, C_GREEN, C_BLUE, C_GOLD,
                           C_WHITE, C_DIM, C_NAVY, 0 };
  for (uint8_t i = 0; i < 8; i++)
    tft.fillRect(i * 60, 0, 60, 40, cs[i]);
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("\n=== SMART DEHUMIDIFIER DISPLAY TEST ==="));

  uint16_t id = tft.readID();
  if (id == 0xD3D3) id = 0x9486;    // common readID() fallback on clones
  Serial.print(F("Controller ID: 0x"));
  Serial.println(id, HEX);
  Serial.println(F(">>> tell me this number (driver choice for the S3)"));

  tft.begin(id);
  tft.setRotation(1);               // 1 or 3 = landscape 480x320
  tft.fillScreen(C_NAVY);

  bars();
  tft.setTextColor(C_GOLD, C_NAVY);
  tft.setTextSize(3);
  tft.setCursor(40, 80);
  tft.print(F("SMART DEHUMIDIFIER"));
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_NAVY);
  tft.setCursor(40, 120);
  tft.print(F("display test OK - "));
  tft.print(colorName(tft.color565(255, 0, 0)));
  tft.setCursor(40, 150);
  tft.print(F("if colours look wrong, tell me which"));

  tft.drawRect(40, 190, 400, 90, C_GOLD);
  tft.setTextColor(C_DIM, C_NAVY);
  tft.setCursor(56, 210);
  tft.print(F("counter + moving box follow..."));
}

void loop() {
  static uint32_t n = 0;
  static uint16_t x = 60;

  tft.setTextColor(C_GREEN, C_NAVY);
  tft.setTextSize(2);
  tft.setCursor(56, 240);
  tft.print(F("count: "));
  tft.print(n);
  tft.print(F("   "));

  // moving box proves fast redraws
  tft.fillRect(x, 170, 30, 14, C_NAVY);        // erase old
  x = (x + 6) % 420;
  tft.fillRect(x, 170, 30, 14, C_BLUE);

  // full-colour wash every ~10 s (backlight + dead-pixel check)
  if (n % 40 == 0 && n > 0) {
    static const uint16_t w[4] = { C_RED, C_GREEN, C_BLUE, C_WHITE };
    tft.fillScreen(w[(n / 40) % 4]);
    tft.setTextColor(C_NAVY, w[(n / 40) % 4]);
    tft.setTextSize(3);
    tft.setCursor(120, 150);
    tft.print(F("WASH TEST"));
    delay(900);
    tft.fillScreen(C_NAVY);
    bars();
    tft.setTextColor(C_DIM, C_NAVY);
    tft.setTextSize(2);
    tft.setCursor(56, 210);
    tft.print(F("counter + moving box follow..."));
  }

  n++;
  delay(250);
}
