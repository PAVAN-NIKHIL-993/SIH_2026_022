/**
 * SUPPLY SELECTOR TEST - Smart Dehumidifier (sensor suite, v2.0.20)
 * ===================================================================
 * Checks the two power-source inputs the dryer uses to know which
 * feed is selected and which one is actually live:
 *
 *   1. SOLAR TOGGLE   - physical toggle switch requesting SOLAR or
 *                        BYPASS mode (pull-up input)
 *   2. SUPPLY OPTO    - optocoupler input: the SELECTED feed is live
 *                        (HIGH = present in this build)
 *
 * Wiring (from the dryer build):
 *   classic ESP32:  TOGGLE = GPIO 27, OPTO = GPIO 35
 *   ESP32-S3     :  TOGGLE = GPIO 39, OPTO = GPIO 18
 *   toggle:  one wire GPIO -> toggle, other wire -> GND
 *            (toggle to GND = BYPASS, pulled up to 3V3 = SOLAR)
 *   opto:    the optocoupler's collector output -> GPIO,
 *            emitter -> GND (active high = feed live)
 *
 * SAFETY: this test READS both inputs only. It never drives the
 * 2-channel feed relay (on the S3 that is GPIO 6/7) - driving it
 * would switch the chamber's power feed.
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> live toggle + opto state on your phone.
 *   3. Serial Monitor @ 115200 for the consistency verdict.
 *   4. Flip the toggle; watch the opto line follow (the selected feed
 *      going live/dead) - that is what the firmware checks before it
 *      switches supplies.
 *   5. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "supply-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Verdict:  PASS = toggle seen + opto responds to it
 *            FAIL = either input stuck (wiring)
 *            WARN = opto never goes live with either toggle position
 *                   (dead feed / opto wiring)
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
static const char *OTA_HOST  = "supply-test";
static const char *TEST_NAME = "SUPPLY SELECTOR";

#define OPTO_ACTIVE_HIGH 1       // as in config.h: HIGH = supply present

static int  sTogglePin = -1, sOptoPin = -1;
static bool sSolarReq = false;   // toggle: true = SOLAR requested
static bool sOptoLive = false;   // selected feed is live
static uint32_t sToggleEdges = 0, sOptoEdges = 0;
static bool  sToggleSeen = false, sOptoSeenLive = false, sOptoSeenDead = false;

// --------------------------------------------------------------------
// Web + OTA
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Supply test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;background:#101418;color:#e8edf2;margin:0;padding:16px}
 h1{font-size:18px;margin:0 0 4px}.sub{color:#8a97a5;font-size:12px;margin-bottom:14px}
 .card{background:#1a2129;border:1px solid #2a3542;border-radius:10px;padding:12px 14px;margin-bottom:12px}
 .big{font-size:30px;font-weight:600}.ok{color:#4cd964}.bad{color:#ff5f57}.warn{color:#ffd60a}
 .row{display:flex;justify-content:space-between;font-size:14px;margin-top:6px;color:#b9c4cf}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:#2a3542}
</style></head><body>
<h1>Supply selector test</h1>
<div class="sub">smart dehummidifier &middot; supply-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
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
  String r1, r2, v1, v2, c1, c2;

  v1 = sSolarReq ? "SOLAR" : "BYPASS";
  c1 = "ok";
  jRows(r1, "Pin", String("GPIO ") + String(sTogglePin) + " (pull-up)");
  jRows(r1, "Changes", String(sToggleEdges));
  if (!sToggleSeen) { c1 = "warn"; jRows(r1, "Note", "no change seen yet - flip the toggle"); }

  v2 = sOptoLive ? "LIVE" : "DEAD";
  c2 = sOptoLive ? "ok" : "warn";
  jRows(r2, "Pin", String("GPIO ") + String(sOptoPin));
  jRows(r2, "Changes", String(sOptoEdges));
  if (!sOptoSeenLive && !sOptoSeenDead) jRows(r2, "Note", "no change seen yet");
  else if (!sOptoSeenLive) { c2 = "bad"; jRows(r2, "Note", "never went live - dead feed or opto wiring"); }

  String j;
  j.reserve(600);
  if (sToggleSeen && sOptoSeenLive)                    j += "{\"verdict\":\"PASS\",\"cards\":[";
  else if (!sToggleSeen && !sOptoSeenLive)             j += "{\"verdict\":\"...\",\"cards\":[";
  else if (!sOptoSeenLive && sOptoSeenDead)            j += "{\"verdict\":\"WARN\",\"cards\":[";
  else                                                 j += "{\"verdict\":\"WARN\",\"cards\":[";
  j += "{\"t\":\"solar toggle (mode request)\",\"big\":\"" + v1 + "\",\"cls\":\"" + c1 + "\",\"rows\":" + r1 + "},";
  j += "{\"t\":\"supply optocoupler (feed live)\",\"big\":\"" + v2 + "\",\"cls\":\"" + c2 + "\",\"rows\":" + r2 + "]}";
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

  if (HW_S3)      { sTogglePin = 39; sOptoPin = 18; }
  else            { sTogglePin = 27; sOptoPin = 35; }

  pinMode(sTogglePin, INPUT_PULLUP);   // exactly as supply::begin() does
  pinMode(sOptoPin, INPUT);
  sSolarReq = (digitalRead(sTogglePin) == HIGH);
  sOptoLive = (digitalRead(sOptoPin) == (OPTO_ACTIVE_HIGH ? HIGH : LOW));
  if (sOptoLive) sOptoSeenLive = true; else sOptoSeenDead = true;

  Serial.printf("[supply] toggle on GPIO %d: %s requested\n",
                sTogglePin, sSolarReq ? "SOLAR" : "BYPASS");
  Serial.printf("[supply] opto on GPIO %d: selected feed %s\n",
                sOptoPin, sOptoLive ? "LIVE" : "NOT LIVE");
  Serial.println(F("[supply] flip the toggle - opto should follow the selected feed"));

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

  bool nowSolar = (digitalRead(sTogglePin) == HIGH);
  if (nowSolar != sSolarReq) {
    sSolarReq = nowSolar;
    sToggleEdges++;
    sToggleSeen = true;
    Serial.printf("[supply] toggle -> %s MODE requested\n", sSolarReq ? "SOLAR" : "BYPASS");
  }

  bool nowLive = (digitalRead(sOptoPin) == (OPTO_ACTIVE_HIGH ? HIGH : LOW));
  if (nowLive != sOptoLive) {
    sOptoLive = nowLive;
    sOptoEdges++;
    if (nowLive) sOptoSeenLive = true; else sOptoSeenDead = true;
    Serial.printf("[supply] opto -> selected feed is now %s\n", nowLive ? "LIVE" : "NOT LIVE");
  }

  static uint32_t lastLine = 0, lastJson = 0;
  uint32_t now = millis();
  if (now - lastLine >= 5000) {
    lastLine = now;
    Serial.printf("[supply] mode: %s | feed: %s | toggle changes: %lu | opto changes: %lu\n",
                  sSolarReq ? "SOLAR" : "BYPASS", sOptoLive ? "LIVE" : "NOT LIVE",
                  (unsigned long)sToggleEdges, (unsigned long)sOptoEdges);
  }
  if (now - lastJson >= 1000) { lastJson = now; refreshJson(); }
}