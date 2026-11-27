/**
 * @file menu.h
 * @brief v2.0 on-device menu: keypad navigation + parameter entry.
 *
 * Key map (spec): 2=UP 4=LEFT 6=RIGHT 8=DOWN, 1/3/5/7/9/0 digits,
 * * = HOME, # = BACK, A = ENTER/OK, B = MENU, C = MODE, D = RUN.
 *
 * The state machine lives here (works with or without a display - every
 * action is also echoed on serial); the TFT renderer in display.cpp
 * reads the getters when menu::active().
 */
#pragma once
#include <Arduino.h>

namespace menu {
  bool key(char k);             // feed one keypad key; true = consumed
  bool active();                // a menu/edit screen is up
  bool editing();               // value-entry screen
  bool info();                  // OTA firmware-update info screen
  uint8_t cursor();             // selected row in the list
  uint8_t itemCount();
  const char *itemName(uint8_t i);
  const char *itemValue(uint8_t i);   // current value, formatted
  const char *editBuffer();           // digits typed so far
  const char *editHint();             // unit / range hint
}
