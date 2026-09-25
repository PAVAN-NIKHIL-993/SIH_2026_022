/**
 * @file pixel.h
 * @brief On-board RGB status pixel (WS2812/NeoPixel) - zero libraries.
 *
 * The DevKitC-1's RGB LED is a WS2812 on ONE data pin (v1.0: GPIO48,
 * v1.1: GPIO38 - check the silkscreen; 48 is this build's default and
 * stays free). Driven through the hardware SPI peripheral at 6.4 MHz:
 * every WS2812 bit becomes 8 SPI bits (2 high = "0", 5 high = "1"),
 * which gives rock-solid timing without any NeoPixel library. The SPI
 * bus is free - the display is gone (website is the screen).
 *
 * Status law (owner spec "indication"):
 *   boot        blue
 *   door open   yellow            (load your trays)
 *   IDLE        dim cyan pulse
 *   RUNNING     green breathing   (boost heat-up: orange blink)
 *   battery<20% yellow blink      (while running)
 *   PURGING     teal blink
 *   DONE        SOLID GREEN       = the spec's "completion LED"
 *   FAULT       fast red blink
 * PIXEL_COUNT > 1 chains an external strip on the same data pin.
 */
#pragma once
#include <Arduino.h>
#include "config.h"

namespace pixel {
  void begin();      // SPI up on PIN_PIXEL, LED off
  void update();     // call ~10 Hz from loop: state -> colour
  bool rePin(int);   // move the pixel to another data pin (48/38) at runtime
}