/**
 * BATTERY SENSE TEST - Smart Dehumidifier (sensor suite, v2.0.20)
 * ===================================================================
 * Checks the battery voltage divider + ADC, exactly as the firmware
 * samples it (16 x oversample, low-side transistor gated).
 *
 * Wiring (from the dryer build):
 *   battery(+) --[100k]--+----> ADC pin
 *                        |
 *                      [15k]
 *                        |
 *   ENABLE pin --[1k]--| GATE  (2N7000 N-MOSFET, or NPN + 1k)
 *                      | D
 *                      +-- divider bottom node
 *                      S ---> GND
 *   classic ESP32:  ADC = GPIO 36, ENABLE = GPIO 19
 *   ESP32-S3     :  ADC = GPIO 4,  ENABLE = GPIO 5
 *
 * NOTE: the divider sits on the BATTERY rail - with only USB power and
 * no battery connected the reading is ~0 V. That is EXPECTED here, not
 * a fault.
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> live volts + % on your phone.
 *   3. Serial Monitor @ 115200, command:
 *         s <0-3>   set chemistry: 0=3S Li-ion  1=4S Li-ion
 *                   2=12V SLA      3=4S LiFePO4 (dryer default)
 *   4. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "battery-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Verdict:  PASS = volts > 1 V and stable (small sample-to-sample drift)
 *            FAIL = ~0 V (battery not connected to the divider)
 *            WARN = volts present but jittery (grounding / cable)
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
static const char *OTA_HOST  = "battery-test";
static const char *TEST_NAME = "BATTERY SENSE";

// same divider as config.h
static const float VBAT_DIV_RATIO = ((100000.0f + 15000.0f) / 15000.0f); // 7.667
static const float VBAT_ADC_REF   = 3.30f;

// same chemistry tables as src/battery.cpp
struct Profile { const char *name; float v100; float v0; };
static const Profile kProfiles[] = {
  { "3S Li-ion",  12.6f,  9.90f },
  { "4S Li-ion",  16.8f, 13.20f },
  { "12V SLA",    12.9f, 11.70f },
  { "4S LiFePO4", 14.2f, 10.00f },
};

static int  sAdcPin = -1, sEnPin = -1;
static uint8_t sType = 3;              // dryer default: 4S LiFePO4
static float  sVolts = 0.0f;
static uint16_t sRawAvg = 0;
static bool  sValid = false;
static uint16_t sJitter = 0;           // max-min of the 32 last samples

static uint8_t pctFor(float v) {
  const Profile &p = kProfiles[sType > 3 ? 0 : sType];
  float pct = (v - p.v0) / (p.v100 - p.v0) * 100.0f;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (uint8_t)(pct + 0.5f);
}

static uint16_t sampleRaw() {
  uint32_t acc = 0;
  for (int i = 0; i < 16; i++) acc += analogRead(sAdcPin);
  return (uint16_t)(acc / 16);
}

static void sample() {
  if (sEnPin >= 0) {
    digitalWrite(sEnPin, HIGH);        // pull the divider down to GND
    delay(8);                          // let the ADC node settle
  }
  uint16_t a = sampleRaw();
  uint16_t b = sampleRaw();
  if (sEnPin >= 0) digitalWrite(sEnPin, LOW);

  sRawAvg = (uint16_t)((a + b) / 2);
  sVolts = (sRawAvg / 4095.0f) * VBAT_ADC_REF * VBAT_DIV_RATIO;
  sValid = sVolts > 1.0f;

  static uint16_t hist[32];
  static uint8_t n = 0, idx = 0;
  hist[idx] = sRawAvg;
  idx = (idx + 1) % 32;
  if (n < 32) n++;
  if (n >= 4) {
    uint16_t lo = hist[0], hi = hist[0];
    for (uint8_t i = 1; i < n; i++) {
      if (hist[i] < lo) lo = hist[i];
      if (hist[i] > hi) hi = hist[i];
    }
    sJitter = hi - lo;
  }
}

// --------------------------------------------------------------------
// Web + OTA
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Battery test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;background:#101418;color:#e8edf2;margin:0;padding:16px}
 h1{font-size:18px;margin:0 0 4px}.sub{color:#8a97a5;font-size:12px;margin-bottom:14px}
 .card{background:#1a2129;border:1px solid #2a3542;border-radius:10px;padding:12px 14px;margin-bottom:12px}
 .big{font-size:30px;font-weight:600}.ok{color:#4cd964}.bad{color:#ff5f57}.warn{color:#ffd60a}
 .row{display:flex;justify-content:space-between;font-size:14px;margin-top:6px;color:#b9c4cf}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:#2a3542}
</style></head><body>
<h1>Battery sense test</h1>
<div class="sub">smart dehummidifier &middot; battery-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
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
  const Profile &p = kProfiles[sType > 3 ? 0 : sType];
  big = String(sVolts, 2) + " V";
  if (sValid) {
    bool calm = sJitter < 20;                 // 20 ADC counts ~ 8 mV on the divider
    cls = calm ? "ok" : "warn";
    jRows(rows, "Percent (" + String(p.name) + ")", String(pctFor(sVolts)) + " %");
    jRows(rows, "Raw ADC", String(sRawAvg) + " of 4095");
    jRows(rows, "Jitter (32 s)", String(sJitter) + " counts");
    jRows(rows, "Divider", String("x" + String(VBAT_DIV_RATIO, 3)) + "  (100k/15k)");
    jRows(rows, "Pins", String("ADC ") + String(sAdcPin) + (sEnPin >= 0 ? String(" / EN ") + String(sEnPin) : ""));
  } else {
    cls = "bad";
    big = "0.00 V";
    jRows(rows, "Note", "battery not connected to the divider (USB alone is expected to read ~0)");
    jRows(rows, "Pins", String("ADC ") + String(sAdcPin) + (sEnPin >= 0 ? String(" / EN ") + String(sEnPin) : ""));
  }
  String j;
  j.reserve(400);
  if (!sValid)                j += "{\"verdict\":\"FAIL\",\"cards\":[";
  else if (sJitter >= 20)     j += "{\"verdict\":\"WARN\",\"cards\":[";
  else                        j += "{\"verdict\":\"PASS\",\"cards\":[";
  j += "{\"t\":\"battery divider + ADC\",\"big\":\"" + big + "\",\"cls\":\"" + cls + "\",\"rows\":" + rows + "]}";
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

  if (HW_S3)      { sAdcPin = 4;  sEnPin = 5; }
  else            { sAdcPin = 36; sEnPin = 19; }

  pinMode(sAdcPin, INPUT);
  analogSetPinAttenuation(sAdcPin, ADC_11db);   // full 0-2.45 V window
  if (sEnPin >= 0) pinMode(sEnPin, OUTPUT);

  Serial.printf("[batt] divider x%.3f on ADC GPIO %d, enable GPIO %d\n",
                (double)VBAT_DIV_RATIO, sAdcPin, sEnPin);
  Serial.println(F("[batt] serial command: s <0-3>  (0=3S Li-ion 1=4S Li-ion 2=12V SLA 3=4S LiFePO4)"));

  web.on("/", []() { web.send_P(200, "text/html", INDEX_HTML); });
  web.on("/data", []() { web.send(200, "application/json", gJson); });
  web.begin();
  netBegin();
  Serial.println("==============================================");
  sample();
  refreshJson();
}

void loop() {
  ArduinoOTA.handle();          // OTA lives in every test sketch
  web.handleClient();

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c >= '0' && c <= '3') {
      sType = (uint8_t)(c - '0');
      Serial.printf("[batt] chemistry: %s\n", kProfiles[sType].name);
    }
  }

  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 1000) {
    last = now;
    sample();
    Serial.printf("[batt] %.2f V   %u %% (%s)   raw %u   jitter %u\n",
                  (double)sVolts, (unsigned)pctFor(sVolts), kProfiles[sType].name,
                  (unsigned)sRawAvg, (unsigned)sJitter);
    refreshJson();
  }
}