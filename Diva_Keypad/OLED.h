#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// Declaration for an SH1106 display connected to I2C (SDA=GPIO16, SCL=GPIO17, shared with keypad; see Wire.begin in Keypad.h)
#define SCREEN_ADDRESS 0x3C //< See datasheet for Address; 0x3C for 128x64
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SH1106G oled = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1, 400000, 100000);


void setupOled() {
  oled.begin(0x3C, true); // Address 0x3C default
  oled.setContrast(255); oled.setTextWrap(true);
  oled.setTextSize(1); oled.setTextColor(SH110X_WHITE); oled.display();
}
