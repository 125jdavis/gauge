#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_SSD1306.h>

// OLED Screen 1
#define SCREEN_W 128 // OLED display width, in pixels
#define SCREEN_H 32 // OLED display height, in pixels
//#define MOSI  51    // SPI Master Out Pin
//#define CLK   52    // SPI Clock Pin
#define OLED_DC_1    6
#define OLED_CS_1  5
#define OLED_RST_1 7

// OLED Screen 2
//#define SCREEN_W_2 128 // both screens are the same size, use only one width definition
//#define SCREEN_H_2 32 // both screens are the same size, use only one height definition
#define OLED_DC_2  28
#define OLED_CS_2  29
#define OLED_RST_2 26

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