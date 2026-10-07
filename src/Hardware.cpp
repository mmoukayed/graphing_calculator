#include "Hardware.h"
#include "BoardConfig.h"
#include <Wire.h>
#include <algorithm>

namespace hardware {
TFT_eSPI display;
bool touchFound=false,keyboardFound=false;
static uint8_t touchAddress=0x5D;
static volatile int ballX=0,ballY=0;
static portMUX_TYPE ballMux=portMUX_INITIALIZER_UNLOCKED;
static void IRAM_ATTR right() {portENTER_CRITICAL_ISR(&ballMux);++ballX;portEXIT_CRITICAL_ISR(&ballMux);}
static void IRAM_ATTR left() {portENTER_CRITICAL_ISR(&ballMux);--ballX;portEXIT_CRITICAL_ISR(&ballMux);}
static void IRAM_ATTR up() {portENTER_CRITICAL_ISR(&ballMux);--ballY;portEXIT_CRITICAL_ISR(&ballMux);}
static void IRAM_ATTR down() {portENTER_CRITICAL_ISR(&ballMux);++ballY;portEXIT_CRITICAL_ISR(&ballMux);}
static bool readReg(uint16_t reg,uint8_t* data,size_t n) {
    Wire.beginTransmission(touchAddress);Wire.write(reg>>8);Wire.write(reg&255);
    if(Wire.endTransmission(false)!=0)return false;
    if(Wire.requestFrom(touchAddress,uint8_t(n))!=n)return false;
    for(size_t j=0;j<n;++j)data[j]=Wire.read();return true;
}
static void acknowledge() {
    Wire.beginTransmission(touchAddress);Wire.write(0x81);Wire.write(0x4E);Wire.write(0);Wire.endTransmission();
}
void brightness(uint8_t level) {
    static uint8_t current=0;level=std::min(uint8_t(16),level);
    if(level==0){digitalWrite(board::Backlight,LOW);delay(3);current=0;return;}
    if(current==0){digitalWrite(board::Backlight,HIGH);delayMicroseconds(30);current=16;}
    int steps=(current-level+16)%16;
    while(steps--){digitalWrite(board::Backlight,LOW);delayMicroseconds(1);digitalWrite(board::Backlight,HIGH);delayMicroseconds(1);}
    current=level;
}
void begin() {
    pinMode(board::Power,OUTPUT);digitalWrite(board::Power,HIGH);
    for(int pin:{board::SdCs,board::RadioCs,12}){pinMode(pin,OUTPUT);digitalWrite(pin,HIGH);}
    pinMode(board::Backlight,OUTPUT);brightness(0);delay(500);
    Wire.begin(board::Sda,board::Scl);Wire.setClock(400000);Wire.setTimeOut(20);
    pinMode(board::TouchInt,INPUT);pinMode(board::KeyboardInt,INPUT);
    for(uint8_t addr:{uint8_t(0x5D),uint8_t(0x14)}) {
        touchAddress=addr;uint8_t id[4];
        if(readReg(0x8140,id,4)&&id[0]=='9'){touchFound=true;break;}
    }
    Wire.beginTransmission(0x55);keyboardFound=Wire.endTransmission()==0;
    // Stock keyboard character mode; older firmware safely ignores command 0x04.
    if(keyboardFound){Wire.beginTransmission(0x55);Wire.write(0x04);Wire.write(0);Wire.endTransmission();}
    for(int pin:{board::BallRight,board::BallLeft,board::BallUp,board::BallDown})pinMode(pin,INPUT_PULLUP);
    pinMode(board::Select,INPUT_PULLUP);
    attachInterrupt(board::BallRight,right,CHANGE);attachInterrupt(board::BallLeft,left,CHANGE);
    attachInterrupt(board::BallUp,up,CHANGE);attachInterrupt(board::BallDown,down,CHANGE);
    display.init();
    // T-Deck's 2024 panel power/gamma settings, as in LILYGO's factory firmware.
    struct Cmd {uint8_t command,count,data[14];};
    const Cmd commands[]={
        {0xB2,5,{0x0C,0x0C,0,0x33,0x33}},{0xB7,1,{0x75}},{0xBB,1,{0x1A}},
        {0xC0,1,{0x2C}},{0xC2,1,{1}},{0xC3,1,{0x13}},{0xC4,1,{0x20}},
        {0xC6,1,{0x0F}},{0xD0,2,{0xA4,0xA1}},{0xD6,1,{0xA1}},
        {0xE0,14,{0xD0,0x0D,0x14,0x0D,0x0D,0x09,0x38,0x44,0x4E,0x3A,0x17,0x18,0x2F,0x30}},
        {0xE1,14,{0xD0,0x09,0x0F,0x08,0x07,0x14,0x37,0x44,0x4D,0x38,0x15,0x16,0x2C,0x3E}}
    };
    for(auto& c:commands){display.writecommand(c.command);for(int j=0;j<c.count;++j)display.writedata(c.data[j]);}
    display.setRotation(1);display.invertDisplay(true);display.fillScreen(TFT_BLACK);brightness(12);
    Serial.printf("GT911: %s at 0x%02X, keyboard: %s\n",touchFound?"OK":"missing",touchAddress,keyboardFound?"OK":"missing");
}
char key() {
    if(!keyboardFound)return 0;
    // Polling avoids losing a buffered key when the keyboard INT pulse is short.
    if(Wire.requestFrom(uint8_t(0x55),uint8_t(1))==1)return char(Wire.read());
    return 0;
}
bool touch(int& x,int& y,bool& pressed) {
    if(!touchFound)return false;uint8_t status;
    if(!readReg(0x814E,&status,1)||!(status&0x80))return false;
    int count=status&0x0F;pressed=false;
    if(count>0&&count<=5) {
        uint8_t point[8];if(!readReg(0x814F,point,8)){acknowledge();return false;}
        int rawX=point[1]|(point[2]<<8),rawY=point[3]|(point[4]<<8);
        x=board::TouchSwapXY?rawY:rawX;y=board::TouchSwapXY?rawX:rawY;
        if(board::TouchMirrorX)x=319-x;if(board::TouchMirrorY)y=239-y;
        x=std::max(0,std::min(319,x));y=std::max(0,std::min(239,y));pressed=true;
    }
    acknowledge();return true;
}
void ball(int& dx,int& dy,bool& click,bool& held) {
    portENTER_CRITICAL(&ballMux);dx=ballX;dy=ballY;ballX=ballY=0;portEXIT_CRITICAL(&ballMux);
    static bool lastRaw=false,stable=false,longSent=false;static uint32_t changed=0,pressedAt=0;
    bool raw=digitalRead(board::Select)==LOW;uint32_t now=millis();click=held=false;
    if(raw!=lastRaw){changed=now;lastRaw=raw;}
    if(now-changed>25&&stable!=raw){stable=raw;if(stable){pressedAt=now;longSent=false;}else if(!longSent)click=true;}
    if(stable&&!longSent&&now-pressedAt>700){held=true;longSent=true;}
}
}
