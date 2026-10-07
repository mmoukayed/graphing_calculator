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
