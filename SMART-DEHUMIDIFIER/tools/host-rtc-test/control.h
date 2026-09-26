#pragma once
// the bits of the real control.h / config.h that src/rtc.cpp uses
#include <Arduino.h>
#ifndef RTC_ENABLED
#define RTC_ENABLED 1
#endif
#define RTC_I2C_ADDR 0x68
#define PIN_I2C0_SDA 8
#define PIN_I2C0_SCL 9
struct Settings { int tzMinutes; };
extern Settings cfg;
