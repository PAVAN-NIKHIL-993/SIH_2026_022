/**
 * @file pwm.h
 * @brief Tiny LEDC wrapper that compiles on BOTH Arduino-ESP32 core 2.x
 *        (ledcSetup/ledcAttachPin) and 3.x (ledcAttach), so the sketch
 *        builds in old Arduino IDE, new Arduino IDE and PlatformIO.
 */
#pragma once
#include <Arduino.h>

void pwmInitPin(int pin, uint32_t freqHz, uint8_t resBits);
void pwmWritePin(int pin, uint32_t duty);   // 0 .. (2^resBits - 1)