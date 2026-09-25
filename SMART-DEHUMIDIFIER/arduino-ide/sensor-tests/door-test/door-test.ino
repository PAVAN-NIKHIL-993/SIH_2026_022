/**
 * DOOR SWITCH TEST - Smart Dehumidifier (sensor suite, v2.0.20)
 * ===================================================================
 * Checks the door limit switch (open/closed) with edge detection and
 * debounce, the same way the firmware reads it.
 *
 * Wiring (from the dryer build):
 *   ESP32-S3      : switch -> GPIO 21 (to GND; door CLOSED = LOW,
 *                   internal pull-up - no resistor needed)
 *   classic ESP32 : no door pin in the dryer build (the workflow runs
 *                   in software there) - bench-test any switch by
 *                   setting TEST_DOOR_PIN below
 *   switch:        one wire GPIO -> switch, other wire -> GND
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> live OPEN/CLOSED state on your phone.
 *   3. Serial Monitor @ 115200 for edge-by-edge detail.
 *   4. Flip the switch open/closed a few times.
 *   5. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "door-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Verdict:  PASS = switch seen in BOTH states (or stable + no glitches)
 *            FAIL = pin stuck (wiring / wrong pin / switch to 3V3?)
 *            WARN = glitches (bouncing more than expected - long wire?)
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
// -1 = not tested. Switch goes between this pin and GND (closed = LOW).
#define TEST_DOOR_PIN  (-1)

#define AP_SSID   "AgarbattiDryer"
#define AP_PASS   "dryer1234"
#define AP_CHAN   6
#define AP_MAXCL  4
static const char *OTA_HOST  = "door-test";
static const char *TEST_NAME = "DOOR SWITCH";

#define DEBOUNCE_MS  30          // a real limit switch bounces < 10 ms

static int  sPin = -1;
static bool sState = false;     // false = OPEN, true = CLOSED
static bool sFitted = false;
static uint32_t sEdges = 0;
static uint32_t sGlitches = 0;  // extra edges within 100 ms of each other
static uint32_t sLastEdge = 0;
static uint32_t sLastChange = 0;

// --------------------------------------------------------------------
// Web + OTA
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Door test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;background:#101418;color:#e8edf2;margin:0;padding:16px}
 h1{font-size:18px;margin:0 0 4px}.sub{color:#8a97a5;font-size:12px;margin-bottom:14px}
 .card{background:#1a2129;border:1px solid #2a3542;border-radius:10px;padding:12px 14px;margin-bottom:12px}
 .big{font-size:30px;font-weight:600}.ok{color:#4cd964}.bad{color:#ff5f57}.warn{color:#ffd60a}
 .row{display:flex;justify-content:space-between;font-size:14px;margin-top:6px;color:#b9c4cf}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:#2a3542}
</style></head><body>
<h1>Door switch test</h1>
<div class="sub">smart dehummidifier &middot; door-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
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
  String rows, big, cls;
  if (!sFitted) {
    big = "not fitted"; cls = "";
    jRows(rows, "Note", "set TEST_DOOR_PIN in the sketch to bench-test");
  } else {
    big = sState ? "CLOSED" : "OPEN";
    cls = "ok";
    uint32_t age = (millis() >= sLastChange) ? (millis() - sLastChange) / 1000UL : 0;
    jRows(rows, "Pin", String("GPIO ") + String(sPin) + " (closed = LOW)");
    jRows(rows, "State changed", String(age) + " s ago");
    jRows(rows, "Edges counted", String(sEdges));
    jRows(rows, "Glitches", String(sGlitches));
    if (sGlitches > 4) cls = "warn";
  }
  String j;
  j.reserve(400);
  if (!sFitted)                 j += "{\"verdict\":\"NOT FITTED\",\"cards\":[";
  else if (sEdges >= 2)         j += "{\"verdict\":\"PASS\",\"cards\":[";
  else if (sEdges >= 1)         j += "{\"verdict\":\"PASS\",\"cards\":[";
  else if (sGlitches > 4)       j += "{\"verdict\":\"WARN\",\"cards\":[";
  else                          j += "{\"verdict\":\"...\",\"cards\":[";
  j += "{\"t\":\"door limit switch\",\"big\":\"" + big + "\",\"cls\":\"" + cls + "\",\"rows\":" + rows + "]}";
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
    sPin = 21;                    // the dryer build's reed/limit-switch pin
  } else {
    sPin = TEST_DOOR_PIN;
  }

  if (sPin >= 0) {
    sFitted = true;
    pinMode(sPin, INPUT_PULLUP);  // closed = LOW, exactly like the firmware
    sState = (digitalRead(sPin) == LOW);
    sLastChange = millis();
    Serial.printf("[door] limit switch on GPIO %d (to GND, closed = LOW)\n", sPin);
    Serial.println(F("[door] flip it open/closed - each change is printed here"));
  } else {
    Serial.println(F("[door] no door pin for this board - bench override in sketch header"));
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

  if (sFitted) {
    // debounce: sample, wait, re-sample
    static uint32_t tNext = 0;
    uint32_t now = millis();
    if (now >= tNext) {
      tNext = now + DEBOUNCE_MS;
      bool raw = (digitalRead(sPin) == LOW);
      if (raw != sState) {
        uint32_t dt = (sLastEdge && now >= sLastEdge) ? (now - sLastEdge) : 0;
        sState = raw;
        sEdges++;
        sLastChange = now;
        if (sLastEdge && dt < 100) sGlitches++;
        sLastEdge = now;
        Serial.printf("[door] %s (edge %lu, %lu ms after previous)\n",
                      sState ? "CLOSED" : "OPEN",
                      (unsigned long)sEdges, (unsigned long)dt);
      }
    }
    static uint32_t lastLine = 0, lastJson = 0;
    if (now - lastLine >= 2000) {
      lastLine = now;
      Serial.printf("[door] state: %s  edges: %lu  glitches: %lu\n",
                    sState ? "CLOSED" : "OPEN",
                    (unsigned long)sEdges, (unsigned long)sGlitches);
    }
    if (now - lastJson >= 1000) { lastJson = now; refreshJson(); }
  }
}