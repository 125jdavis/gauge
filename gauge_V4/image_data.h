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
extern const unsigned char IMG_2300_TURBO[] PROGMEM;     // 128x32px - 2300 turbo
extern const unsigned char IMG_144_CID[] PROGMEM;        // 128x32px - 144 Cubic Inches
extern const unsigned char IMG_170_CID[] PROGMEM;        // 128x32px - 170 Cubic Inches
extern const unsigned char IMG_200_CID[] PROGMEM;        // 128x32px - 200 Cubic Inches
extern const unsigned char IMG_260_CID[] PROGMEM;        // 128x32px - 260 Cubic Inches
extern const unsigned char IMG_260V[] PROGMEM;           // 128x32px - 260 with V emblem
extern const unsigned char IMG_289_CID[] PROGMEM;        // 128x32px - 289 Cubic Inches
extern const unsigned char IMG_289V[] PROGMEM;           // 128x32px - 289 with V emblem

// Selectable splash images, indexed by splash image ID (append-only).
// Each entry is a PROGMEM pointer: read with pgm_read_ptr().
extern const unsigned char *const SPLASH_IMAGES[] PROGMEM;
extern const uint8_t SPLASH_IMAGE_COUNT;

// Sensor icons for displays
extern const unsigned char IMG_OIL_PRS[] PROGMEM;        // 40x32px - Oil can icon
extern const unsigned char IMG_OIL_TEMP[] PROGMEM;       // 40x32px - Oil can with thermometer
extern const unsigned char IMG_BATT_VOLT[] PROGMEM;      // 38x32px - Battery icon
extern const unsigned char IMG_COOLANT_TEMP[] PROGMEM;   // 35x32px - Thermometer icon
extern const unsigned char IMG_FUEL_LVL[] PROGMEM;       // 32x32px - Gas pump icon
extern const unsigned char IMG_TURBO[] PROGMEM;          // 24x30px - Turbo icon

#endif // IMAGE_DATA_H
