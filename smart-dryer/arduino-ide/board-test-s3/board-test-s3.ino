/**
 * BOARD TEST — ESP32-S3 DevKitC (N8 / N16 / N16R8)
 * ------------------------------------------------
 * Flash this FIRST on a new board and open Serial Monitor @ 115200.
 * It proves the board from the serial monitor alone:
 *
 *   1. chip model + revision + cores + MAC
 *   2. flash size  (N16 = 16 MB expected)
 *   3. PSRAM size  (R8 = 8 MB expected) + a write/read test
 *      -> if PSRAM is 8 MB this is an OCTAL board: GPIO 35/36/37 are
 *         used by PSRAM. The dryer pin map (v2.0.13) never touches
 *         them (opto = 18, DHT11 = 41), so an N16R8 is fully OK.
 *   4. I2C bus scan on the dryer's pins GPIO 8/9:
 *      0x38 = AHT10, 0x20 = PCF #1 keypad, 0x21 = PCF #2 panel
 *      (only when those are wired)
 *   5. live pin monitor, 1 line per second — press BOOT, short a pin
 *      to GND, connect a sensor, and watch the level flip:
 *        BOOT(0) HX711(1=SCK 2=DT) ADC(4) DHT22(10) DHT11(41)
 *        BTN1(15) BTN2(16) OPTO(18) DOOR(21)
 *
 * Arduino IDE settings (Tools):
 *   Board: "ESP32S3 Dev Module"
 *   USB CDC On Boot: ENABLED        <- required for Serial!
 *   Flash Size: 16MB (or 8MB on N8)
 *   PSRAM: "OPI PSRAM" (R8 boards)
 * Use the USB-C port labeled USB / the one closer to the board edge
 * (native USB). Nothing else needed — no wiring required.
 */

#include <Wire.h>

// the dryer build's pins (config-s3.h, v2.0.13)
#define PIN_I2C_SDA  8
#define PIN_I2C_SCL  9
static const struct { int gpio; const char *name; } INPUTS[] = {
  {  0, "BOOT"   },   // on-board BOOT button (idle H, press = L)
  {  1, "HX711SK"},   // HX711 SCK (idle H when wired)
  {  2, "HX711DT"},   // HX711 DT  (idle H when wired)
  { 10, "DHT22"  },   // DHT22 data (idle H with its 10k pull-up)
  { 15, "BTN1"   },   // BUTTON-1 (idle H, press = L)
  { 16, "BTN2"   },   // BUTTON-2 (idle H, press = L)
  { 18, "OPTO"   },   // supply optocoupler OUT
  { 41, "DHT11"  },   // DHT11 data (idle H with its 10k pull-up)
  { 21, "DOOR"   },   // door limit switch (closed = L)
};

void rule() { Serial.println("--------------------------------------------------"); }

void i2cScan() {
  Serial.println("I2C scan on GPIO 8 (SDA) / GPIO 9 (SCL):");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X  %s\r\n", a,
        a == 0x38 ? "AHT10 (chamber top)"   :
        a == 0x20 ? "PCF8574 #1 (keypad)"   :
        a == 0x21 ? "PCF8574 #2 (panel)"    :
        a == 0x22 ? "PCF8574 (check strap!)" : "unknown device");
      n++;
    }
  }
  if (!n) Serial.println("  (nothing found - fine on a bare board)");
  Serial.println("  type r + Enter to rescan");
}

void setup() {
  Serial.begin(115200);
  delay(2000);                                   // let the monitor open
  rule();
  Serial.println("  ESP32-S3 BOARD TEST  -  smart-dryer v2.0.13 pin map");
  rule();

  Serial.printf("chip      : %s  rev %d  %d cores @ %d MHz\r\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.printf("flash     : %u MB @ %u MHz\r\n",
                ESP.getFlashChipSize() / 1048576, ESP.getFlashChipSpeed() / 1000000);
  Serial.printf("heap free : %u kB\r\n", ESP.getFreeHeap() / 1024);
  uint8_t mac[6];  ESP.getEfuseMac(mac);  // byte-wise to avoid printf issues
  Serial.printf("MAC       : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                mac[0], mac[5], mac[3], mac[4], mac[2], mac[1]);

  // ---- PSRAM: the R8 question ------------------------------------------
  uint32_t ps = ESP.getPsramSize();
  if (ps) {
    Serial.printf("PSRAM     : %u MB (%u kB free)\r\n", ps / 1048576,
                  ESP.getFreePsram() / 1024);
    uint8_t *t = (uint8_t *) ps_malloc(1024);
    if (t) {
      bool ok = true;
      for (int i = 0; i < 1024; i++) t[i] = (uint8_t)(i ^ 0xA5);
      for (int i = 0; i < 1024; i++) if (t[i] != (uint8_t)(i ^ 0xA5)) ok = false;
      free(t);
      Serial.println(ok ? "PSRAM test: PASS (write/read OK)"
                        : "PSRAM test: FAIL - board is faulty");
    } else Serial.println("PSRAM test: FAIL (ps_malloc refused)");
    if (ps >= 8 * 1048576) {
      Serial.println("  -> OCTAL PSRAM (R8): GPIO 35/36/37 belong to PSRAM.");
      Serial.println("     This build never wires them (opto=18, DHT11=41):");
      Serial.println("     your N16R8 is FULLY SUPPORTED. Nothing to change.");
    }
  } else {
    Serial.println("PSRAM     : none (N8/N16 quad boards) - GPIO 35 free, fine too");
  }

  // ---- the build's pin map (what this board will run) --------------------
  rule();
  Serial.println("dryer pin map (config-s3.h): 1/2 HX711 - 4 ADC - 5 gate -");
  Serial.println("8/9 I2C - 10 DHT22 - 12/13 heater - 14 latch - 17 fan -");
  Serial.println("18 opto - 41 DHT11 - 39 toggle - 21 door - 6/7 relays");
  Serial.println("GPIO 35/36/37: never wired (PSRAM on R8) - 45/46 strapping");
  rule();

  for (auto &p : INPUTS) pinMode(p.gpio, INPUT_PULLUP);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  i2cScan();
  rule();
  Serial.println("live pins (1/sec) - press BOOT / short pins to GND /");
  Serial.println("plug sensors in and watch the levels change:");
}

void loop() {
  if (Serial.available() && Serial.read() == 'r') i2cScan();

  Serial.printf("[%6u s] ", millis() / 1000);
  for (auto &p : INPUTS)
    Serial.printf("%s=%d ", p.name, digitalRead(p.gpio));
  Serial.printf("ADC(4)=%d\r\n", analogRead(4));   // raw 0..4095 (floats bare)
  delay(1000);
}