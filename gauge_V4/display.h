/*
 * ========================================
 * DISPLAY FUNCTIONS
 * ========================================
 * 
 * OLED display rendering functions
 * These functions render specific screens and data on the OLED displays
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

// Main display control functions
void disp2(void);                                    // Display 2 main controller
void dispMenu();                                      // Display 1 menu system controller

// Menu header screens
void dispSettings(Adafruit_SSD1306 *display);       // "SETTINGS" header
void dispDisp2Select(Adafruit_SSD1306 *display);    // "DISPLAY 2" header
void dispUnits(Adafruit_SSD1306 *display);          // "UNITS" header
void dispClockOffset(Adafruit_SSD1306 *display);    // "SET CLOCK" header

// Data display screens
void dispRPM(Adafruit_SSD1306 *display);            // RPM numerical display
void dispSpd(Adafruit_SSD1306 *display);            // Speed numerical display
void dispOilTemp(Adafruit_SSD1306 *display);        // Oil temperature
void dispFuelPrs(Adafruit_SSD1306 *display);        // Fuel pressure
void dispFuelComp(Adafruit_SSD1306 *display);       // Fuel composition (ethanol %)
void dispAFR(Adafruit_SSD1306 *display);            // Air/Fuel Ratio
void dispIgnAng(Adafruit_SSD1306 *display);         // Ignition angle
void dispInjDuty(Adafruit_SSD1306 *display);        // Injector duty cycle
void dispClock(Adafruit_SSD1306 *display);          // Clock display
void dispTripOdo(Adafruit_SSD1306 *display);        // Trip odometer

// Graphical display screens (with icons)
void dispOilPrsGfx(Adafruit_SSD1306 *display);      // Oil pressure with icon
void dispOilTempGfx(Adafruit_SSD1306 *display);     // Oil temp with icon
void dispCoolantTempGfx(Adafruit_SSD1306 *display); // Coolant temp with icon
void dispBattVoltGfx(Adafruit_SSD1306 *display);    // Battery voltage with icon
void dispFuelLvlGfx(Adafruit_SSD1306 *display);     // Fuel level with icon

// Logo/image displays
void dispFalconScript(Adafruit_SSD1306 *display);   // Falcon logo
void disp302CID(Adafruit_SSD1306 *display);         // 302 CID logo
void disp302V(Adafruit_SSD1306 *display);           // 302V logo

// Odometer reset confirmation screens
void dispOdoResetYes(Adafruit_SSD1306 *display);    // "YES" confirmation
void dispOdoResetNo(Adafruit_SSD1306 *display);     // "NO" confirmation

// Utility functions
byte digits(float val);                              // Count digits in number for centering

#endif // DISPLAY_H
