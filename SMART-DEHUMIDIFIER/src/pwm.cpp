#include "pwm.h"

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
// ---------- Arduino-ESP32 core 3.x : channel handled internally ----------
void pwmInitPin(int pin, uint32_t freqHz, uint8_t resBits) {
  ledcAttach(pin, freqHz, resBits);
  ledcWrite(pin, 0);
}
void pwmWritePin(int pin, uint32_t duty) { ledcWrite(pin, duty); }

#else
// ---------- Arduino-ESP32 core 2.x / 1.x : manual channel bookkeeping ----
static const int  kMaxCh = 8;
static int        chPin[kMaxCh];
static int        chCount = 0;

static int chOf(int pin) {
  for (int i = 0; i < chCount; i++) if (chPin[i] == pin) return i;
  return -1;
}

void pwmInitPin(int pin, uint32_t freqHz, uint8_t resBits) {
  if (chOf(pin) >= 0) return;
  if (chCount >= kMaxCh) return;
  int ch = chCount++;
  chPin[ch] = pin;
  ledcSetup(ch, freqHz, resBits);
  ledcAttachPin(pin, ch);
  ledcWrite(ch, 0);
}

void pwmWritePin(int pin, uint32_t duty) {
  int ch = chOf(pin);
  if (ch >= 0) ledcWrite(ch, duty);
}
#endif