/**
 * BUTTONS TEST - Smart Dehumidifier (sensor suite, v2.0.20)
 * ===================================================================
 * Checks BUTTON-1 (master power button) and BUTTON-2 (default
 * automation) with press duration measurement.
 *
 * Wiring (from the dryer build):
 *   classic ESP32:  BTN1 = GPIO 15, BTN2 = GPIO 18
 *   ESP32-S3     :  BTN1 = GPIO 15, BTN2 = GPIO 16
 *   each button:   one wire GPIO -> button, other wire -> GND
 *                  (pull-up input, press = LOW)
 *
 * SAFETY / WHAT IS DIFFERENT FROM THE FIRMWARE:
 *   In the real firmware, holding BUTTON-1 for 3 s cuts the WHOLE
 *   system (P-MOSFET latch) and 10 s reboots. THIS TEST DOES NOT DO
 *   THAT - it only watches the pins, so you can hold BUTTON-1 as long
 *   as you like while testing. It still asserts the latch keep-alive
 *   pin (so a wired-up harness stays powered through the test):
 *     classic: HOLD = GPIO 16, S3: HOLD = GPIO 14.
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> live button states on your phone.
 *   3. Serial Monitor @ 115200 - press and hold each button; the
 *      release line shows the press duration (the firmware judges
 *      short vs 3 s vs 10 s holds from exactly this number).
 *   4. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "buttons-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Verdict:  PASS = both buttons registered at least one press
 *            WARN = only one (or neither) seen yet
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

#define AP_SSID   "AgarbattiDryer"
#define AP_PASS   "dryer1234"
#define AP_CHAN   6
#define AP_MAXCL  4
static const char *OTA_HOST  = "buttons-test";
static const char *TEST_NAME = "BUTTONS";

struct Btn {
  int pin = -1;
  const char *label = "";
  bool pressed = false;
  uint32_t presses = 0;
  uint32_t longestMs = 0;
  uint32_t lastDur = 0;
  uint32_t pressStart = 0;
};

static Btn b1, b2;
static int sHoldPin = -1;

static void btnTick(Btn &b) {
  bool p = (digitalRead(b.pin) == LOW);   // active low (to GND), like the firmware
  if (p && !b.pressed) {
    b.pressed = true;
    b.pressStart = millis();
  } else if (!p && b.pressed) {
    b.pressed = false;
    b.presses++;
    b.lastDur = millis() - b.pressStart;
    if (b.lastDur > b.longestMs) b.longestMs = b.lastDur;
    Serial.printf("[%s] released - held %lu ms (press #%lu)\n",
                  b.label, (unsigned long)b.lastDur, (unsigned long)b.presses);
  }
}

// --------------------------------------------------------------------
// Web + OTA
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Buttons test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;background:#101418;color:#e8edf2;margin:0;padding:16px}
 h1{font-size:18px;margin:0 0 4px}.sub{color:#8a97a5;font-size:12px;margin-bottom:14px}
 .card{background:#1a2129;border:1px solid #2a3542;border-radius:10px;padding:12px 14px;margin-bottom:12px}
 .big{font-size:30px;font-weight:600}.ok{color:#4cd964}.bad{color:#ff5f57}.warn{color:#ffd60a}
 .row{display:flex;justify-content:space-between;font-size:14px;margin-top:6px;color:#b9c4cf}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:#2a3542}
</style></head><body>
<h1>Buttons test</h1>
<div class="sub">smart dehummidifier &middot; buttons-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
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

static void cardBtn(Btn &b, String &j, bool comma) {
  String rows;
  String big = b.pressed ? "HELD" : "released";
  String cls = b.pressed ? "warn" : (b.presses ? "ok" : "");
  jRows(rows, "Pin", String("GPIO ") + String(b.pin));
  jRows(rows, "Presses", String(b.presses));
  if (b.pressed) {
    jRows(rows, "Holding now", String((millis() - b.pressStart) / 1000UL) + " s");
    if (b.pressed && (millis() - b.pressStart) >= 3000)
      jRows(rows, "Note", "firmware would hard-power-off the system at 3 s - this test does not");
  } else {
    jRows(rows, "Last hold", String(b.lastDur) + " ms");
    jRows(rows, "Longest", String(b.longestMs) + " ms");
  }
  j += "{\"t\":\"" + String(b.label) + "\",\"big\":\"" + big + "\",\"cls\":\"" + cls + "\",\"rows\":" + rows + "}";
  if (comma) j += ",";
}

static void refreshJson() {
  String j;
  j.reserve(500);
  if (b1.presses && b2.presses)   j += "{\"verdict\":\"PASS\",\"cards\":[";
  else if (b1.presses || b2.presses) j += "{\"verdict\":\"WARN\",\"cards\":[";
  else                             j += "{\"verdict\":\"...\",\"cards\":[";
  cardBtn(b1, j, true);
  cardBtn(b2, j, false);
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

  if (HW_S3) { b1.pin = 15; b2.pin = 16; sHoldPin = 14; }
  else       { b1.pin = 15; b2.pin = 18; sHoldPin = 16; }
  b1.label = "BUTTON-1 (master power)";
  b2.label = "BUTTON-2 (default automation)";

  // keep the soft-latch alive while we test (the harness stays powered)
  if (sHoldPin >= 0) {
    pinMode(sHoldPin, OUTPUT);
    digitalWrite(sHoldPin, HIGH);
  }
  pinMode(b1.pin, INPUT_PULLUP);
  pinMode(b2.pin, INPUT_PULLUP);

  Serial.printf("[btn] BUTTON-1 on GPIO %d, BUTTON-2 on GPIO %d (both to GND)\n", b1.pin, b2.pin);
  Serial.printf("[btn] hold pin %d asserted - holding BUTTON-1 here does NOT power off\n", sHoldPin);
  Serial.println(F("[btn] press either button and release - the duration is printed"));

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

  btnTick(b1);
  btnTick(b2);

  static uint32_t lastJson = 0;
  uint32_t now = millis();
  if (now - lastJson >= 500) { lastJson = now; refreshJson(); }
}
