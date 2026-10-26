#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace hardware {
extern TFT_eSPI display;
extern bool touchFound, keyboardFound;
struct BatteryInfo {
    uint16_t millivolts=0,adcMillivolts=0,raw=0;
    bool valid=false;
};
void begin();
void brightness(uint8_t level);
void sampleBattery();
BatteryInfo batteryInfo();
bool generateQr(const char* payload);
int qrSize();
bool qrModule(int x,int y);
char key();
// True means a fresh GT911 report; pressed=false is an explicit release.
bool touch(int& x, int& y, bool& pressed);
void ball(int& dx, int& dy, bool& click, bool& held);
}
