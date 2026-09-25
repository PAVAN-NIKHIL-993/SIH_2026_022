#include "pixel.h"

#if PIXEL_ENABLED
#include <SPI.h>
#include "control.h"
#include "door.h"

namespace pixel {

static SPIClass spi(FSPI);          // the display is gone - SPI is free
static int      sPin = PIN_PIXEL;   // v2.0.21: re-pinnable at runtime (48/38)
static uint8_t  lastG = 255, lastR = 255, lastB = 255;

static void write(uint8_t g, uint8_t r, uint8_t b) {   // GRB order
  if (g == lastG && r == lastR && b == lastB) return;  // unchanged
  lastG = g;  lastR = r;  lastB = b;
  uint8_t buf[24];
  uint8_t grb[3] = {g, r, b};
  for (uint8_t i = 0; i < 24; i++)
    buf[i] = (grb[i / 8] >> (7 - (i % 8))) & 1 ? 0b11111000 : 0b11000000;
  spi.beginTransaction(SPISettings(6400000, MSBFIRST, SPI_MODE0));
  for (uint8_t n = 0; n < PIXEL_COUNT; n++) spi.writeBytes(buf, 24);
  spi.endTransaction();
  delayMicroseconds(60);                               // reset latch
}

void begin() {
  spi.begin(-1, -1, sPin, -1);                         // MOSI only
  write(0, 0, 0);
  Serial.printf("[pixel] RGB status pixel on GPIO%d (x%u) - "
                "v1.0 devkits: 48, v1.1: 38 (serial 'pixel 48|38' to switch)\n",
                sPin, (unsigned)PIXEL_COUNT);
}

// v2.0.21: the on-board pixel is GPIO48 on DevKitC v1.0 boards and
// GPIO38 on v1.1 boards (check the silkscreen). This moves the driver
// to the other candidate at runtime - the 300 ms white flash shows
// which physical LED just lit, so an external strip wired to either
// pin is driven identically. Only 48/38 are allowed (both safe as
// plain outputs; nothing else in the build uses them).
bool rePin(int pin) {
  if (pin != 48 && pin != 38) return false;
  sPin = pin;
  spi.end();
  spi.begin(-1, -1, sPin, -1);
  write(0, 0, 0);
  write(0, 255, 255);                                  // white = "that's the one"
  delay(300);
  write(0, 0, 0);
  Serial.printf("[pixel] moved to GPIO%d (x%u)\n", sPin, (unsigned)PIXEL_COUNT);
  return true;
}

void update() {
  uint32_t ms = millis();
  bool blink2 = (ms / 250) & 1;          // 2 Hz
  bool blink1 = (ms / 500) & 1;          // 1 Hz
  uint32_t ph = ms % 3000;
  uint8_t breathe = 20 + (uint8_t)(80 *
      (ph < 1500 ? ph : 3000 - ph) / 1500);            // 0..100 triangle

  if (ms < 3000)                 { write(0, 0, 60); return; }   // boot blue

  DState st = dryer.state();
  switch (st) {
    case DState::FAULT:   if (blink2) write(0, 120, 0);  else write(0, 0, 0);   return;
    case DState::DONE:    write(120, 0, 0);             return;   // batch ready
    case DState::COOLDOWN:if (blink1) write(60, 0, 60);  else write(0, 0, 0);   return;
    case DState::RUNNING:
      if (battery.percent() < 20) {                     // low battery
        if (blink1) write(60, 120, 0); else write(0, 0, 0);
      } else if (dryer.warnActive()) {                  // DEGRADED (v2.0.17):
        if (blink1) write(80, 80, 0); else write(0, 0, 0);  // yellow blink
      } else if (dryer.boosting()) {                    // max heat-up
        if (blink1) write(40, 100, 0); else write(0, 0, 0);
      } else {
        write(breathe, 0, 0);                           // green breathe
      }
      return;
    default:                                             // IDLE
      if (door::fitted() && !door::closed()) {          // load the trays
        write(60, 120, 0);
      } else {
        write((uint8_t)(breathe / 4), 0, (uint8_t)(breathe / 4)); // dim cyan
      }
      return;
  }
}

}  // namespace pixel

#else

namespace pixel {
void begin() {}
void update() {}
bool rePin(int) { return false; }
}

#endif