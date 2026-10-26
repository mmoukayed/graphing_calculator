#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#define SET_LOOP_TASK_STACK_SIZE(n)
inline uint32_t mockMillis=0;
inline uint32_t millis(){return mockMillis;}
inline void delay(unsigned long ms){mockMillis+=ms;}
struct MockSerial {void begin(int){} int available(){return 0;} int read(){return 0;}};
inline MockSerial Serial;
using String=std::string;
struct MockESP {
    const char* getChipModel()const{return "ESP32-S3";}
    const char* getSdkVersion()const{return "host mock";}
    uint8_t getChipRevision()const{return 1;}
    uint8_t getChipCores()const{return 2;}
    uint32_t getCpuFreqMHz()const{return 240;}
    uint32_t getFlashChipSize()const{return 16*1024*1024;}
    uint32_t getPsramSize()const{return 8*1024*1024;}
    uint32_t getFreePsram()const{return 7*1024*1024;}
    uint32_t getHeapSize()const{return 300*1024;}
    uint32_t getFreeHeap()const{return 200*1024;}
};
inline MockESP ESP;
