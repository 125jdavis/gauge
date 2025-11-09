/*
 * ========================================
 * HARDWARE CONFIGURATION
 * ========================================
 * 
 * This file contains all hardware-specific definitions including:
 * - Pin assignments for all peripherals
 * - Hardware object instances (CAN, displays, GPS, motors, LEDs)
 * - Hardware-related constants
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <mcp_can.h>
#include <Rotary.h>
#include <EEPROM.h>
#include <FastLED.h>
#include <Adafruit_GPS.h>
#include <SwitecX25.h>
#include <SwitecX12.h>
#include <Stepper.h>

///// PIN DEFINITIONS AND HARDWARE CONFIGURATION /////

// CAN Bus Configuration
#define CAN0_CS 53        // MCP2515 CAN controller chip select pin (SPI)
#define CAN0_INT 18       // MCP2515 interrupt pin - triggers when CAN message received

// GAUGE HARDWARE SETUP //
#define pwrPin 49              // Power control pin - keeps system alive after ignition is off
#define speedoMax (100*100)    // Maximum speedometer reading: 100 mph * 100 (stored as integer for precision)

#define MOTOR_RST 36           // Stepper motor driver reset pin - shared by all motor drivers

// Motor 1 Configuration (typically speedometer or fuel gauge)
#define M1_SWEEP (58*12)       // Total steps for full sweep: 58 degrees * 12 steps/degree = 696 steps
                               // X25.168 motors have 315° range at 1/3° per step
#define M1_STEP  37            // Motor 1 step pulse pin
#define M1_DIR   38            // Motor 1 direction control pin

// Motor 2 Configuration (typically coolant temp or secondary gauge)
#define M2_SWEEP (58*12)       // Total steps: 58 degrees * 12 steps/degree = 696 steps
#define M2_STEP  34            // Motor 2 step pulse pin
#define M2_DIR   35            // Motor 2 direction control pin

// Motor 3 Configuration (typically speedometer - note larger sweep angle)
#define M3_SWEEP (118*12)      // Total steps: 118 degrees * 12 steps/degree = 1416 steps (wider range)
#define M3_STEP  33            // Motor 3 step pulse pin
#define M3_DIR   32            // Motor 3 direction control pin

// Motor 4 Configuration (typically fuel level or coolant temp)
#define M4_SWEEP (58*12)       // Total steps: 58 degrees * 12 steps/degree = 696 steps
#define M4_STEP  40            // Motor 4 step pulse pin
#define M4_DIR   41            // Motor 4 direction control pin

// GPS Configuration
#define GPSECHO  false         // Set to true to echo raw GPS data to serial monitor (debug only)

// Rotary Encoder Configuration
#define SWITCH 24              // Rotary encoder push button pin (for menu selection)

// OLED Display 1 Configuration (SPI interface)
#define SCREEN_W 128           // OLED display width in pixels
#define SCREEN_H 32            // OLED display height in pixels
#define OLED_DC_1    6         // Display 1 Data/Command pin
#define OLED_CS_1  5           // Display 1 Chip Select pin
#define OLED_RST_1 7           // Display 1 Reset pin

// OLED Display 2 Configuration (SPI interface)
#define OLED_DC_2  28          // Display 2 Data/Command pin
#define OLED_CS_2  29          // Display 2 Chip Select pin
#define OLED_RST_2 26          // Display 2 Reset pin

// LED Tachometer Configuration
#define NUM_LEDS 26            // Total number of LEDs in the tachometer strip
#define WARN_LEDS 6            // Warning zone LEDs on each side of center (turns yellow/orange)
#define SHIFT_LEDS 2           // Shift light LEDs on each side of center (turns red at shift point)
#define TACH_DATA_PIN 22       // WS2812 data pin for LED tachometer strip

// Odometer motor configuration (mechanical digit roller - currently non-functional)
#define odoSteps 32        // Steps per revolution for odometer motor
#define odoPin1 10         // Odometer motor coil 1 pin
#define odoPin2 11         // Odometer motor coil 2 pin
#define odoPin3 12         // Odometer motor coil 3 pin
#define odoPin4 13         // Odometer motor coil 4 pin

///// HARDWARE OBJECT INITIALIZATION /////
// External declarations - actual instances are defined in HardwareConfig.cpp

extern MCP_CAN CAN0;
extern Adafruit_SSD1306 display1;
extern Adafruit_SSD1306 display2;
extern Rotary rotary;
extern CRGB leds[NUM_LEDS];
extern SwitecX12 motor1;
extern SwitecX12 motor2;
extern SwitecX12 motor3;
extern SwitecX12 motor4;
extern Stepper odoMotor;
extern Adafruit_GPS GPS;

#endif // HARDWARE_CONFIG_H
