/**
 * DS1302 RTC TEST - Smart Dehumidifier (sensor suite, v2.0.21)
 * ===================================================================
 * Checks the DS1302 real-time clock with the SAME bit-bang driver the
 * firmware uses (src/rtc.cpp - no RTC library needed): auto-detect
 * (scratch-RAM magic probe), WP/CH flag handling, BCD read, write-back
 * and the local-time <-> epoch conversion.
 *
 * Wiring (from the dryer build, variants/esp32-s3/config-s3.h):
 *   DS1302 module -> ESP32-S3 :
 *     VCC -> 3V3, GND -> GND, SCLK -> GPIO 40, I/O -> GPIO 42, RST -> GPIO 47
 *     (the module's BZ pin is unused; its CR2032 stays ON the module)
 *   classic ESP32: no DS1302 in the dryer build - to bench-test a spare
 *     module on the classic, set the three pins below (defaults -1 = off)
 *
 * Use:
 *   1. Flash over USB (this single file - only the ESP32 core).
 *   2. Join WiFi "AgarbattiDryer" (password: dryer1234), open
 *      http://192.168.4.1 -> live chip status on your phone.
 *   3. Press "SET TIME FROM PHONE" -> your browser's clock is written
 *      into the chip; the page then reads it back and shows both.
 *   4. Serial Monitor @ 115200 -> status line + every register.
 *   5. OTA (always present): while this test runs, Arduino IDE
 *      Tools > Port > "rtc-test at 192.168.4.1" lets you upload
 *      ANY sketch in this folder - or the main dryer firmware -
 *      straight over WiFi. No USB needed afterwards.
 *
 * Verdict:  PASS = chip detected AND time valid (and settable)
 *            WARN = chip detected, no valid time yet (fresh module -
 *                   press SET TIME, or check the coin cell)
 *            FAIL = no chip on the bus (check VCC/GND/3 wires)
 *            NOT FITTED = pins are -1 (classic bench override unset)
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

// ---- pins ----------------------------------------------------------------
// S3: EXACTLY the dryer build pins (config-s3.h). Classic: bench override,
// defaults -1 (dryer build has no RTC fitted on the classic).
#if HW_S3
  #define PIN_RTC_RST    40
  #define PIN_RTC_SCLK   42
  #define PIN_RTC_IO     47
#else
  #define PIN_RTC_RST    (-1)
  #define PIN_RTC_SCLK   (-1)
  #define PIN_RTC_IO     (-1)
#endif

static int16_t tzMinutes = 330;           // default IST; /tz updates it

#define AP_SSID   "AgarbattiDryer"
#define AP_PASS   "dryer1234"
#define AP_CHAN   6
#define AP_MAXCL  4
static const char *OTA_HOST  = "rtc-test";
static const char *TEST_NAME = "DS1302 RTC";

// --------------------------------------------------------------------
// DS1302 driver - same logic as src/rtc.cpp (v2.0.21)
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

static inline void ceHiLo(bool hi) { digitalWrite(PIN_RTC_RST, hi ? HIGH : LOW); }
static inline void clkHiLo(bool hi) { digitalWrite(PIN_RTC_SCLK, hi ? HIGH : LOW); }
static void bitWrite(bool b) {
  digitalWrite(PIN_RTC_IO, b ? HIGH : LOW);
  clkHiLo(true);
  clkHiLo(false);
}
static bool bitRead() {
  clkHiLo(true);
  clkHiLo(false);                              // falling edge: chip updates
  return (digitalRead(PIN_RTC_IO) == HIGH);
}
static void byteWrite(uint8_t v) {
  pinMode(PIN_RTC_IO, OUTPUT);
  for (int i = 0; i < 8; i++) bitWrite((v >> i) & 1);
}
static uint8_t byteRead() {
  pinMode(PIN_RTC_IO, INPUT);
  uint8_t v = 0;
  for (int i = 0; i < 8; i++) if (bitRead()) v |= (uint8_t)(1u << i);
  return v;
}
static void xfer(uint8_t addr, const uint8_t *data, uint8_t n, uint8_t *out) {
  ceHiLo(false);
  ceHiLo(true);                                   // chip select (CE = RST)
  bitWrite(false);                                // START
  byteWrite(addr);
  if (addr & 0x01) {                              // R/W bit (bit0): 1 = read
    for (uint8_t i = 0; i < n; i++) out[i] = byteRead();
  } else {
    for (uint8_t i = 0; i < n; i++) byteWrite(data[i]);
  }
  bitWrite(true);                                 // STOP
  ceHiLo(false);
}
static bool sane(const uint8_t *t) {
  return bcd2bin(t[0]) < 60 && bcd2bin(t[1]) < 60
      && bcd2bin(t[2] & 0x1F) <= 23
      && bcd2bin(t[4]) >= 1 && bcd2bin(t[4]) <= 31
      && bcd2bin(t[5]) >= 1 && bcd2bin(t[5]) <= 12
      && bcd2bin(t[6]) >= 19 && bcd2bin(t[6]) <= 99;
}

static bool sPresent = false;
static bool sTrusted = false;
static uint8_t sRegs[8];                          // last read (for the page)

static bool rtcBegin() {
  sPresent = false;
  sTrusted = false;
  if (PIN_RTC_RST < 0 || PIN_RTC_SCLK < 0 || PIN_RTC_IO < 0) return false;
  pinMode(PIN_RTC_RST, OUTPUT);  ceHiLo(false);
  pinMode(PIN_RTC_SCLK, OUTPUT); clkHiLo(false);
  pinMode(PIN_RTC_IO, OUTPUT);   digitalWrite(PIN_RTC_IO, LOW);
  {
    uint8_t zero = 0x00;
    xfer(0x8E, &zero, 1, nullptr);               // clear WP (power-on state
  }                                              // is undefined per datasheet)
  uint8_t orig = 0, magic = 0xA5, back = 0;
  xfer(0xC1, nullptr, 1, &orig);
  xfer(0xC0, &magic, 1, nullptr);
  xfer(0xC1, nullptr, 1, &back);
  xfer(0xC0, &orig, 1, nullptr);                 // give the byte back
  sPresent = (back == magic);
  if (!sPresent) return false;
  {
    uint8_t a[7];
    xfer(0x81, nullptr, 7, a);
    sTrusted = sane(a) && !(a[0] & 0x80);        // CH flag (seconds bit7)
  }
  return true;
}

static bool rtcReadTime(time_t *outEp) {
  if (!sPresent) return false;
  uint8_t t[7];
  xfer(0x81, nullptr, 7, t);
  if (!sane(t)) return false;
  memcpy(sRegs, t, 7);
  sRegs[7] = 0; xfer(0x8F, nullptr, 1, &sRegs[7]);
  int year = 2000 + bcd2bin(t[6]);
  int hour;
  if (t[2] & 0x80) {                              // 12-hour mode
    hour = bcd2bin(t[2] & 0x1F);
    if (t[2] & 0x20) hour = (hour == 12) ? 12 : hour + 12;
    else            hour = (hour == 12) ? 0  : hour;
  } else
    hour = bcd2bin(t[2] & 0x1F);
  time_t ep = civilToEpoch(year, bcd2bin(t[5]), bcd2bin(t[4]),
                           hour, bcd2bin(t[1]), bcd2bin(t[0]));
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
  uint8_t dow = dowFromCivil(y, mo, d);
  uint8_t t[7] = {
    bin2bcd((uint8_t)s), bin2bcd((uint8_t)mi),
    bin2bcd((uint8_t)(h & 0x1F)),
    bin2bcd(dow), bin2bcd((uint8_t)d), bin2bcd((uint8_t)mo),
    bin2bcd((uint8_t)((y >= 2000 ? y - 2000 : y) % 100)),
  };
  uint8_t zero = 0x00;
  xfer(0x8E, &zero, 1, nullptr);                  // clear WP before the write
  xfer(0x80, t, 7, nullptr);                      // burst; clears CH too
  sTrusted = true;
  return true;
}

static String fmtLocal(time_t ep) {
  time_t local = ep + (time_t)tzMinutes * 60;
  int y, mo, d, h, mi, s;
  epochToCivil(local, &y, &mo, &d, &h, &mi, &s);
  char b[32];
  snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d:%02d", y, mo, d, h, mi, s);
  return String(b);
}

// --------------------------------------------------------------------
// Web + OTA
// --------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DS1302 RTC test</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,sans-serif;background:#101418;color:#e8edf2;margin:0;padding:16px}
 h1{font-size:18px;margin:0 0 4px}.sub{color:#8a97a5;font-size:12px;margin-bottom:14px}
 .card{background:#1a2129;border:1px solid #2a3542;border-radius:10px;padding:12px 14px;margin-bottom:12px}
 .big{font-size:26px;font-weight:600}.ok{color:#4cd964}.bad{color:#ff5f57}.warn{color:#ffd60a}
 .row{display:flex;justify-content:space-between;font-size:14px;margin-top:6px;color:#b9c4cf}
 .tag{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;background:#2a3542}
 button{margin-top:10px;background:#2f6fed;border:0;color:#fff;padding:9px 14px;border-radius:8px;font-size:14px}
 button:disabled{background:#3a4553;color:#8a97a5}
</style></head><body>
<h1>DS1302 RTC test</h1>
<div class="sub">smart dehummidifier &middot; rtc-test &middot; refreshes 1/s &middot; verdict <span id="st" class="tag">...</span></div>
<div id="cards">loading...</div>
<script>
function card(c){var h='<div class="card"><div class="big '+(c.cls||'')+'">'+c.big+'</div>';
 h+='<div style="color:#8a97a5;font-size:12px">'+c.t+'</div>';
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
  j.reserve(700);
  time_t ep = 0;
  bool have = rtcReadTime(&ep);
  const char *verdict;
  String rows, big, cls;

  if (PIN_RTC_RST < 0) {
    verdict = "NOT FITTED";
    big = "no pins"; cls = "";
    jRows(rows, "Board", HW_S3 ? "S3 (pins fixed)" : "classic ESP32");
    jRows(rows, "Note", "set the three PIN_RTC_* defines to bench-test a spare module");
  } else if (!sPresent) {
    verdict = "FAIL";
    big = "NO CHIP"; cls = "bad";
    jRows(rows, "Probe", "scratch-RAM magic 0xA5 not echoed back");
    jRows(rows, "Check", "VCC->3V3, GND->GND, SCLK->40, I/O->42, RST->47");
  } else if (have) {
    verdict = "PASS";
    big = fmtLocal(ep); cls = "ok";
    char regs[64];
    snprintf(regs, sizeof(regs), "sec=%02X min=%02X hr=%02X dow=%02X date=%02X mon=%02X yr=%02X ctrl=%02X",
             sRegs[0], sRegs[1], sRegs[2], sRegs[3], sRegs[4], sRegs[5], sRegs[6], sRegs[7]);
    jRows(rows, "Chip registers (BCD)", String(regs));
    jRows(rows, "CH (halt) flag", (sRegs[0] & 0x80) ? "SET (clock stopped)" : "clear (ticking)");
    jRows(rows, "WP (write-protect)", (sRegs[7] & 0x80) ? "SET" : "clear");
    jRows(rows, "Timezone", String(tzMinutes / 60) + (tzMinutes % 60 ? "+" + String(tzMinutes % 60) : "") + " h (UTC)");
  } else {
    verdict = "WARN";
    big = "present, no valid time"; cls = "warn";
    jRows(rows, "State", "fresh module or halted clock - press SET TIME FROM PHONE");
    jRows(rows, "Note", "chip is detected; its registers are not a sane time yet");
  }
  if (sPresent && PIN_RTC_RST >= 0) jRows(rows, "Last set", gLastSetMsg);

  j += "{\"verdict\":\"" + String(verdict) + "\",\"cards\":[{\"t\":\"DS1302 module (RST 40 / SCLK 42 / I/O 47)\",";
  j += "\"big\":\"" + big + "\",\"cls\":\"" + cls + "\",";
  j += (sPresent && !have && !gSetBusy) ? "\"btn\":true," : "";
  j += "\"rows\":" + rows + "]}]}";
  gJson = j;
}

static void handleSet() {
  if (!web.hasArg("epoch")) { web.send(200, "application/json", "{\"msg\":\"epoch?\"}"); return; }
  if (PIN_RTC_RST < 0 || !sPresent) { web.send(200, "application/json", "{\"msg\":\"no DS1302 detected\"}"); return; }
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
           ok ? (readBack && back == ep ? "written + verified" : "written, re-read mismatch")
              : "rejected (junk epoch)");
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
  Serial.printf(" %s TEST - smart dehummidifier v2.0.21\n", TEST_NAME);
  Serial.printf(" board: %s (S3: %s)\n",
                ESP.getChipModel(), HW_S3 ? "YES" : "no");
  Serial.printf(" pins: RST=%d SCLK=%d IO=%d  tz=UTC+%d\n",
                PIN_RTC_RST, PIN_RTC_SCLK, PIN_RTC_IO, tzMinutes / 60);

  bool found = rtcBegin();
  time_t ep = 0;
  if (!found) Serial.println("[rtc] NO DS1302 on the bus");
  else if (rtcReadTime(&ep))
    Serial.printf("[rtc] DS1302 OK - %s (local, tz %d)\n", fmtLocal(ep).c_str(), tzMinutes);
  else Serial.println("[rtc] DS1302 present, no valid time yet (fresh module)");

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

  static uint32_t lastLine = 0;
  uint32_t now = millis();
  if (now - lastLine >= 2000) {
    lastLine = now;
    time_t ep = 0;
    if (PIN_RTC_RST < 0) Serial.println("[rtc] not fitted (pins -1)");
    else if (!sPresent)  Serial.println("[rtc] no DS1302 on the bus");
    else if (rtcReadTime(&ep)) Serial.printf("[rtc] %s (local, tz %d)\n", fmtLocal(ep).c_str(), tzMinutes);
    else Serial.println("[rtc] present, no valid time yet");
    refreshJson();
  }
}
