/**
 * @file txdisp.h
 * @brief v2.0.3: serial display bridge - streams the status screen as
 *        tiny text packets (~150 bytes/s, 1 Hz) to a companion Arduino
 *        that drives a PARALLEL (D0-D7 + WR + RD "UNO-shield") TFT.
 *
 * Why: the parallel display cannot connect to the ESP32-S3 (no SPI pins,
 * not enough GPIO for an 8-bit bus). But it plugs straight onto an
 * Arduino UNO/Mega - so the Arduino becomes the display driver and the
 * S3 just tells it what to show. One wire, one direction:
 *
 *   S3 GPIO3 (TX, 3.3 V) ---> Arduino RX (UNO/Nano pin 4 SoftwareSerial,
 *                              Mega pin 19 RX1)      + GND common
 *   NEVER wire the Arduino's TX (5 V) back to the S3!
 *
 * Packet format (plain lines, key,value - see display-bridge-uno.ino):
 *   $ST,DRYING   $T,60.5,58.2   $H,45,50   $SET,60,120
 *   $E,3600,3600 $P,34,100,12.4,78  heat,fan,batV,bat%
 *   $W,1234,1000,234  cur,target,diff   $D,READY,1,1234  phase,closed,batch
 *   $S,SOLAR MODE,1,0 $F,<fault text>   $V,2.0.3
 *
 * The S3 firmware stays zero-library; the ARDUINO side uses its own
 * graphics stack (Adafruit GFX + MCUfriend_kbv) - manual 02 sect.4.12.
 */
#pragma once

namespace txdisp {
  void begin();     // UART up on PIN_TXDISP_TX at TXDISP_BAUD
  void update();    // 1 Hz gated: one $-packet burst (call from loop)
}
