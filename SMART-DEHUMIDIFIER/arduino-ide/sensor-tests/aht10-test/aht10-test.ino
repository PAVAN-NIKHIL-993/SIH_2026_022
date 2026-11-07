/**
 * AHT10 SENSOR TEST - Smart Dehumidifier (sensor suite, v2.0.20)
 * ===================================================================
 * Checks the AHT10 chamber temperature/humidity sensor(s) with the
 * SAME raw-I2C driver the firmware uses (no extra libraries needed).
 *
 * Wiring (from the dryer build):
 *   classic ESP32:  sensor #1 (chamber top)    I2C0: SDA 21 / SCL 22
 *                   sensor #2 (chamber bottom) I2C1: SDA 32 / SCL 33
 *                   (the keypad PCF8574 @0x20 and AT24C256 @0x50 may
 *                    share I2C0 - they show up in the bus scan)
 *   ESP32-S3      : one sensor (chamber top)   I2C0: SDA 8 / SCL 9
 *   each sensor:   VCC -> 3V3, GND -> GND, SDA/SCL as above
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> live readings on your phone.
 *   3. Serial Monitor @ 115200 -> same + I2C bus scan + diagnosis.
 *   4. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "aht10-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Verdict:  PASS = sensor answers + plausible temperature/humidity
 *            FAIL = no AHT10 at 0x38 (check VCC/SDA/SCL)
 *            WARN = chip answers but measurements keep failing
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <ESPmDNS.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
  #define HW_S3 1
#else
  #define HW_S3 0
#endif

#define AP_SSID   "AgarbattiDryer"
#define AP_PASS   "dryer1234"
#define AP_CHAN   6
#define AP_MAXCL  4
static const char *OTA_HOST  = "aht10-test";
static const char *TEST_NAME = "AHT10";

// --------------------------------------------------------------------
// AHT10 driver - same protocol as src/aht10.cpp (firmware v2.0.20)
// --------------------------------------------------------------------
static const uint8_t AHT_ADDR  = 0x38;   // factory-fixed, not changeable
static const uint8_t CMD_RESET = 0xBA;
static const uint8_t CMD_CAL   = 0xE1;   // AHT10 init/calibrate
static const uint8_t CMD_TRIG  = 0xAC;   // trigger one-shot measurement

struct Aht {
  TwoWire *bus = nullptr;
  int sda = -1, scl = -1;
  const char *label = "";
  bool fitted = true;      // false = this board has no such sensor
  bool present = false;    // I2C ACK at 0x38
  bool ok = false;         // last measurement decoded
  float t = NAN, h = NAN;
  uint32_t lastOk = 0;

  void begin() {
    if (!fitted) return;
    bus->begin(sda, scl, 100000);
    bus->beginTransmission(AHT_ADDR);
    present = (bus->endTransmission() == 0);
    if (!present) {
      Serial.printf("[aht] %s: NO device at 0x38 (SDA %d / SCL %d) - check VCC + lines\n",
                    label, sda, scl);
      return;
    }
    bus->beginTransmission(AHT_ADDR);
    bus->write(CMD_RESET);
    bus->endTransmission();
    delay(25);
    bus->beginTransmission(AHT_ADDR);
    bus->write(CMD_CAL); bus->write(0x08); bus->write(0x00);
    bus->endTransmission();
    delay(50);
    Serial.printf("[aht] %s: AHT10 present (SDA %d / SCL %d)\n", label, sda, scl);
  }

  void read() {
    if (!present) return;
    uint32_t now = millis();
    if (now - lastOk < 2000) return;               // 0.5 Hz is plenty for a check

    bus->beginTransmission(AHT_ADDR);
    bus->write(CMD_TRIG); bus->write(0x33); bus->write(0x00);
    if (bus->endTransmission() != 0) return;

    uint32_t t0 = millis();
    while (millis() - t0 < 400) {                  // measurement takes ~80 ms
      delay(8);
      bus->requestFrom(AHT_ADDR, (uint8_t)1);
      if (bus->available() < 1) continue;
      uint8_t st = (uint8_t)bus->read();
      if (st & 0x80) continue;                     // still busy
      uint8_t b[6];
      bus->requestFrom(AHT_ADDR, (uint8_t)6);
      if (bus->available() < 6) break;
      for (int i = 0; i < 6; i++) b[i] = (uint8_t)bus->read();
      if (b[0] & 0x80) break;
      uint32_t rh = ((uint32_t)b[1] << 12) | ((uint32_t)b[2] << 4) | (b[3] >> 4);
      uint32_t rt = (((uint32_t)b[3] & 0x0F) << 16) | ((uint32_t)b[4] << 8) | b[5];
      float hv = (float)rh * 100.0f / 1048576.0f;  // datasheet formulas
      float tv = (float)rt * 200.0f / 1048576.0f - 50.0f;
      if (tv < -40 || tv > 90 || hv < 0 || hv > 105) break;
      t = tv; h = hv; ok = true; lastOk = now;
      return;
    }
    if (ok && now - lastOk > 10000) ok = false;    // keep last value, flag stale
  }

  const char *verdict() const {
    if (!fitted) return "n/a";
    if (!present) return "FAIL";
    if (!ok) return "WARN";
    return "PASS";
  }
};

Aht a1, a2;

// --------------------------------------------------------------------
// I2C bus scan (diagnosis: which chips answer where)
// --------------------------------------------------------------------
static void i2cScan(TwoWire &bus, int sda, int scl, const char *name) {
  Serial.printf("[scan] %s bus (SDA %d / SCL %d):\n", name, sda, scl);
  bus.begin(sda, scl, 100000);
  int n = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      Serial.printf("[scan]   0x%02X  %s\n", a,
        a == 0x38 ? "AHT10 (fixed address)"      :
        (a >= 0x20 && a <= 0x26) ? "PCF8574 keypad (strap pins A0-A2)" :
        a == 0x50 ? "AT24C256 EEPROM"            : "unknown device");
      n++;
    }
  }
  if (n == 0) Serial.printf("[scan]   nothing found - pull-ups? VCC? SDA/SCL swapped?\n");
}

// --------------------------------------------------------------------
// Web + OTA
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AHT10 test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;margin:0;padding:16px;color:#fff8ea;min-height:100vh;
  background:#1b45b5 linear-gradient(180deg,#2352c9 0%,#1d48b8 45%,#3560c8 62%,#c4914a 76%,#8a5a14 100%) fixed}
 h1{font-size:18px;margin:0 0 4px;color:#f6dd9c}.sub{color:#d6def4;font-size:12px;margin-bottom:14px}
 .card{background:rgba(19,48,142,.88);border:1px solid rgba(233,196,106,.35);border-radius:12px;padding:12px 14px;margin-bottom:12px;
  box-shadow:0 8px 20px rgba(6,16,58,.35)}
 .big{font-size:30px;font-weight:600}.ok{color:#5ee89a}.bad{color:#ff8a8a}.warn{color:#f7c552}
 .row{display:flex;justify-content:space-between;gap:12px;font-size:14px;margin-top:6px;color:#d6def4}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:rgba(8,24,80,.6)}
</style></head><body>
<h1>AHT10 sensor test</h1>
<div class="sub">smart dehumidifier &middot; aht10-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
<div id="cards">loading...</div>
<script>
function card(c){var h='<div class="card"><div class="big '+(c.cls||'')+'">'+c.big+'</div>';
 h+='<div style="color:#d6def4;font-size:12px">'+c.t+'</div>';
 (c.rows||[]).forEach(function(r){h+='<div class="row"><span>'+r[0]+'</span><span>'+r[1]+'</span></div>'});
 return h+'</div>'}
async function tick(){try{
 var j=await (await fetch('/data')).json();
 var st=document.getElementById('st');st.textContent=j.verdict;
 st.className='tag '+(j.verdict==='PASS'?'ok':(j.verdict==='FAIL'?'bad':'warn'));
 document.getElementById('cards').innerHTML=j.cards.map(card).join('');
}catch(e){}setTimeout(tick,1000)}
tick();
</script>
</body></html>
)rawliteral";

WebServer web(80);
String gJson = "{}";

// tiny JSON helper: rows = [["key","value"],...] built without trailing commas
static void jRows(String &rows, const char *k, const String &v) {
  if (rows.length()) rows += ",";
  rows += "[";
  rows += "\"";  rows += k;     rows += "\",";
  rows += "\"";  rows += v;     rows += "\"";
  rows += "]";
}

static void cardAht(const Aht &a, String &j) {
  String rows;
  if (!a.fitted) {
    jRows(rows, "Board", "ESP32-S3");
    jRows(rows, "Note", "S3 uses ONE AHT10 + a DHT22 (see dht-test)");
    j += "{\"t\":\"" + String(a.label) + "\",\"big\":\"not fitted\",\"cls\":\"\",\"rows\":[" + rows + "]}";
    return;
  }
  if (a.ok) {
    uint32_t age = (millis() >= a.lastOk) ? (millis() - a.lastOk) / 1000UL : 0;
    String big = String(a.t, 1) + " C";
    jRows(rows, "Humidity", String(a.h, 1) + " %RH");
    jRows(rows, "Last reading", String(age) + " s ago");
    jRows(rows, "I2C", String("SDA ") + String(a.sda) + " / SCL " + String(a.scl));
    jRows(rows, "Bus", "ACK + data OK");
    j += "{\"t\":\"" + String(a.label) + "\",\"big\":\"" + big + "\",\"cls\":\"ok\",\"rows\":[" + rows + "]}";
    return;
  }
  if (a.present) {
    String big = "no data";
    jRows(rows, "I2C", "ACK at 0x38, but measurements fail");
    jRows(rows, "Check", "SDA/SCL contact, 3V3 supply, long cables");
    j += "{\"t\":\"" + String(a.label) + "\",\"big\":\"" + big + "\",\"cls\":\"warn\",\"rows\":[" + rows + "]}";
  } else {
    jRows(rows, "I2C", "nothing ACKs at 0x38");
    jRows(rows, "Check", "VCC, GND, SDA, SCL - and that it is an AHT10 (0x38), not an AHT20");
    j += "{\"t\":\"" + String(a.label) + "\",\"big\":\"NO DEVICE\",\"cls\":\"bad\",\"rows\":[" + rows + "]}";
  }
}

static void refreshJson() {
  String j;
  j.reserve(600);
  // overall: any FAIL -> FAIL, else any WARN -> WARN, else PASS
  if (String(a1.verdict()) == "FAIL" || String(a2.verdict()) == "FAIL")      j += "{\"verdict\":\"FAIL\",\"cards\":[";
  else if (String(a1.verdict()) == "WARN" || String(a2.verdict()) == "WARN") j += "{\"verdict\":\"WARN\",\"cards\":[";
  else                                                                       j += "{\"verdict\":\"PASS\",\"cards\":[";
  cardAht(a1, j);
  j += ",";
  cardAht(a2, j);
  j += "]}";
  gJson = j;
}

static void netBegin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS, AP_CHAN, 0, AP_MAXCL);
  String ip = WiFi.softAPIP().toString();
  Serial.printf("[net] hotspot \"%s\" up (pass: %s) - phone/PC: http://%s\n",
                AP_SSID, AP_PASS, ip.c_str());

  ArduinoOTA.setHostname(OTA_HOST);
  ArduinoOTA.onStart([]() { Serial.println(F("[ota] network update STARTED")); });
  ArduinoOTA.onEnd([]()   { Serial.println(F("[ota] update complete - rebooting")); });
  ArduinoOTA.onProgress([](uint32_t d, uint32_t tot) {
    if (tot == 0) return;
    static uint8_t lastPct = 255;
    uint8_t pct = (uint8_t)(d * 100UL / tot);
    if (pct != lastPct && pct % 25 == 0) { lastPct = pct; Serial.printf("[ota] %u%%\n", (unsigned)pct); }
  });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[ota] error %u\n", (unsigned)e); });
  if (!MDNS.begin(OTA_HOST)) Serial.println(F("[ota] mDNS failed - use the IP directly"));
  else MDNS.addService("http", "tcp", 80);
  ArduinoOTA.begin();
  Serial.printf("[ota] ready - Arduino IDE: join \"%s\" -> Tools > Port > \"%s at %s\"\n",
                AP_SSID, OTA_HOST, ip.c_str());
}

// --------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==============================================");
  Serial.printf(" %s SENSOR TEST - smart dehumidifier v2.0.20\n", TEST_NAME);
  Serial.printf(" board: %s (S3: %s)\n",
                ESP.getChipModel(), HW_S3 ? "YES" : "no");

  if (HW_S3) {
    a1.bus = &Wire;   a1.sda = 8;  a1.scl = 9;
    a1.label = "AHT10 chamber top (S3)";
    a2.fitted = false;
    a2.label = "AHT10 #2 (classic only)";
  } else {
    a1.bus = &Wire;   a1.sda = 21; a1.scl = 22;
    a1.label = "AHT10 #1 chamber top";
    a2.bus = &Wire1;  a2.sda = 32; a2.scl = 33;
    a2.label = "AHT10 #2 chamber bottom";
  }

  i2cScan(Wire,  a1.sda, a1.scl, HW_S3 ? "I2C0 (S3)" : "I2C0 (classic)");
  if (!HW_S3) i2cScan(Wire1, a2.sda, a2.scl, "I2C1 (classic)");

  a1.begin();
  a2.begin();

  web.on("/", []() { web.send_P(200, "text/html", INDEX_HTML); });
  web.on("/data", []() { web.send(200, "application/json", gJson); });
  web.begin();
  netBegin();
  Serial.println("==============================================");
  refreshJson();
}

void loop() {
  ArduinoOTA.handle();          // OTA lives in every test sketch
  web.handleClient();

  a1.read();
  a2.read();

  static uint32_t lastLine = 0;
  uint32_t now = millis();
  if (now - lastLine >= 1000) {
    lastLine = now;
    auto fmt = [](const Aht &a) -> String {
      if (!a.fitted) return "n/a (not on S3)";
      if (!a.present) return "FAIL - no device at 0x38";
      if (!a.ok) return "WARN - chip answers, no data yet";
      return String(a.t, 1) + " C  " + String(a.h, 1) + " %RH";
    };
    Serial.printf("[aht] %s : %s\n", a1.label, fmt(a1).c_str());
    Serial.printf("[aht] %s : %s\n", a2.label, fmt(a2).c_str());
    refreshJson();
  }
}
