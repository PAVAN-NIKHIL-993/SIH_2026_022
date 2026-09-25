#include "dht.h"

namespace dht {

// ---- one 40-bit frame (blocking ~5 ms, timeout-bounded) ----------------
static bool frame(int8_t pin, bool is11, float &t, float &h) {
  uint8_t d[5] = {0, 0, 0, 0, 0};

  // start: pull low 2 ms, release, wait for the sensor's 80/80 us reply
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(2);
  digitalWrite(pin, HIGH);
  pinMode(pin, INPUT);              // external 10k pulls the line up
  delayMicroseconds(30);

  uint32_t t0 = micros();
  while (digitalRead(pin) == LOW)  if (micros() - t0 > 100) return false;
  t0 = micros();
  while (digitalRead(pin) == HIGH) if (micros() - t0 > 100) return false;

  // 40 bits: 50 us low then a 26-70 us high; long high = 1
  for (uint8_t i = 0; i < 40; i++) {
    t0 = micros();
    while (digitalRead(pin) == LOW)  if (micros() - t0 > 80)  return false;
    t0 = micros();
    while (digitalRead(pin) == HIGH) if (micros() - t0 > 100) return false;
    d[i / 8] <<= 1;
    if (micros() - t0 > 48) d[i / 8] |= 1;   // high was long -> bit 1
  }

  if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return false;  // checksum

  if (is11) {                       // DHT11: integer bytes + decimal byte
    h = (float)d[0] + d[1] * 0.1f;
    t = (float)d[2] + d[3] * 0.1f;
  } else {                          // DHT22/AM2302: tenths, sign bit
    h = (float)(((uint16_t)d[0] << 8) | d[1]) * 0.1f;
    float raw = (float)(((uint16_t)(d[2] & 0x7F) << 8) | d[3]) * 0.1f;
    t = (d[2] & 0x80) ? -raw : raw;
  }
  return true;
}

void Dev::update() {
  if (pin < 0) return;              // not fitted on this variant
  uint32_t now = millis();
  if (last != 0 && now - last < everyMs) return;
  last = now ? now : 1;

  float t, h;
  if (frame(pin, is11, t, h)) {
    this->t = t;  this->h = h;  miss = 0;  ok = true;  gen++;
  } else if (++miss >= 3) {         // absent/unplugged after 3 tries
    if (ok) Serial.printf("[warn] DHT on GPIO%d stopped responding\n", pin);
    ok = false;                     // keep the last values for display
  }
}

static Dev mk(int8_t pin, bool is11, uint32_t every) {
  Dev d;  d.pin = pin;  d.is11 = is11;  d.everyMs = every;  return d;
}

#if DHT_CHAMBER_ENABLED
  Dev chamber = mk(PIN_DHT22_CHAMBER, false, DHT_CHAMBER_MS);  // DHT22
#else
  Dev chamber;                                                  // off
#endif
#if DHT_OUT_ENABLED
  Dev outdoor = mk(PIN_DHT11_OUT, true, DHT_READ_MS);          // DHT11
#else
  Dev outdoor;                                                  // off
#endif

void begin() {
  if (chamber.pin >= 0) {
    pinMode(chamber.pin, INPUT);
    Serial.printf("[dht] DHT22 chamber sensor on GPIO%d (cool return)\n",
                  chamber.pin);
  }
  if (outdoor.pin >= 0) {
    pinMode(outdoor.pin, INPUT);
    Serial.printf("[dht] DHT11 outdoor sensor on GPIO%d (shade!)\n",
                  outdoor.pin);
  }
}

void update() { chamber.update(); outdoor.update(); }

}  // namespace dht