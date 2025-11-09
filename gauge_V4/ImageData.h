/*
 * ========================================
 * IMAGE DATA
 * ========================================
 * 
 * Bitmap images stored in program memory (PROGMEM) for OLED displays
 * Each image is either 128x32, 40x32, 38x32, 35x32, or 32x32 pixels
 * (1 bit per pixel)
 * Generated from graphics using image2cpp or similar tool
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef IMAGE_DATA_H
#define IMAGE_DATA_H

#include <Arduino.h>

// 'falcon_script', 128x32px
// Falcon logo in script font - displayed on startup splash screen
extern const unsigned char img_falcon_script [] PROGMEM;

// '302_CID', 128x32px
// Engine displacement designation: 302 Cubic Inch Displacement
extern const unsigned char img_302_CID [] PROGMEM;

// '302V', 128x32px
// Alternative engine badge - 302 with V8 symbol
extern const unsigned char img_302V [] PROGMEM;

// 'Oil Pressure Icon', 40x32px
// Icon showing oil can symbol - displayed alongside oil pressure reading
extern const unsigned char img_oilPrs [] PROGMEM;

// 'Oil Temp Icon', 40x32px
// Icon combining oil can and thermometer - for oil temperature display
extern const unsigned char img_oilTemp [] PROGMEM;

// 'Battery Icon', 38x32px
// Battery symbol with + and - terminals - for voltage display
extern const unsigned char img_battVolt [] PROGMEM;

// 'Eng Temp Icon', 35x32px
// Thermometer icon for coolant/engine temperature display
extern const unsigned char img_coolantTemp [] PROGMEM;

// 'Gas Icon', 32x32px
// Gas pump icon for fuel level display
extern const unsigned char img_fuelLvl [] PROGMEM;

#endif // IMAGE_DATA_H
