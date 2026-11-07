// Runs ONE sensor-test sketch on the host (mock Arduino core) and prints the
// JSON its web page polls from /data. Build: see run.sh (-DSKETCH="...").
#include "Arduino.h"
#include "WiFi.h"
#include "WebServer.h"
#include "ArduinoOTA.h"
#include "ESPmDNS.h"
#include "Wire.h"

uint64_t gMockUs = 0;
int gPinLevel[64];
int gPinToggle = 0;
bool gI2cPresent[128];
SerialMock Serial;
EspMock ESP;
WiFiMock WiFi;
ArduinoOTAMock ArduinoOTA;
MDNSMock MDNS;
TwoWire Wire;
TwoWire Wire1;

// ---- scriptable I2C devices ----------------------------------------------
static uint8_t eeMem[32768];              // 0x50: 24C256 (2-byte addressing)
static uint16_t eePtr = 0;
static uint8_t rtcReg[64] = {0x33, 0x04, 0x15, 0x06, 0x25, 0x09, 0x26, 0x00};  // DS1307
static uint8_t rtcPtr = 0;
uint8_t TwoWire::endTransmission(bool) {
  if (addr < 0 || addr > 127 || !gI2cPresent[addr]) return 2;       // NACK
  if (addr == 0x50 && txn >= 2) {
    eePtr = (uint16_t)(((tx[0] << 8) | tx[1]) & 0x7FFF);
    for (int i = 2; i < txn; i++) eeMem[(eePtr + i - 2) & 0x7FFF] = tx[i];
  } else if (addr == 0x68 && txn >= 1) {
    rtcPtr = tx[0] & 0x3F;
    for (int i = 1; i < txn; i++) { rtcReg[rtcPtr] = tx[i]; rtcPtr = (rtcPtr + 1) & 0x3F; }
  }
  return 0;
}
uint8_t TwoWire::requestFrom(int a, int n) {
  rxn = rxi = 0;
  if (a < 0 || a > 127 || !gI2cPresent[a]) return 0;
  static const uint8_t aht[6] = {0x18, 0x80, 0x00, 0x06, 0x00, 0x00};  // ~50 %RH / 25 C
  for (int i = 0; i < n && i < 64; i++) {
    uint8_t v = 0xFF;
    if (a == 0x50) v = eeMem[(eePtr + i) & 0x7FFF];
    else if (a == 0x68) { v = rtcReg[rtcPtr]; rtcPtr = (rtcPtr + 1) & 0x3F; }
    else if (a == 0x38) v = aht[i % 6];
    rx[rxn++] = v;
  }
  if (a == 0x50) eePtr = (uint16_t)((eePtr + n) & 0x7FFF);
  return (uint8_t)rxn;
}

#include SKETCH

int main(int argc, char **argv) {
  int sc = argc > 1 ? atoi(argv[1]) : 0;   // 0 = nothing connected, 1 = all present
  srand(1234 + sc);
  memset(gPinLevel, 0, sizeof gPinLevel);
  if (sc == 1) {
    gI2cPresent[0x38] = gI2cPresent[0x20] = gI2cPresent[0x50] = gI2cPresent[0x68] = true;
    for (int &p : gPinLevel) p = HIGH;
    gPinToggle = 1;                          // buttons / door / toggles move
  }
  setup();
  for (int i = 0; i < 80; i++) { delay(250); loop(); }
  if (web.routes.count("/set")) {            // exercise the rtc-test SET path too
    web.args["epoch"] = "1790330000"; web.args["tz"] = "330";
    web.routes["/set"]();
    fprintf(stderr, "[harness] /set -> %s\n", web.lastBody.c_str());
    for (int i = 0; i < 12; i++) { delay(250); loop(); }
  }
  if (!web.routes.count("/data")) { fprintf(stderr, "no /data route\n"); return 3; }
  web.routes["/data"]();                     // what the browser polls
  printf("%s\n", web.lastBody.c_str());
  return 0;
}
