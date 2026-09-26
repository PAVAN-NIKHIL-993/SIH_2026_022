#include "txdisp.h"
#include "config.h"

#if TXDISP_ENABLED

#include "control.h"
#include "sensors.h"
#include "battery.h"
#include "scale.h"
#include "door.h"
#include "supply.h"

// TX-only UART to the companion Arduino (RX pin -1 = transmit only)
#define BRIDGE Serial1

namespace txdisp {

void begin() {
  BRIDGE.begin(TXDISP_BAUD, SERIAL_8N1, -1, PIN_TXDISP_TX);
  Serial.printf("[txdisp] display bridge up: TX GPIO%d @ %u baud -> Arduino\n",
                PIN_TXDISP_TX, (unsigned)TXDISP_BAUD);
}

void update() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (last != 0 && now - last < 1000) return;   // one burst per second
  last = now ? now : 1;

  const char *st  = stateName(dryer.state());
  const char *why = dryer.faultWhy();
  float g = (scale.ok() && !isnan(scale.grams())) ? scale.grams() : 0.0f;

  BRIDGE.printf("$ST,%s\n", st);
  BRIDGE.printf("$T,%.1f,%.1f\n", (double)sensors.t1(), (double)sensors.t2());
  BRIDGE.printf("$H,%.0f,%.0f\n", (double)sensors.h1(), (double)sensors.h2());
  BRIDGE.printf("$SET,%.0f,%u\n", (double)cfg.setTemp, (unsigned)cfg.dryMinutes);
  BRIDGE.printf("$E,%lu,%lu\n",
                (unsigned long)dryer.elapsedS(), (unsigned long)dryer.remainingS());
  BRIDGE.printf("$P,%u,%u,%.2f,%u\n", dryer.heatDuty(), dryer.fanOutDuty(),
                (double)battery.volts(), battery.percent());
  BRIDGE.printf("$W,%.0f,%.0f,%.0f\n", (double)g, (double)cfg.targetG,
                (double)(g - cfg.targetG));
  BRIDGE.printf("$D,%s,%d,%.0f\n", door::phase(), door::closed() ? 1 : 0,
                (double)door::batchG());
  BRIDGE.printf("$S,%s,%d,%u\n", supply::modeName(),
                supply::optoLive() ? 1 : 0, (unsigned)cfg.mode);
  BRIDGE.printf("$F,%s\n", (why && why[0]) ? why : "");
  BRIDGE.printf("$V,%s\n", FW_VERSION);
}

}  // namespace txdisp

#else

namespace txdisp {
void begin() {}
void update() {}
}

#endif
