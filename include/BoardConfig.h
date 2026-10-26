#pragma once
// LILYGO T-Deck / T-Deck Plus, 320x240 ST7789 + GT911.
// This is not the e-paper T-Deck Pro or T-Deck Max.
namespace board {
constexpr int Power = 10, Sda = 18, Scl = 8, TouchInt = 16, BatteryAdc = 4;
constexpr int KeyboardInt = 46, Backlight = 42, Select = 0;
constexpr int BallRight = 2, BallUp = 3, BallLeft = 1, BallDown = 15;
constexpr int SdCs = 39, RadioCs = 9;
constexpr bool TouchSwapXY = true, TouchMirrorX = false, TouchMirrorY = true;
constexpr int PointerStep = 5;
}
