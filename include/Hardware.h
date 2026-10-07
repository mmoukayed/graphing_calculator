#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace hardware {
extern TFT_eSPI display;
extern bool touchFound, keyboardFound;
void begin();
void brightness(uint8_t level);
char key();
// True means a fresh GT911 report; pressed=false is an explicit release.
bool touch(int& x, int& y, bool& pressed);
void ball(int& dx, int& dy, bool& click, bool& held);
}
