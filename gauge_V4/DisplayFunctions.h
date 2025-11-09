/*
 * ========================================
 * DISPLAY FUNCTIONS
 * ========================================
 * 
 * This file contains all OLED display rendering functions
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef DISPLAY_FUNCTIONS_H
#define DISPLAY_FUNCTIONS_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

// Settings menu displays
void dispSettings(Adafruit_SSD1306 *display);
void dispDisp2Select(Adafruit_SSD1306 *display);
void dispUnits(Adafruit_SSD1306 *display);
void dispClockOffset(Adafruit_SSD1306 *display);

// Data displays - text based
void dispRPM(Adafruit_SSD1306 *display);
void dispSpd(Adafruit_SSD1306 *display);
void dispOilTemp(Adafruit_SSD1306 *display);
void dispFuelPrs(Adafruit_SSD1306 *display);
void dispFuelComp(Adafruit_SSD1306 *display);
void dispAFR(Adafruit_SSD1306 *display);

// Logo/splash screens
void dispFalconScript(Adafruit_SSD1306 *display);
void disp302CID(Adafruit_SSD1306 *display);
void disp302V(Adafruit_SSD1306 *display);

// Data displays - graphical with icons
void dispOilPrsGfx(Adafruit_SSD1306 *display);
void dispOilTempGfx(Adafruit_SSD1306 *display);
void dispCoolantTempGfx(Adafruit_SSD1306 *display);
void dispBattVoltGfx(Adafruit_SSD1306 *display);
void dispFuelLvlGfx(Adafruit_SSD1306 *display);

// Trip odometer and reset
void dispTripOdo(Adafruit_SSD1306 *display);
void dispOdoResetYes(Adafruit_SSD1306 *display);
void dispOdoResetNo(Adafruit_SSD1306 *display);

// Engine parameters
void dispIgnAng(Adafruit_SSD1306 *display);
void dispInjDuty(Adafruit_SSD1306 *display);

// Clock display
void dispClock(Adafruit_SSD1306 *display);

#endif // DISPLAY_FUNCTIONS_H
