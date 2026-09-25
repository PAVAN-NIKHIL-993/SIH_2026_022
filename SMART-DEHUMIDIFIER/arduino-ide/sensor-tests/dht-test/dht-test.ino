/**
 * DHT SENSOR TEST - Smart Dehumidifier (sensor suite, v2.0.20)
 * ===================================================================
 * Checks the DHT sensors with the SAME bit-bang 40-bit-frame decoder
 * the firmware uses (no DHT library needed).
 *
 * Wiring (from the dryer build):
 *   ESP32-S3      : DHT22 chamber (cool return) DATA -> GPIO 10
 *                   DHT11 outdoor (shade!)         DATA -> GPIO 41
 *   classic ESP32 : no DHT pins in the dryer build - but you can still
 *                   bench-test any DHT11/DHT22: set TEST_DHT_PIN below
 *   each sensor:   VCC -> 3V3, GND -> GND, DATA -> GPIO,
 *                   + 10 k resistor DATA -> 3V3 (at the sensor end
 *                   of long cables)
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> live readings on your phone.
 *   3. Serial Monitor @ 115200 -> same + frame statistics.
 *   4. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "dht-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Verdict:  PASS = frames decode (temperature + humidity in range)
 *            FAIL = no frames at all (wiring / pull-up / 3V3)
 *            WARN = frames arriving but bad checksums
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
  #define HW_S3 1
#else
  #define HW_S3 0
#endif

// ---- classic board bench override -------------------------------------
// The classic ESP32 has no DHT pins in the dryer build. To test a spare
// DHT on the classic, set the DATA GPIO here (and TEST_DHT11 if it is a
// DHT11). On the S3 these are ignored (the dryer pins are used).
#define TEST_DHT_PIN   (-1)
#define TEST_DHT11     false

#define AP_SSID   "AgarbattiDryer"
#define AP_PASS   "dryer1234"
#define AP_CHAN   6
#define AP_MAXCL  4
static const char *OTA_HOST  = "dht-test";
static const char *TEST_NAME = "DHT";

// --------------------------------------------------------------------
// DHT driver - same 40-bit frame decoder as src/dht.cpp (v2.0.20)
// --------------------------------------------------------------------
static bool dhtFrame(int8_t pin, bool is11, float &t, float &h) {
  uint8_t d[5] = {0, 0, 0, 0, 0};

  // start: pull low 2 ms, release, wait for the sensor's 80/80 us reply
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(2);
  digitalWrite(pin, HIGH);
  pinMode(pin, INPUT);              // external 10 k pulls the line up
  delayMicroseconds(30);

  uint32_t t0 = micros();
  while (digitalRead(pin) == LOW)  if (micros() - t0 > 100) return false;
  t0 = micros();
  while (digitalRead(pin) == HIGH) if (micros() - t0 > 100) return false;

  // 40 bits: 50 us low, then a 26-70 us high; long high = 1
  for (uint8_t i = 0; i < 40; i++) {
    t0 = micros();
    while (digitalRead(pin) == LOW)  if (micros() - t0 > 80)  return false;
    t0 = micros();
    while (digitalRead(pin) == HIGH) if (micros() - t0 > 100) return false;
    d[i / 8] <<= 1;
    if (micros() - t0 > 48) d[i / 8] |= 1;
  }

  if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return false;   // checksum

  if (is11) {                  // DHT11: integer bytes + decimal byte
    h = (float)d[0] + d[1] * 0.1f;
    t = (float)d[2] + d[3] * 0.1f;
  } else {                     // DHT22/AM2302: tenths, sign bit
    h = (float)(((uint16_t)d[0] << 8) | d[1]) * 0.1f;
    float raw = (float)(((uint16_t)(d[2] & 0x7F) << 8) | d[3]) * 0.1f;
    t = (d[2] & 0x80) ? -raw : raw;
  }
  return true;
}

struct DhtDev {
  int8_t pin = -1;
  bool is11 = false;
  const char *label = "";
  bool fitted = true;
  uint32_t everyMs = 10000;
  float t = NAN, h = NAN;
  bool ok = false;
  uint32_t good = 0, bad = 0, miss = 0;
  uint32_t lastTry = 0;

  void begin() {
    if (pin < 0) { fitted = false; return; }
    pinMode(pin, INPUT);
    Serial.printf("[dht] %s on GPIO %d (cadence %u s)\n",
                  label, (int)pin, (unsigned)(everyMs / 1000UL));
  }

  void update() {
    if (pin < 0) return;
    uint32_t now = millis();
    if (now - lastTry < everyMs) return;
    lastTry = now;

    float t, h;
    if (dhtFrame(pin, is11, t, h)) {
      if (t < -20 || t > 60 || h < 1 || h > 100) { bad++; return; }  // implausible
      this->t = t; this->h = h; miss = 0;
      if (!ok) Serial.printf("[dht] %s: FIRST GOOD FRAME\n", label);
      ok = true; good++;
    } else {
      if (++miss >= 3) {
        if (ok) Serial.printf("[dht] %s: stopped responding\n", label);
        ok = false;
      }
    }
  }
};

static DhtDev d22, d11;

// --------------------------------------------------------------------
// Web + OTA
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DHT test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;background:#101418;color:#e8edf2;margin:0;padding:16px}
 h1{font-size:18px;margin:0 0 4px}.sub{color:#8a97a5;font-size:12px;margin-bottom:14px}
 .card{background:#1a2129;border:1px solid #2a3542;border-radius:10px;padding:12px 14px;margin-bottom:12px}
 .big{font-size:30px;font-weight:600}.ok{color:#4cd964}.bad{color:#ff5f57}.warn{color:#ffd60a}
 .row{display:flex;justify-content:space-between;font-size:14px;margin-top:6px;color:#b9c4cf}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:#2a3542}
</style></head><body>
<h1>DHT sensor test</h1>
<div class="sub">smart dehummidifier &middot; dht-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
<div id="cards">loading...</div>
<script>
function card(c){var h='<div class="card"><div class="big '+(c.cls||'')+'">'+c.big+'</div>';
 h+='<div style="color:#8a97a5;font-size:12px">'+c.t+'</div>';
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

static void jRows(String &rows, const char *k, const String &v) {
  if (rows.length()) rows += ",";
  rows += "[";
  rows += "\"";  rows += k;     rows += "\",";
  rows += "\"";  rows += v;     rows += "\"";
  rows += "]";
}

static void cardDht(DhtDev &a, String &j) {
  String rows;
  if (a.pin < 0) {
    jRows(rows, "Board", HW_S3 ? "no pin in this layout" : "classic ESP32");
    jRows(rows, "Note", HW_S3 ? "check S3 wiring" : "set TEST_DHT_PIN in the sketch to bench-test one");
    j += "{\"t\":\"" + String(a.label) + "\",\"big\":\"not fitted\",\"cls\":\"\",\"rows\":" + rows + "}";
    return;
  }
  if (a.ok) {
    jRows(rows, "Humidity", String(a.h, 1) + " %RH");
    jRows(rows, "Good frames", String(a.good));
    jRows(rows, "Bad/checksum", String(a.bad + a.miss));
    jRows(rows, "Data pin", "GPIO " + String((int)a.pin));
    j += "{\"t\":\"" + String(a.label) + "\",\"big\":\"" + String(a.t, 1) +
         " C\",\"cls\":\"ok\",\"rows\":" + rows + "}";
    return;
  }
  if (a.good > 0 || a.bad > 0) {
    jRows(rows, "Good frames so far", String(a.good));
    jRows(rows, "Note", "was working, check the connection now");
    j += "{\"t\":\"" + String(a.label) + "\",\"big\":\"no data\",\"cls\":\"warn\",\"rows\":" + rows + "}";
  } else {
    jRows(rows, "Frames", "none decoded yet (3 s / 10 s cadence)");
    jRows(rows, "Check", "3V3 + GND + 10 k pull-up DATA->3V3, GPIO");
    j += "{\"t\":\"" + String(a.label) + "\",\"big\":\"NO FRAMES\",\"cls\":\"bad\",\"rows\":" + rows + "}";
  }
}

static void refreshJson() {
  String j;
  j.reserve(600);
  if (!d22.fitted && !d11.fitted)                          j += "{\"verdict\":\"NOT FITTED\",\"cards\":[";
  else if (d22.fitted && !d22.ok && d11.fitted && !d11.ok) j += "{\"verdict\":\"FAIL\",\"cards\":[";
  else if (d22.ok || d11.ok)                               j += "{\"verdict\":\"PASS\",\"cards\":[";
  else                                                     j += "{\"verdict\":\"WARN\",\"cards\":[";
  cardDht(d22, j);
  j += ",";
  cardDht(d11, j);
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
  Serial.printf(" %s SENSOR TEST - smart dehummidifier v2.0.20\n", TEST_NAME);
  Serial.printf(" board: %s (S3: %s)\n",
                ESP.getChipModel(), HW_S3 ? "YES" : "no");

  if (HW_S3) {
    d22.pin = 10; d22.everyMs = 3000;  d22.label = "DHT22 chamber (cool return)";
    d11.pin = 41; d11.everyMs = 10000; d11.is11 = true; d11.label = "DHT11 outdoor (shade)";
  } else {
    d22.pin = (int8_t)TEST_DHT_PIN; d22.is11 = TEST_DHT11;
    d22.everyMs = 3000; d22.label = "DHT bench (TEST_DHT_PIN)";
    d11.pin = -1; d11.label = "DHT11 outdoor (S3 only)";
  }

  d22.begin();
  d11.begin();

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

  d22.update();
  d11.update();

  static uint32_t lastLine = 0;
  uint32_t now = millis();
  if (now - lastLine >= 1000) {
    lastLine = now;
    auto fmt = [](DhtDev &a) -> String {
      if (a.pin < 0) return "n/a";
      if (!a.ok) return a.good ? "was OK, now silent" : "no frames yet";
      return String(a.t, 1) + " C  " + String(a.h, 1) + " %RH";
    };
    Serial.printf("[dht] %-28s: %s\n", d22.label, fmt(d22).c_str());
    Serial.printf("[dht] %-28s: %s\n", d11.label, fmt(d11).c_str());
    refreshJson();
  }
}
