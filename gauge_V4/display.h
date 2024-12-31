#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_SSD1306.h>

void updateDisplay();
void dispMenu();
void disp2();
void swRead();
void rotate();
void incrementOffset();
void dispSettings(Adafruit_SSD1306 *display);
void dispDisp2Select(Adafruit_SSD1306 *display);
// Add other display functions...

#endif // DISPLAY_H