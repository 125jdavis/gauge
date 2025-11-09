/*
 * ========================================
 * HARDWARE CONFIGURATION IMPLEMENTATION
 * ========================================
 * 
 * This file contains the actual instantiation of all hardware objects
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#include "HardwareConfig.h"

// CAN bus controller object with CS pin 53
MCP_CAN CAN0(CAN0_CS);

// OLED displays
Adafruit_SSD1306 display1(SCREEN_W, SCREEN_H, &SPI, OLED_DC_1, OLED_RST_1, OLED_CS_1);  // Left display
Adafruit_SSD1306 display2(SCREEN_W, SCREEN_H, &SPI, OLED_DC_2, OLED_RST_2, OLED_CS_2);  // Right display

// Rotary encoder on interrupt pins 2 and 3 for responsive menu navigation
Rotary rotary = Rotary(2, 3);

// LED array for tachometer strip
CRGB leds[NUM_LEDS];

// Initialize stepper motors with sweep range and control pins
SwitecX12 motor1(M1_SWEEP, M1_STEP, M1_DIR); // Motor 1 - typically fuel level gauge
SwitecX12 motor2(M2_SWEEP, M2_STEP, M2_DIR); // Motor 2 - typically secondary gauge
SwitecX12 motor3(M3_SWEEP, M3_STEP, M3_DIR); // Motor 3 - typically speedometer (wider sweep)
SwitecX12 motor4(M4_SWEEP, M4_STEP, M4_DIR); // Motor 4 - typically coolant temperature gauge

// Odometer motor
Stepper odoMotor(odoSteps, odoPin1, odoPin2, odoPin3, odoPin4);

// GPS object using hardware serial port 2
Adafruit_GPS GPS(&Serial2);
