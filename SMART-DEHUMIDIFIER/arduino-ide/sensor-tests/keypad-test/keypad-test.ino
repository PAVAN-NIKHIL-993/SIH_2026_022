/**
 * KEYPAD TEST - Smart Dehumidifier (sensor suite, v2.0.20)
 * ===================================================================
 * Checks the 16-key hex keypad on its PCF8574 I2C backpack, using the
 * same row/column sweep the firmware uses (no Keypad library needed).
 *
 * Wiring (from the dryer build):
 *   classic ESP32:  PCF8574 on I2C0: SDA 21 / SCL 22, address 0x20
 *   ESP32-S3     :  PCF8574 on I2C0: SDA 8 / SCL 9, address 0x20
 *   PCF8574 board: VCC 3V3, GND, SDA, SCL; A0/A1/A2 strapped to GND
 *                  (address 0x20). Buy the PCF8574, NOT the PCF8574A
 *                  (the A is 0x38 = the AHT10's address - collision!).
 *   keypad: 16 tactile keys, rows on P0-P3, columns on P4-P7
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> key press table on your phone.
 *   3. Serial Monitor @ 115200 - press every key once; each one is
 *      printed with its menu meaning.
 *   4. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "keypad-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Key map (the on-device menu meanings):
 *   2 = UP      4 = LEFT      6 = RIGHT    8 = DOWN
 *   1 3 5 7 9 0 = digits      * = HOME     # = BACK
 *   A = ENTER/OK   B = MENU   C = MODE     D = RUN
 *
 * Verdict:  PASS = backpack answers + keys register
 *            FAIL = nothing at 0x20..0x27 (wiring / wrong chip / straps)
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
static const char *OTA_HOST  = "keypad-test";
static const char *TEST_NAME = "KEYPAD (PCF8574)";

// rows on P0-P3 (drive one LOW at a time), columns read back on P4-P7
static const char KMAP[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'},
};

static const char *keyMeaning(char k) {
  switch (k) {
    case '2': return "UP";
    case '4': return "LEFT";
    case '6': return "RIGHT";
    case '8': return "DOWN";
    case '*': return "HOME";
    case '#': return "BACK";
    case 'A': return "ENTER/OK";
    case 'B': return "MENU";
    case 'C': return "MODE";
    case 'D': return "RUN";
    case '0': case '1': case '3': case '5':
    case '7': case '9': return "digit";
    default: return "?";
  }
}

static int  sSda = -1, sScl = -1;
static uint8_t sAddr = 0x20;
static bool   sFound = false;
static bool   sOk = false;         // backpack answering recently
static uint32_t sScans = 0;
static uint32_t sKeys = 0;
static char  sLastKey = 0;
static uint32_t sLastKeyT = 0;
static uint8_t sSeenMask = 0;      // which of the 16 keys pressed (for verdict)
static bool  sSeen[16] = {false};

static uint8_t keyIndex(char k) {
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (KMAP[r][c] == k) return (uint8_t)(r * 4 + c);
  return 15;
}

static char kScan() {
  if (!sFound) return 0;
  for (uint8_t r = 0; r < 4; r++) {
    uint8_t out = 0xF0 | (uint8_t)~(1u << r);    // only row r low, cols high
    Wire.beginTransmission(sAddr);
    Wire.write(out);
    if (Wire.endTransmission() != 0) { sOk = false; return 0; }
    Wire.requestFrom(sAddr, (uint8_t)1);
    if (!Wire.available()) { sOk = false; return 0; }
    uint8_t in = (uint8_t)Wire.read();
    sOk = true;
    uint8_t cols = (in >> 4) ^ 0x0F;             // pressed column reads LOW -> 1
    if (cols) {
      for (uint8_t c = 0; c < 4; c++)
        if (cols & (1u << c)) return KMAP[r][c];
    }
  }
  return 0;
}

// --------------------------------------------------------------------
// Web + OTA
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Keypad test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;background:#101418;color:#e8edf2;margin:0;padding:16px}
 h1{font-size:18px;margin:0 0 4px}.sub{color:#8a97a5;font-size:12px;margin-bottom:14px}
 .card{background:#1a2129;border:1px solid #2a3542;border-radius:10px;padding:12px 14px;margin-bottom:12px}
 .big{font-size:30px;font-weight:600}.ok{color:#4cd964}.bad{color:#ff5f57}.warn{color:#ffd60a}
 .row{display:flex;justify-content:space-between;font-size:14px;margin-top:6px;color:#b9c4cf}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:#2a3542}
 table{border-collapse:collapse;margin-top:10px}
 td,th{border:1px solid #2a3542;padding:8px 12px;text-align:center;font-size:15px}
 th{color:#8a97a5;font-size:11px}
 .pressed{background:#1e3a2a;color:#4cd964}
</style></head><body>
<h1>Keypad test</h1>
<div class="sub">smart dehummidifier &middot; keypad-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
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
 var html=j.cards.map(card).join('');
 if(j.grid){html+='<table><tr>';
   j.grid[0].forEach(function(x){html+='<th></th>'});html+='</tr>';
   for(var r=0;r<4;r++){html+='<tr>';
     for(var c=0;c<4;c++){var k=j.grid[r+1][c];
       html+='<td class="'+(j.seen[k]?('pressed'):(''))+'">'+k+'</td>'}
     html+='</tr>'}
   html+='</table>'}
 document.getElementById('cards').innerHTML=html;
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
  if (!sFound) {
    big = "NO BACKPACK"; cls = "bad";
    jRows(rows, "Scanned", "0x20..0x27 on SDA " + String(sSda) + " / SCL " + String(sScl));
    jRows(rows, "Check", "VCC/GND, SDA/SCL, strap A0-A2 to GND, PCF8574 (not 8574A)");
  } else {
    big = sOk ? "alive" : "silent";
    cls = sOk ? "ok" : "warn";
    jRows(rows, "Address", String("0x") + String(sAddr, HEX));
    jRows(rows, "Keys registered", String(sKeys));
    if (sLastKey) jRows(rows, "Last key", String(sLastKey) + " = " + keyMeaning(sLastKey));
  }
  // grid for the 16-key pad
  String grid = "\"grid\":[[\"\",\"\",\"\",\"\"],";
  for (int r = 0; r < 4; r++) {
    grid += "[";
    for (int c = 0; c < 4; c++) grid += "\"" + String(KMAP[r][c]) + (c < 3 ? "," : "") + "\"";
    grid += "]";
    if (r < 3) grid += ",";
  }
  grid += "]";
  String seen = "\"seen\":{";
  for (int i = 0; i < 16; i++) {
    char k = KMAP[i / 4][i % 4];
    seen += "\"" + String(k) + "\":" + (sSeen[i] ? "true" : "false");
    if (i < 15) seen += ",";
  }
  seen += "}";

  String j;
  j.reserve(800);
  if (!sFound)                              j += "{\"verdict\":\"FAIL\",\"cards\":[";
  else if (sKeys >= 16)                     j += "{\"verdict\":\"PASS\",\"cards\":[";
  else if (sKeys >= 1)                      j += "{\"verdict\":\"PASS\",\"cards\":[";
  else                                      j += "{\"verdict\":\"...\",\"cards\":[";
  j += "{\"t\":\"PCF8574 backpack + 16 keys\",\"big\":\"" + big + "\",\"cls\":\"" + cls + "\",\"rows\":" + rows + "]}";
  j += "," + grid + "," + seen;
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
static void findBackpack() {
  Wire.begin(sSda, sScl, 100000);
  for (uint8_t a = 0x20; a <= 0x27; a++) {      // strap pins A0-A2 select 0x20..0x26
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      sAddr = a;
      sFound = true;
      break;
    }
  }
  if (!sFound) {
    // one more useful diagnostic: something at 0x38?
    Wire.beginTransmission(0x38);
    if (Wire.endTransmission() == 0)
      Serial.println(F("[keypad] WARNING: device at 0x38 = you have a PCF8574A or an AHT10 - wrong backpack!"));
    Serial.printf("[keypad] no PCF8574 at 0x20..0x27 (SDA %d / SCL %d)\n", sSda, sScl);
    return;
  }
  Wire.beginTransmission(sAddr);
  Wire.write(0xFF);                       // all pins high (idle)
  sOk = (Wire.endTransmission() == 0);
  Serial.printf("[keypad] PCF8574 found at 0x%02X (SDA %d / SCL %d)\n", sAddr, sSda, sScl);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==============================================");
  Serial.printf(" %s SENSOR TEST - smart dehummidifier v2.0.20\n", TEST_NAME);
  Serial.printf(" board: %s (S3: %s)\n",
                ESP.getChipModel(), HW_S3 ? "YES" : "no");

  if (HW_S3) { sSda = 8;  sScl = 9;  }
  else       { sSda = 21; sScl = 22; }

  findBackpack();
  if (sFound) Serial.println(F("[keypad] press each key - it is printed here with its menu meaning"));

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

  if (sFound) {
    static uint32_t lastScan = 0;
    static char prev = 0;
    static uint32_t tEdge = 0;
    static bool fired = false;
    uint32_t now = millis();
    if (now - lastScan >= 15) {              // same 15 ms cadence as the firmware
      lastScan = now;
      char k = kScan();
      sScans++;
      if (k != prev) { prev = k; tEdge = now; fired = false; }
      else if (k != 0 && !fired && now - tEdge >= 15) {  // stable 15 ms -> real press
        fired = true;                         // one fire per press, like the firmware
        sKeys++;
        sSeen[keyIndex(k)] = true;
        sLastKey = k; sLastKeyT = now;
        Serial.printf("[keypad] key '%c'  (%s)\n", k, keyMeaning(k));
      }
      if (millis() - sLastKeyT > 2000 && sLastKey) sLastKey = 0;   // stop repeating
    }
    static uint32_t lastJson = 0;
    if (now - lastJson >= 1000) { lastJson = now; refreshJson(); }
  }
}
