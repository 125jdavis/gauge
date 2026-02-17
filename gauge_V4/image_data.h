/*
 * ========================================
 * IMAGE DATA
 * ========================================
 * 
 * Bitmap images stored in program memory (PROGMEM) for OLED displays
 * Each image is 128x32 pixels (1 bit per pixel = 512 bytes) unless otherwise noted
 * Generated from graphics using image2cpp or similar tool
 */

#ifndef IMAGE_DATA_H
#define IMAGE_DATA_H

#include <Arduino.h>

// Splash screen logos
extern const unsigned char IMG_FALCON_SCRIPT[] PROGMEM;  // 128x32px - Falcon logo
extern const unsigned char IMG_302_CID[] PROGMEM;        // 128x32px - Engine displacement
extern const unsigned char IMG_302V[] PROGMEM;           // 128x32px - 302 with V8 symbol

// Sensor icons for displays
extern const unsigned char IMG_OIL_PRS[] PROGMEM;        // 40x32px - Oil can icon
extern const unsigned char IMG_OIL_TEMP[] PROGMEM;       // 40x32px - Oil can with thermometer
extern const unsigned char IMG_BATT_VOLT[] PROGMEM;      // 38x32px - Battery icon
extern const unsigned char IMG_COOLANT_TEMP[] PROGMEM;   // 35x32px - Thermometer icon
extern const unsigned char IMG_FUEL_LVL[] PROGMEM;       // 32x32px - Gas pump icon
extern const unsigned char IMG_TURBO[] PROGMEM;          // 24x30px - Turbo icon

#endif // IMAGE_DATA_H
