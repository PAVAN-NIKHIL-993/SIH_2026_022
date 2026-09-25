/**
 * DS1307 RTC TEST - Smart Dehumidifier (sensor suite, v2.0.23)
 * ===================================================================
 * Checks the DS1307 real-time clock with the SAME I2C driver logic the
 * firmware uses (src/rtc.cpp - Wire only, no RTC library): detection at
 * 0x68, CH (clock-halt) flag, BCD + 12/24 h decoding, write-back and the
 * local-time <-> epoch conversion. Also scans the whole I2C bus and names
 * what it finds (DS1307, the AT24C32 EEPROM many RTC boards carry, AHT10,
 * PCF8574 keypad, AT24C256 registry).
 *
 * Wiring (exactly the dryer build - the RTC shares the I2C0 bus):
 *   DS1307 module -> ESP32-S3 :   VCC -> 5V   GND -> GND
 *                                 SDA -> GPIO 8   SCL -> GPIO 9
 *   classic ESP32:                SDA -> GPIO 21  SCL -> GPIO 22
 *   !! The DS1307 needs 4.5-5.5 V on VCC (at 3.3 V it ignores the bus).
 *   !! Most DS1307 boards ("Tiny RTC") pull SDA/SCL up to 5 V through
 *      R2/R3 - ESP32 pins are NOT 5 V tolerant: remove R2 + R3 (or use an
 *      I2C level shifter). The DS1307 reads 3.3 V logic fine.
 *   !! Tiny RTC + a plain CR2032: remove its LIR2032 charger (D1, R4, R5)
 *      and bridge R6 - or keep the rechargeable LIR2032 it came with.
 *   DS3231 boards (3.3 V-native) use the same registers and pass too.
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> live chip status + bus scan on your phone.
 *   3. Press "SET TIME FROM PHONE" -> your browser's clock is written
 *      into the chip; the page then reads it back and shows both.
 *   4. Serial Monitor @ 115200 -> status line + every register.
 *   5. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "rtc-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Verdict:  PASS = chip detected AND time valid (and settable)
 *            WARN = chip detected, clock halted / not set yet (fresh
 *                   module - press SET TIME, or check the coin cell)
 *            FAIL = nothing answers at 0x68 (VCC 5 V? SDA/SCL swapped?)
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

// ---- pins: EXACTLY the dryer build's I2C0 (config-s3.h / config.h) -------
#if HW_S3
  #define PIN_I2C0_SDA   8
  #define PIN_I2C0_SCL   9
#else
  #define PIN_I2C0_SDA   21
  #define PIN_I2C0_SCL   22
#endif
#define RTC_ADDR 0x68

static int16_t tzMinutes = 330;           // default IST; /set updates it

#define AP_SSID   "AgarbattiDryer"
#define AP_PASS   "dryer1234"
#define AP_CHAN   6
#define AP_MAXCL  4
static const char *OTA_HOST  = "rtc-test";
static const char *TEST_NAME = "DS1307 RTC";

// --------------------------------------------------------------------
// DS1307 driver - same logic as src/rtc.cpp (v2.0.23)
// --------------------------------------------------------------------
static long daysSinceEpoch(int y, int mo, int d) {
  int yy = y - (mo <= 2 ? 1 : 0);
  int era = (yy >= 0 ? yy : yy - 399) / 400;
  int yoe = yy - era * 400;
  int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (long)era * 146097L + doe - 719468;
}
static time_t civilToEpoch(int y, int mo, int d, int h, int mi, int s) {
  return daysSinceEpoch(y, mo, d) * 86400L + h * 3600L + mi * 60L + s;
}
static uint8_t dowFromCivil(int y, int mo, int d) {
  long days = daysSinceEpoch(y, mo, d);
  return (uint8_t)(((days % 7) + 7 + 4) % 7 + 1);
}
static void epochToCivil(time_t ep, int *y, int *mo, int *d,
                         int *h, int *mi, int *s) {
  long days = ep / 86400;
  int rem = (int)(ep - days * 86400);
  *h = rem / 3600;  rem %= 3600;
  *mi = rem / 60;   *s = rem % 60;
  days += 719468;
  int era = (days >= 0 ? days : days - 146096) / 146097;
  int doe = (int)(days - era * 146097);
  int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int y_ = yoe + era * 400;
  int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int mp = (5 * doy + 2) / 153;
  *d = doy - (153 * mp + 2) / 5 + 1;
  *mo = mp + (mp < 10 ? 3 : -9);
  *y = y_ + (*mo <= 2 ? 1 : 0);
}
static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
static bool bcdOk(uint8_t v) { return (v & 0x0F) <= 9 && (v >> 4) <= 9; }

static bool readRegs(uint8_t *t, uint8_t n) {   // n registers from 0x00
  Wire.beginTransmission(RTC_ADDR);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)RTC_ADDR, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) t[i] = (uint8_t)Wire.read();
  return true;
}
static bool writeRegs(const uint8_t *t) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write((uint8_t)0x00);
  for (uint8_t i = 0; i < 7; i++) Wire.write(t[i]);
  return Wire.endTransmission() == 0;
}
static int hour24(uint8_t hr) {
  if (hr & 0x40) {                               // 12-hour mode
    int h = bcd2bin(hr & 0x1F);
    if (hr & 0x20) return (h == 12) ? 12 : h + 12;
    return (h == 12) ? 0 : h;
  }
  return bcd2bin(hr & 0x3F);                     // 24-hour mode (bit5 = 20s)
}
static bool sane(const uint8_t *t) {
  uint8_t sec = t[0] & 0x7F, min = t[1] & 0x7F, mon = t[5] & 0x1F;
  uint8_t hr = (t[2] & 0x40) ? (t[2] & 0x1F) : (t[2] & 0x3F);
  if (!bcdOk(sec) || !bcdOk(min) || !bcdOk(hr) || !bcdOk(t[4]) ||
      !bcdOk(mon) || !bcdOk(t[6])) return false;
  bool hourOk = (t[2] & 0x40) ? (bcd2bin(hr) >= 1 && bcd2bin(hr) <= 12)
                              : (bcd2bin(hr) <= 23);
  return bcd2bin(sec) < 60 && bcd2bin(min) < 60 && hourOk
      && bcd2bin(t[4]) >= 1 && bcd2bin(t[4]) <= 31
      && bcd2bin(mon) >= 1 && bcd2bin(mon) <= 12
      && bcd2bin(t[6]) >= 19 && bcd2bin(t[6]) <= 99;
}

static bool sPresent = false;
static uint8_t sRegs[8];                          // last read (for the page)

static bool rtcBegin() {
  Wire.beginTransmission(RTC_ADDR);
  sPresent = (Wire.endTransmission() == 0);
  return sPresent;
}

static bool rtcReadTime(time_t *outEp) {
  if (!sPresent) return false;
  uint8_t t[8];
  if (!readRegs(t, 8)) return false;             // time + control register
  memcpy(sRegs, t, 8);
  if ((t[0] & 0x80) || !sane(t)) return false;   // halted / junk
  time_t ep = civilToEpoch(2000 + bcd2bin(t[6]), bcd2bin(t[5] & 0x1F),
                           bcd2bin(t[4]), hour24(t[2]),
                           bcd2bin(t[1] & 0x7F), bcd2bin(t[0] & 0x7F));
  ep -= (time_t)tzMinutes * 60;                   // chip holds LOCAL time
  *outEp = ep;
  return true;
}

// write an explicit epoch (from the phone) into the chip
static bool rtcWriteEpoch(time_t ep) {
  if (!sPresent) return false;
  if (ep <= (time_t)1700000000) return false;     // refuse junk
  int y, mo, d, h, mi, s;
  epochToCivil(ep + (time_t)tzMinutes * 60, &y, &mo, &d, &h, &mi, &s);
  uint8_t t[7] = {
    bin2bcd((uint8_t)s),                          // CH = 0: oscillator runs
    bin2bcd((uint8_t)mi),
    bin2bcd((uint8_t)h),                          // 24-hour mode
    dowFromCivil(y, mo, d),
    bin2bcd((uint8_t)d), bin2bcd((uint8_t)mo),
    bin2bcd((uint8_t)((y >= 2000 ? y - 2000 : y) % 100)),
  };
  return writeRegs(t);
}

static String fmtLocal(time_t ep) {
  time_t local = ep + (time_t)tzMinutes * 60;
  int y, mo, d, h, mi, s;
  epochToCivil(local, &y, &mo, &d, &h, &mi, &s);
  char b[32];
  snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d:%02d", y, mo, d, h, mi, s);
  return String(b);
}

// ---- I2C bus scan: name what answers --------------------------------------
static const char *devName(uint8_t a) {
  if (a == 0x68) return "DS1307 / DS3231 RTC";
  if (a == 0x50) return "24Cxx EEPROM (RTC board AT24C32 or AT24C256 registry)";
  if (a >= 0x51 && a <= 0x57) return "24Cxx EEPROM (0x57 = DS3231 board AT24C32)";
  if (a == 0x38) return "AHT10 chamber sensor";
  if (a >= 0x20 && a <= 0x27) return "PCF8574 keypad / expander";
  if (a >= 0x38 && a <= 0x3F) return "PCF8574A (clashes with AHT10!)";
  return "unknown";
}
static String gScan = "not scanned";
static uint8_t gScanN = 0;
static void scanBus() {
  String out;
  gScanN = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      char b[80];
      snprintf(b, sizeof(b), "%s0x%02X %s", gScanN ? " &middot; " : "", a, devName(a));
      out += b;
      gScanN++;
    }
  }
  gScan = gScanN ? out : String("nothing answered - check SDA/SCL, GND and the pull-ups");
  Serial.printf("[i2c] %u device(s): %s\n", (unsigned)gScanN, gScan.c_str());
}

// --------------------------------------------------------------------
// Web + OTA  (royal blue + golden brown, like the dryer website)
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DS1307 RTC test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;margin:0;padding:16px;color:#fff8ea;min-height:100vh;
  background:#1b45b5 linear-gradient(180deg,#2352c9 0%,#1d48b8 45%,#3560c8 62%,#c4914a 76%,#8a5a14 100%) fixed}
 h1{font-size:18px;margin:0 0 4px;color:#f6dd9c}.sub{color:#d6def4;font-size:12px;margin-bottom:14px}
 .card{background:rgba(19,48,142,.88);border:1px solid rgba(233,196,106,.35);border-radius:12px;padding:12px 14px;margin-bottom:12px;
  box-shadow:0 8px 20px rgba(6,16,58,.35)}
 .big{font-size:26px;font-weight:600}.ok{color:#5ee89a}.bad{color:#ff8a8a}.warn{color:#f7c552}
 .row{display:flex;justify-content:space-between;gap:12px;font-size:14px;margin-top:6px;color:#d6def4}
 .row span:last-child{text-align:right;color:#fff8ea}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:rgba(8,24,80,.6)}
 button{margin-top:10px;background:linear-gradient(135deg,#f6dd9c,#b8862b);border:0;color:#2b1a02;font-weight:700;
  padding:9px 14px;border-radius:8px;font-size:14px}
 button:disabled{background:rgba(8,24,80,.5);color:#b2c0e4}
</style></head><body>
<h1>DS1307 RTC test</h1>
<div class="sub">smart dehumidifier &middot; rtc-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
<div id="cards">loading...</div>
<script>
function card(c){var h='<div class="card"><div class="big '+(c.cls||'')+'">'+c.big+'</div>';
 h+='<div style="color:#d6def4;font-size:12px">'+c.t+'</div>';
 (c.rows||[]).forEach(function(r){h+='<div class="row"><span>'+r[0]+'</span><span>'+r[1]+'</span></div>'});
 if(c.btn)h+='<button onclick="setnow()">SET TIME FROM PHONE</button>';
 return h+'</div>'}
async function setnow(){
  var r=await fetch('/set?epoch='+Math.floor(Date.now()/1000)+'&tz='+
    (new Date().getTimezoneOffset()*-1));
  var j=await r.json();alert(j.msg);}
async function tick(){try{
 var j=await (await fetch('/data')).json();
 var st=document.getElementById('st');st.textContent=j.verdict;
 st.className='tag '+(j.verdict==='PASS'?'ok':(j.verdict.indexOf('FAIL')===0?'bad':'warn'));
 document.getElementById('cards').innerHTML=j.cards.map(card).join('');
}catch(e){}setTimeout(tick,1000)}
tick();
</script>
</body></html>
)rawliteral";

WebServer web(80);
String gJson = "{}";
bool gSetBusy = false;
String gLastSetMsg = "not set yet";

static void jRows(String &rows, const char *k, const String &v) {
  if (rows.length()) rows += ",";
  rows += "[";
  rows += "\"";  rows += k;     rows += "\",";
  rows += "\"";  rows += v;     rows += "\"";
  rows += "]";
}

static void refreshJson() {
  String j;
  j.reserve(1400);
  time_t ep = 0;
  if (!sPresent) rtcBegin();                      // hot-plug: re-probe
  bool have = rtcReadTime(&ep);
  const char *verdict;
  String rows, big, cls;
  char pins[48];
  snprintf(pins, sizeof(pins), "SDA %d / SCL %d, VCC 5 V", PIN_I2C0_SDA, PIN_I2C0_SCL);

  if (!sPresent) {
    verdict = "FAIL";
    big = "NO CHIP"; cls = "bad";
    jRows(rows, "Probe", "nothing ACKs at 0x68");
    jRows(rows, "Check", String(pins) + " (the DS1307 ignores the bus at 3.3 V)");
    jRows(rows, "Pull-ups", "5 V pull-ups on the module (R2/R3)? remove them / level shifter");
  } else if (have) {
    verdict = "PASS";
    big = fmtLocal(ep); cls = "ok";
    char regs[80];
    snprintf(regs, sizeof(regs), "sec=%02X min=%02X hr=%02X dow=%02X date=%02X mon=%02X yr=%02X ctrl=%02X",
             sRegs[0], sRegs[1], sRegs[2], sRegs[3], sRegs[4], sRegs[5], sRegs[6], sRegs[7]);
    jRows(rows, "Chip registers (BCD)", String(regs));
    jRows(rows, "CH (clock halt)", "clear (ticking)");
    jRows(rows, "Hour mode", (sRegs[2] & 0x40) ? "12 h (firmware re-writes 24 h on set)" : "24 h");
    int tzA = abs((int)tzMinutes);
    jRows(rows, "Timezone", String("UTC") + (tzMinutes < 0 ? "-" : "+") + String(tzA / 60) +
                            (tzA % 60 ? ":" + String(tzA % 60) : String("")));
  } else {
    verdict = "WARN";
    big = "found, clock not set"; cls = "warn";
    jRows(rows, "CH (clock halt)", (sRegs[0] & 0x80) ? "SET - oscillator stopped (fresh module / dead coin cell)" : "clear");
    jRows(rows, "State", "press SET TIME FROM PHONE");
  }
  if (sPresent) jRows(rows, "Last set", gLastSetMsg);
  jRows(rows, "I2C bus", gScan);

  j += "{\"verdict\":\"" + String(verdict) + "\",\"cards\":[{\"t\":\"DS1307 module (I2C 0x68, " + String(pins) + ")\",";
  j += "\"big\":\"" + big + "\",\"cls\":\"" + cls + "\",";
  j += (sPresent && !gSetBusy) ? "\"btn\":true," : "";
  j += "\"rows\":[" + rows + "]}]}";
  gJson = j;
}

static void handleSet() {
  if (!web.hasArg("epoch")) { web.send(200, "application/json", "{\"msg\":\"epoch?\"}"); return; }
  if (!sPresent) { web.send(200, "application/json", "{\"msg\":\"no DS1307 detected\"}"); return; }
  time_t ep = (time_t)web.arg("epoch").toInt();
  if (web.hasArg("tz")) tzMinutes = constrain(web.arg("tz").toInt(), -720, 840);
  gSetBusy = true;
  bool ok = rtcWriteEpoch(ep);
  delay(50);
  time_t back = 0;
  bool readBack = rtcReadTime(&back);
  gSetBusy = false;
  char msg[64];
  snprintf(msg, sizeof(msg), "%s",
           ok ? (readBack && back >= ep && back <= ep + 2 ? "written + verified" : "written, re-read mismatch")
              : "rejected (junk epoch or bus error)");
  gLastSetMsg = String(msg) + " @ " + fmtLocal(ep) + " local";
  Serial.printf("[rtc] SET from phone: %s\n", msg);
  String out = "{\"msg\":\"" + String(msg) + "\"}";
  web.send(200, "application/json", out);
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
  Serial.printf(" %s TEST - smart dehumidifier v2.0.23\n", TEST_NAME);
  Serial.printf(" board: %s (S3: %s)\n",
                ESP.getChipModel(), HW_S3 ? "YES" : "no");
  Serial.printf(" I2C0: SDA=%d SCL=%d @100 kHz  RTC 0x%02X  tz=UTC%+d min\n",
                PIN_I2C0_SDA, PIN_I2C0_SCL, RTC_ADDR, tzMinutes);

  Wire.begin(PIN_I2C0_SDA, PIN_I2C0_SCL, 100000);   // DS1307 max = 100 kHz
  scanBus();
  bool found = rtcBegin();
  time_t ep = 0;
  if (!found) Serial.println("[rtc] NO DS1307 at 0x68 (VCC must be 5 V; check SDA/SCL/GND)");
  else if (rtcReadTime(&ep))
    Serial.printf("[rtc] DS1307 OK - %s (local, tz %d)\n", fmtLocal(ep).c_str(), tzMinutes);
  else Serial.println("[rtc] DS1307 found, clock not set yet (fresh module / coin cell)");

  web.on("/", []() { web.send_P(200, "text/html", INDEX_HTML); });
  web.on("/data", []() { web.send(200, "application/json", gJson); });
  web.on("/set", []() { handleSet(); });
  web.begin();
  netBegin();
  Serial.println("==============================================");
  refreshJson();
}

void loop() {
  ArduinoOTA.handle();          // OTA lives in every test sketch
  web.handleClient();

  static uint32_t lastLine = 0, lastScan = 0;
  uint32_t now = millis();
  if (now - lastScan >= 10000) { lastScan = now; scanBus(); }
  if (now - lastLine >= 2000) {
    lastLine = now;
    time_t ep = 0;
    if (!sPresent && !rtcBegin()) Serial.println("[rtc] no DS1307 at 0x68");
    else if (rtcReadTime(&ep)) Serial.printf("[rtc] %s (local, tz %d)\n", fmtLocal(ep).c_str(), tzMinutes);
    else Serial.println("[rtc] found, clock not set yet");
    refreshJson();
  }
}
