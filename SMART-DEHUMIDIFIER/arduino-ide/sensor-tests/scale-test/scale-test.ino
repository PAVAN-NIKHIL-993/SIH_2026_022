/**
 * WEIGH SCALE TEST - Smart Dehumidifier (sensor suite, v2.0.20)
 * ===================================================================
 * Checks the HX711 + 2 x half-bridge load cells ("dry to weight") with
 * the SAME bit-bang driver the firmware uses (no HX711 library needed).
 *
 * Wiring (from the dryer build):
 *   ESP32-S3      : HX711 SCK -> GPIO 1, DOUT -> GPIO 2
 *   classic ESP32 : no HX711 pins in the dryer build - bench-test any
 *                   HX711 by setting TEST_SCALE_CLK / TEST_SCALE_DOUT
 *                   (the config.h suggestion for classic: CLK 12, DOUT 34)
 *   HX711 board:   VCC 3V3 (or 5V), GND, SCK, DOUT -> as above
 *   load cells:    the two half-bridge cells on the tray rails, OUT+/
 *                   OUT- to the HX711 A+/A-
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> live raw readings + stability on phone.
 *   3. Serial Monitor @ 115200, then use the commands:
 *         t        tare (zero at whatever is on the tray now)
 *         c <g>    calibrate: put a KNOWN weight on, then type it,
 *                  e.g. c 100  for 100 g (tare first!)
 *         f        show current factor + offset
 *   4. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "scale-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Verdict:  PASS = chip answers + stable raw counts (small spread)
 *            FAIL = HX711 not responding (VCC? SCK/DOUT swapped?)
 *            WARN = chip answers but the counts are jumping a lot
 *                   (cell wiring / mechanical vibration / 5 V noise)
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
// -1 = not tested. Classic dryer build suggestion: CLK 12, DOUT 34.
#define TEST_SCALE_CLK   (-1)
#define TEST_SCALE_DOUT  (-1)

#define AP_SSID   "AgarbattiDryer"
#define AP_PASS   "dryer1234"
#define AP_CHAN   6
#define AP_MAXCL  4
static const char *OTA_HOST  = "scale-test";
static const char *TEST_NAME = "WEIGH SCALE";

// --------------------------------------------------------------------
// HX711 driver - same bit-banging as src/scale.cpp (v2.0.20)
// --------------------------------------------------------------------
int sclPin = -1, dPin = -1;

static bool hxPresent() {
  pinMode(dPin, INPUT);
  pinMode(sclPin, OUTPUT);
  digitalWrite(sclPin, LOW);          // HX711 active (high > 60 us = power down)
  delay(2);
  return digitalRead(dPin) == LOW;    // data pending = chip alive
}

// one conversion: 24 bits MSB-first + 25th pulse keeps channel A / x128
static bool hxRaw(int32_t &v) {
  uint32_t t0 = millis();
  while (digitalRead(dPin) == HIGH) {
    if (millis() - t0 > 40) return false;
  }
  int32_t d = 0;
  for (int i = 0; i < 24; i++) {
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(1);
    digitalWrite(sclPin, LOW);
    delayMicroseconds(1);
    d = (d << 1) | (digitalRead(dPin) ? 1 : 0);
  }
  digitalWrite(sclPin, HIGH);         // 25th pulse: next read ch A / x128
  delayMicroseconds(1);
  digitalWrite(sclPin, LOW);
  if (d & 0x800000) d |= (int32_t)0xFF000000;   // 24-bit two's complement
  v = d;
  return true;
}

// ---- calibration state -------------------------------------------------
static bool   sOk      = false;
static int32_t sOffset = 0;
static float  sFactor  = 0.0f;       // raw units per gram (0 = not calibrated)
static int32_t sRaw    = 0;
static bool    sHaveRaw = false;

// stability window: last 30 samples
static int32_t sHist[30];
static uint8_t sHistN = 0, sHistIdx = 0;
static int32_t sSpread = 0;

static float gramsNow() {
  if (!sHaveRaw) return NAN;
  if (sFactor <= 0.0f) return NAN;
  return ((float)(sRaw - sOffset)) / sFactor;
}

static void cmdTare() {
  if (!sHaveRaw) { Serial.println(F("[scale] nothing to tare (no reading)")); return; }
  sOffset = sRaw;
  sSpread = 0; sHistN = 0;
  Serial.println(F("[scale] TARE done - current reading is now zero"));
}

static void cmdCalibrate(float g) {
  if (!sHaveRaw) { Serial.println(F("[scale] no reading yet")); return; }
  if (g <= 0.0f) { Serial.println(F("[scale] grams must be > 0")); return; }
  float rawAtWeight = (float)(sRaw - sOffset);
  if (rawAtWeight == 0.0f) { Serial.println(F("[scale] tare == weight: nothing on the scale?")); return; }
  sFactor = rawAtWeight / g;
  Serial.printf("[scale] CALIBRATED: factor %.3f raw/g (known weight %.1f g)\n", (double)sFactor, (double)g);
}

// --------------------------------------------------------------------
// Web + OTA
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Scale test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;background:#101418;color:#e8edf2;margin:0;padding:16px}
 h1{font-size:18px;margin:0 0 4px}.sub{color:#8a97a5;font-size:12px;margin-bottom:14px}
 .card{background:#1a2129;border:1px solid #2a3542;border-radius:10px;padding:12px 14px;margin-bottom:12px}
 .big{font-size:30px;font-weight:600}.ok{color:#4cd964}.bad{color:#ff5f57}.warn{color:#ffd60a}
 .row{display:flex;justify-content:space-between;font-size:14px;margin-top:6px;color:#b9c4cf}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:#2a3542}
</style></head><body>
<h1>Weigh scale test</h1>
<div class="sub">smart dehummidifier &middot; scale-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
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

static void refreshJson() {
  String rows;
  String big, cls;
  if (dPin < 0) {
    big = "not fitted"; cls = "";
    jRows(rows, "Note", "set TEST_SCALE_CLK / TEST_SCALE_DOUT to bench-test");
  } else if (!sOk) {
    big = "NO HX711"; cls = "bad";
    jRows(rows, "Pins", String("SCK ") + String(sclPin) + " / DOUT " + String(dPin));
    jRows(rows, "Check", "VCC/GND, SCK-DOUT not swapped, cell ribbon seated");
  } else {
    bool calm = sHistN >= 10 && sSpread < 120;   // < ~0.01-0.1 g of jitter at x128
    big = String((long)sRaw);
    cls = calm ? "ok" : "warn";
    jRows(rows, "Pins", String("SCK ") + String(sclPin) + " / DOUT " + String(dPin));
    jRows(rows, "Stability (30 s)", String((long)sSpread) + " raw spread");
    if (sFactor > 0.0f && sHaveRaw) {
      String g = String(gramsNow(), 2) + " g (tared)";
      jRows(rows, "Weight", g);
    } else {
      jRows(rows, "Calibration", "none - serial: t (tare), c 100 (cal)");
    }
  }
  String j;
  j.reserve(400);
  if (dPin < 0)                          j += "{\"verdict\":\"NOT FITTED\",\"cards\":[";
  else if (!sOk)                         j += "{\"verdict\":\"FAIL\",\"cards\":[";
  else if (sHistN < 10)                  j += "{\"verdict\":\"...\",\"cards\":[";
  else if (sSpread >= 120)               j += "{\"verdict\":\"WARN\",\"cards\":[";
  else                                   j += "{\"verdict\":\"PASS\",\"cards\":[";
  j += "{\"t\":\"HX711 + load cells\",\"big\":\"" + big + "\",\"cls\":\"" + cls + "\",\"rows\":" + rows + "]}";
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
    sclPin = 1;  dPin = 2;              // the dryer build's pins
  } else {
    sclPin = TEST_SCALE_CLK;  dPin = TEST_SCALE_DOUT;
  }

  if (dPin < 0) {
    Serial.println(F("[scale] no HX711 pins for this board - bench override in sketch header"));
  } else {
    sOk = hxPresent();
    Serial.printf("[scale] HX711 %s (SCK %d / DOUT %d)\n",
                  sOk ? "FOUND" : "NOT RESPONDING", sclPin, dPin);
    if (sOk)
      Serial.println(F("[scale] serial commands: t = tare | c <grams> = calibrate | f = show factors"));
  }

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

  // ---- serial commands --------------------------------------------------
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 't') cmdTare();
    else if (c == 'f') {
      Serial.printf("[scale] offset %ld raw, factor %.3f raw/g\n",
                    (long)sOffset, (double)sFactor);
    } else if (c == 'c') {
      float g = 0.0f;
      uint32_t t0 = millis();
      String in;
      while (millis() - t0 < 1500) {
        while (Serial.available()) {
          char d = (char)Serial.read();
          if (d == '\n' || d == '\r') break;
          if (d >= '0' && d <= '9' || d == '.') in += d;
        }
        if (in.length()) break;
      }
      g = in.toFloat();
      cmdCalibrate(g);
    }
    else if (c != '\n' && c != '\r' && c != ' ')
      Serial.println(F("[scale] commands: t | c <grams> | f"));
  }

  // ---- sampling ~2 Hz (HX711 RATE mode is 10 Hz max) ---------------------
  static uint32_t lastTry = 0, lastOk = 0;
  uint32_t now = millis();
  if (dPin >= 0 && sOk && now - lastTry >= 500) {
    lastTry = now;
    int32_t v;
    if (hxRaw(v)) {
      sRaw = v; sHaveRaw = true; lastOk = now;
      sHist[sHistIdx] = v;
      sHistIdx = (sHistIdx + 1) % 30;
      if (sHistN < 30) sHistN++;
      if (sHistN >= 2) {
        int32_t lo = sHist[0], hi = sHist[0];
        for (uint8_t i = 1; i < sHistN; i++) {
          if (sHist[i] < lo) lo = sHist[i];
          if (sHist[i] > hi) hi = sHist[i];
        }
        sSpread = hi - lo;
      }
    } else if (lastOk && now - lastOk > 10000) {
      Serial.println(F("[scale] HX711 stopped responding"));
      sOk = false;
    }
  }

  static uint32_t lastLine = 0;
  if (now - lastLine >= 1000) {
    lastLine = now;
    if (dPin >= 0 && sOk) {
      String g;
      if (sFactor > 0.0f) g = "  " + String(gramsNow(), 2) + " g";
      Serial.printf("[scale] raw %ld  spread %lu raw (30 s)%s\n",
                    (long)sRaw, (unsigned long)sSpread, g.c_str());
    }
    refreshJson();
  }
}
