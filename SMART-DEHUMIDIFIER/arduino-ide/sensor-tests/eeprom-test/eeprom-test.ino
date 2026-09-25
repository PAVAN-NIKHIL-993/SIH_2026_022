/**
 * EEPROM TEST - Smart Dehumidifier (sensor suite, v2.0.20)
 * ===================================================================
 * Checks the AT24C256 32 kB I2C EEPROM that stores the long-term cycle
 * registry (every drying cycle's summary, ~817 slots).
 *
 * Wiring (from the dryer build):
 *   classic ESP32:  AT24C256 on I2C0: SDA 21 / SCL 22, address 0x50
 *   ESP32-S3     :  AT24C256 on I2C0: SDA 8 / SCL 9, address 0x50
 *   chip: VCC 3V3, GND, SDA, SCL; A0/A1/A2 strapped to GND (= 0x50)
 *
 * WHAT THIS TEST DOES (safe by design):
 *   - I2C ACK probe at 0x50
 *   - reads the registry HEADER (magic, record count, sequence number)
 *   - write/read/RESTORE test on byte 32744 - the first byte of the
 *     24-byte tail the registry never uses (records end at 32743).
 *     The original byte is always written back. It NEVER touches
 *     header or record data - your cycle history is untouched.
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> header + write-test result on your phone.
 *   3. Serial Monitor @ 115200, commands:
 *         h   re-read and print the registry header
 *         w   run the write/read/restore test again
 *   4. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "eeprom-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Verdict:  PASS = chip answers + header sane (or blank) + write test OK
 *            FAIL = no ACK at 0x50 (wiring / chip / straps)
 *            WARN = chip answers but the write test mismatches
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
static const char *OTA_HOST  = "eeprom-test";
static const char *TEST_NAME = "EEPROM (AT24C256)";

// chip geometry - same as src/eelog.cpp
static const uint16_t ELOG_ADDR = 0x50;
static const uint16_t TEST_OFF  = 32744;                   // first unused byte
static const uint32_t MAGIC     = 0x454C4F47;              // "ELOG"

// header layout: magic u32, ver u16, slots u16, count u16, head u16, seq u32
struct Hdr {
  uint32_t magic;
  uint16_t ver;
  uint16_t slots;
  uint16_t count;
  uint16_t head;
  uint32_t seq;
};
static Hdr sH = {};
static bool sHValid = false;
static bool sChip = false;
static bool sWriteOk = false;
static uint32_t sWriteTries = 0;

static int  sSda = -1, sScl = -1;

static bool eeProbe() {
  Wire.beginTransmission(ELOG_ADDR);
  return Wire.endTransmission() == 0;
}

// random read (n <= 28, one rep-start transaction like the firmware)
static bool eeRead(uint16_t a, uint8_t *b, uint8_t n) {
  Wire.beginTransmission(ELOG_ADDR);
  Wire.write((uint8_t)(a >> 8));
  Wire.write((uint8_t)a);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ELOG_ADDR, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) b[i] = (uint8_t)Wire.read();
  return true;
}

// random write (n <= 28)
static bool eeWrite(uint16_t a, const uint8_t *b, uint8_t n) {
  Wire.beginTransmission(ELOG_ADDR);
  Wire.write((uint8_t)(a >> 8));
  Wire.write((uint8_t)a);
  for (uint8_t i = 0; i < n; i++) Wire.write(b[i]);
  if (Wire.endTransmission() != 0) return false;
  delay(20);                     // max AT24C256 write cycle time
  return true;
}

static void readHeader() {
  uint8_t b[16] = {0};
  if (!sChip || !eeRead(0, b, 16)) { sHValid = false; return; }
  sH.magic = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
  sH.ver   = (uint16_t)(b[4] | (b[5] << 8));
  sH.slots = (uint16_t)(b[6] | (b[7] << 8));
  sH.count = (uint16_t)(b[8] | (b[9] << 8));
  sH.head  = (uint16_t)(b[10] | (b[11] << 8));
  sH.seq   = (uint32_t)b[12] | ((uint32_t)b[13] << 8) | ((uint32_t)b[14] << 16) | ((uint32_t)b[15] << 24);
  sHValid = (sH.magic == MAGIC);
  Serial.printf("[eeprom] header: %s (ver %u, %u records, head %u, seq %u)\n",
                sHValid ? "VALID" : "BLANK/JUNK (firmware auto-formats blank chips)",
                (unsigned)sH.ver, (unsigned)sH.count, (unsigned)sH.head, (unsigned)sH.seq);
}

static void writeTest() {
  sWriteTries++;
  uint8_t orig = 0xFF;
  eeRead(TEST_OFF, &orig, 1);
  uint8_t pat = (uint8_t)(0xA0 + (sWriteTries % 8));
  if (!eeWrite(TEST_OFF, &pat, 1)) {
    Serial.println(F("[eeprom] WRITE FAILED (no ACK)"));
    sWriteOk = false;
    return;
  }
  uint8_t back = 0;
  eeRead(TEST_OFF, &back, 1);
  bool ok = (back == pat);
  eeWrite(TEST_OFF, &orig, 1);        // always restore the original byte
  delay(20);
  uint8_t final_ = 0;
  eeRead(TEST_OFF, &final_, 1);
  if (!ok) { sWriteOk = false; return; }
  sWriteOk = (final_ == orig);
  Serial.printf("[eeprom] write test #%u: 0x%02X written, read back 0x%02X, restored 0x%02X -> %s\n",
                (unsigned)sWriteTries, pat, back, final_, sWriteOk ? "OK" : "RESTORE MISMATCH!");
}

// --------------------------------------------------------------------
// Web + OTA
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EEPROM test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;background:#101418;color:#e8edf2;margin:0;padding:16px}
 h1{font-size:18px;margin:0 0 4px}.sub{color:#8a97a5;font-size:12px;margin-bottom:14px}
 .card{background:#1a2129;border:1px solid #2a3542;border-radius:10px;padding:12px 14px;margin-bottom:12px}
 .big{font-size:30px;font-weight:600}.ok{color:#4cd964}.bad{color:#ff5f57}.warn{color:#ffd60a}
 .row{display:flex;justify-content:space-between;font-size:14px;margin-top:6px;color:#b9c4cf}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:#2a3542}
</style></head><body>
<h1>EEPROM test (cycle registry)</h1>
<div class="sub">smart dehummidifier &middot; eeprom-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
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
  String r1, r2;
  String b1, b2, c1, c2;

  if (sChip) {
    b1 = "0x50 found"; c1 = "ok";
    jRows(r1, "Pins", String("SDA ") + String(sSda) + " / SCL " + String(sScl));
    jRows(r1, "Capacity", "32768 bytes, 64-byte pages");
  } else {
    b1 = "NO CHIP"; c1 = "bad";
    jRows(r1, "Pins", String("SDA ") + String(sSda) + " / SCL " + String(sScl));
    jRows(r1, "Check", "VCC/GND, SDA/SCL, strap A0-A2 to GND");
  }

  if (!sChip) {
    b2 = "n/a"; c2 = "bad";
    jRows(r2, "Note", "chip missing");
  } else if (sWriteOk) {
    b2 = "OK"; c2 = "ok";
    if (sHValid) {
      jRows(r2, "Registry header", "VALID (ELOG magic)");
      jRows(r2, "Records stored", String(sH.count) + " of " + String(sH.slots));
    } else {
      jRows(r2, "Registry header", "blank/junk - firmware formats it on first run");
    }
    jRows(r2, "Write test", String(sWriteTries) + " x at offset 32744 (unused tail, always restored)");
  } else {
    b2 = "FAIL"; c2 = "bad";
    jRows(r2, "Write test", "mismatch - chip half-dead? try another AT24C256");
  }

  String j;
  j.reserve(600);
  if (!sChip)                 j += "{\"verdict\":\"FAIL\",\"cards\":[";
  else if (!sWriteOk)         j += "{\"verdict\":\"WARN\",\"cards\":[";
  else                        j += "{\"verdict\":\"PASS\",\"cards\":[";
  j += "{\"t\":\"AT24C256 presence\",\"big\":\"" + b1 + "\",\"cls\":\"" + c1 + "\",\"rows\":" + r1 + "},";
  j += "{\"t\":\"header + write/restore test\",\"big\":\"" + b2 + "\",\"cls\":\"" + c2 + "\",\"rows\":" + r2 + "]}";
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

  if (HW_S3) { sSda = 8;  sScl = 9;  }
  else       { sSda = 21; sScl = 22; }
  Wire.begin(sSda, sScl, 100000);

  sChip = eeProbe();
  if (sChip) {
    Serial.printf("[eeprom] AT24C256 present at 0x%02X (SDA %d / SCL %d)\n", ELOG_ADDR, sSda, sScl);
    readHeader();
    writeTest();
    Serial.println(F("[eeprom] serial commands: h = re-read header | w = run write test"));
  } else {
    Serial.printf("[eeprom] nothing at 0x50 (SDA %d / SCL %d) - chip absent or unstrapped\n", sSda, sScl);
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

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'h' && sChip) readHeader();
    else if (c == 'w' && sChip) writeTest();
  }

  static uint32_t lastJson = 0;
  if (millis() - lastJson >= 1000) { lastJson = millis(); refreshJson(); }
}