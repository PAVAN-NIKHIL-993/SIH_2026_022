#include "sensors.h"
#include "config.h"

bool SensorModule::begin() {
  Wire.begin(PIN_I2C0_SDA, PIN_I2C0_SCL);
  _s1.begin(Wire, 0x38, SENSOR_PERIOD_MS);   // top of chamber
#if SENS2_DHT
  // chamber source #2 = DHT22 on the cool-return path;
  // dht::update() (main loop) drives its frames.
#else
  Wire1.begin(PIN_I2C1_SDA, PIN_I2C1_SCL);
  _s2.begin(Wire1, 0x38, SENSOR_PERIOD_MS);  // bottom of chamber
#endif
  recompute();
  return anyOk();
}

void SensorModule::update() {
  bool n1 = _s1.update(millis());
#if SENS2_DHT
  static uint32_t lastGen = 0;
  bool n2 = (dht::chamber.gen != lastGen);   // a new DHT22 frame arrived
  if (n2) lastGen = dht::chamber.gen;
#else
  bool n2 = _s2.update(millis());
#endif
  if (n1 || n2) recompute();
}

void SensorModule::recompute() {
  float tSum = 0, hSum = 0; int n = 0;
  float tM = -300, hM = -300;
  if (s1ok()) {
    tSum += t1(); hSum += h1(); n++;
    tM = max(tM, t1()); hM = max(hM, h1());
  }
  if (s2ok()) {
    tSum += t2(); hSum += h2(); n++;
    tM = max(tM, t2()); hM = max(hM, h2());
  }
  if (n > 0) {
    _tAvg = tSum / n; _hAvg = hSum / n;
    _tMax = tM;       _hMax = hM;
  } else {
    _tAvg = NAN; _hAvg = NAN; _tMax = NAN; _hMax = NAN;
  }
}
