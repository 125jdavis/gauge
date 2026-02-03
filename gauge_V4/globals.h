/*
 * ========================================
 * GLOBAL VARIABLE DECLARATIONS
 * ========================================
 * 
 * Global variables used throughout the gauge system
 * Following STYLE.MD: variables use lowerCamelCase
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GPS.h>
#include <STM32_CAN.h>  // arduino-STM32-CAN library by nopnop2002
#include <SwitecX12.h>
#define HALF_STEP
#include <Rotary.h>
#include <FastLED.h>
#include "config_hardware.h"
#include "config_calibration.h"

// ===== HARDWARE OBJECT INSTANCES =====
extern Adafruit_SSD1306 display1;
extern Adafruit_SSD1306 display2;
extern Rotary rotary;
extern CRGB leds[MAX_LEDS];
extern SwitecX12 motor1;
extern SwitecX12 motor2;
extern SwitecX12 motor3;
extern SwitecX12 motor4;
extern SwitecX12 motorS;
// Note: odoMotor no longer uses Arduino Stepper library
// Direct pin control is used in outputs.cpp for non-blocking operation
extern Adafruit_GPS GPS;

// ===== ANALOG SENSOR READINGS =====
extern float vBatt;                 // Current battery voltage in volts (filtered)
extern int vBattRaw;                // Raw battery reading (0-500, representing 0-5V after mapping)
extern int fuelSensorRaw;           // Raw fuel sensor ADC reading (0-500)
extern float therm;                 // Current temperature in Celsius (after lookup table conversion)
extern float thermSensor;           // Voltage reading from thermistor (0-5V)
extern int thermCAN;                // Temperature formatted for CAN transmission (temp * 10)
extern float sensor_av1;            // Barometric pressure in kPa * 10
extern float sensor_av2;            // Reserved sensor B value
extern float sensor_av3;            // Reserved sensor C value

// ===== HALL EFFECT SPEED SENSOR VARIABLES =====
extern volatile unsigned long hallLastTime;     // Last pulse time (micros)
extern volatile float hallSpeedRaw;             // Most recent calculated speed (MPH)
extern unsigned int spdHall;                    // Filtered speed (km/h * 100)

// ===== ENGINE RPM SENSOR VARIABLES =====
extern volatile unsigned long ignitionLastTime; // Last ignition pulse time (micros)
extern volatile int engineRPMRaw;               // Most recent calculated RPM (unfiltered)
extern int engineRPMEMA;                        // Filtered RPM with exponential moving average

// ===== GPS SPEED AND ODOMETER VARIABLES =====
extern unsigned long v_old;         // Previous GPS speed reading (km/h * 100)
extern unsigned long spdGPS;        // Current GPS speed reading (km/h * 100)
extern unsigned long t_old;         // Previous GPS timestamp (milliseconds)
extern unsigned long t_new;         // Current GPS timestamp (milliseconds)
extern unsigned long v_100;         // Speed value * 100 for integer math precision
extern float v;                     // Current speed in km/h (floating point)
extern bool usingInterrupt;         // Flag indicating if GPS uses interrupt-based reading
extern int lagGPS;                  // Time delay since last GPS update (milliseconds)
extern int v_g;                     // GPS speed (alternate variable)
extern float odo;                   // Total odometer reading in kilometers (saved to EEPROM)
extern float odoTrip;               // Trip odometer in kilometers (resettable, saved to EEPROM)
extern float distLast;              // Distance traveled since last GPS update (km)
extern byte hour;                   // Current GPS hour (UTC, 0-23)
extern byte minute;                 // Current GPS minute (0-59)

// ===== STEPPER MOTOR POSITION VARIABLES =====
extern unsigned int spd_g;          // Speedometer target value (mph * 100)
extern unsigned int fuelLevelPct_g; // Fuel level percentage * 10
extern unsigned int coolantTemp_g;  // Coolant temperature for gauge calculation

// ===== ROTARY ENCODER VARIABLES =====
extern bool button;                 // Button press event flag (set when press completes)

// ===== TIMING VARIABLES =====
extern unsigned long timer0, timerDispUpdate, timerCANsend;
extern unsigned long timerSensorRead, timerTachUpdate, timerTachFlash;
extern unsigned long timerCheckGPS, timerGPSupdate, timerAngleUpdate;
extern unsigned long timerHallUpdate;
extern unsigned long timerEngineRPMUpdate;
extern unsigned long timerSigSelectUpdate;

// ===== CAN BUS ENGINE PARAMETERS =====
extern int rpmCAN;                  // Engine RPM (direct value, 0-10000+)
extern int mapCAN;                  // Manifold Absolute Pressure in kPa * 10
extern int tpsCAN;                  // Throttle Position Sensor percentage * 10
extern int fuelPrsCAN;              // Fuel pressure in kPa * 10 (absolute pressure)
extern int oilPrsCAN;               // Oil pressure in kPa * 10 (absolute pressure)
extern int injDutyCAN;              // Injector duty cycle percentage * 10
extern int ignAngCAN;               // Ignition timing in degrees BTDC * 10
extern int afr1CAN;                 // Air/Fuel Ratio * 1000
extern int knockCAN;                // Knock sensor level
extern int coolantTempCAN;          // Coolant temperature in Kelvin * 10
extern int airTempCAN;              // Intake Air Temperature in Kelvin * 10
extern int fuelTempCAN;             // Fuel temperature in Kelvin * 10
extern int oilTempCAN;              // Oil temperature in Celsius * 10
extern int transTempCAN;            // Transmission temperature in Celsius * 10
extern int fuelCompCAN;             // Fuel composition (ethanol %) * 10
extern int fuelLvlCAN;              // Fuel level percentage (0-100)
extern int baroCAN;                 // Barometric pressure in kPa * 10
extern int spdCAN;                  // Vehicle speed sent to CAN bus (km/h * 16)
extern int pumpPressureCAN;         // Fuel pump pressure (test variable)

// ===== OBDII POLLING VARIABLES =====
extern unsigned long timerOBDIIPriority1;  // Timer for 10Hz priority 1 polls
extern unsigned long timerOBDIIPriority2;  // Timer for 1Hz priority 2 polls
extern bool obdiiAwaitingResponse;         // Flag indicating waiting for OBDII response
extern uint8_t obdiiCurrentPID;            // Current PID being polled

// ===== PROCESSED ENGINE PARAMETERS FOR DISPLAY =====
extern float oilPrs;                // Oil pressure in kPa (gauge pressure)
extern float coolantTemp;           // Coolant temperature in Celsius
extern float fuelPrs;               // Fuel pressure in kPa (gauge pressure)
extern float oilTemp;               // Oil temperature in Celsius
extern float fuelLvl;               // Fuel level in gallons (or liters if metric)
extern float battVolt;              // Battery voltage in volts
extern float afr;                   // Air/Fuel Ratio
extern float fuelComp;              // Ethanol percentage (0-100%)
extern float manifoldPrs;           // Manifold Absolute Pressure in kPa
extern int RPM;                     // Engine RPM for display
extern int spd;                     // Vehicle speed in km/h * 100
extern float spdMph;                // Vehicle speed in miles per hour

// ===== CAN BUS COMMUNICATION BUFFERS =====
extern byte canMessageData[8];      // Received CAN message data
extern unsigned long rxId;          // Received CAN message ID
extern unsigned char len;           // Length of received CAN message
extern unsigned char rxBuf[8];      // Raw receive buffer from CAN controller
extern char msgString[128];         // String buffer for serial debug output

// ===== LOOKUP TABLES =====
extern const int thermTable_length;
extern float thermTable_x[];
extern float thermTable_l[];
extern const int fuelLvlTable_length;
extern float fuelLvlTable_x[];
extern float fuelLvlTable_l[];

// ===== EEPROM STORAGE ADDRESSES =====
extern byte dispArray1Address;      // Display 1 menu selections (4 bytes)
extern byte dispArray2Address;      // Display 2 selection (1 byte)
extern byte clockOffsetAddress;     // Time zone offset for clock (1 byte)
extern byte odoAddress;             // Total odometer value (4 bytes)
extern byte odoTripAddress;         // Trip odometer value (4 bytes)
extern byte fuelSensorRawAddress;   // Last fuel sensor reading (4 bytes)
extern byte unitsAddress;           // Unit system selection (1 byte)
extern int *input;                  // Pointer for EEPROM operations
extern int output;                  // Output buffer for EEPROM operations

// ===== MENU NAVIGATION VARIABLES =====
extern byte menuLevel;              // Current menu depth
extern byte units;                  // Unit system: 0=metric, 1=imperial
extern unsigned int nMenuLevel;     // Number of items in current menu level
extern byte dispArray1[4];          // Menu position array for display 1
extern byte dispArray2[1];          // Menu selection for display 2

#endif // GLOBALS_H
