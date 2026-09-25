#include "battery.h"

// Resting-voltage approximations - good enough for a dryer dashboard.
static const BatteryProfile kProfiles[] = {
  { "3S Li-ion",  12.6f,  9.90f },   // 0
  { "4S Li-ion",  16.8f, 13.20f },   // 1
  { "12V SLA",    12.9f, 11.70f },   // 2
  { "4S LiFePO4", 14.2f, 10.00f },   // 3
};

const BatteryProfile &BatteryMonitor::profile(uint8_t id) {
  if (id > 3) id = 0;
  return kProfiles[id];
}

uint8_t BatteryMonitor::percentFor(float v, uint8_t type) {
  const BatteryProfile &p = profile(type);
  float pct = (v - p.v0) / (p.v100 - p.v0) * 100.0f;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return (uint8_t)(pct + 0.5f);
}

  pinMode(PIN_VBAT_ADC, INPUT);
  analogSetPinAttenuation(PIN_VBAT_ADC, ADC_11db);   // full 0-2.45 V usable window
  if (PIN_VBAT_ENABLE >= 0) pinMode(PIN_VBAT_ENABLE, OUTPUT);
#if RELAYS_ENABLED
  pinMode(PIN_BYPASS_CTRL, OUTPUT);
  digitalWrite(PIN_BYPASS_CTRL, LOW);
#endif
  update();                                        // first reading right away
}

void BatteryMonitor::setBypass(bool on) {
  _bypass = on;
#if RELAYS_ENABLED
  digitalWrite(PIN_BYPASS_CTRL, on ? HIGH : LOW);
#endif
}

float BatteryMonitor::readVoltsOnce() {
  digitalWrite(PIN_BYPASS_CTRL, on ? HIGH : LOW);
#endif
}

float BatteryMonitor::readVoltsOnce() {
  // 16x oversample to tame the ESP32 ADC noise
  uint32_t acc = 0;
  for (int i = 0; i < 16; i++) acc += analogRead(PIN_VBAT_ADC);
  float raw = (acc / 16.0f) / 4095.0f;
  return raw * VBAT_ADC_REF * VBAT_DIV_RATIO + VBAT_CAL_OFFSET;
}

void BatteryMonitor::update() {
  uint32_t now = millis();
  if (_last != 0 && (now - _last) < BATT_PERIOD_MS) return;
  _last = now ? now : 1;

  if (PIN_VBAT_ENABLE >= 0) {
    digitalWrite(PIN_VBAT_ENABLE, HIGH);   // pull the divider down to GND
    delay(8);                              // let the ADC node settle
  }
  float v1 = readVoltsOnce();
  float v2 = readVoltsOnce();
  if (PIN_VBAT_ENABLE >= 0) digitalWrite(PIN_VBAT_ENABLE, LOW);

  _v = (v1 + v2) / 2.0f;
  _valid = _v > 1.0f;                      // below 1 V -> nothing connected
  _pct = percentFor(_v, _type);
}