/*
 * ========================================
 * GLOBAL VARIABLES
 * ========================================
 * 
 * This file contains all global variables used throughout the gauge system
 * including sensor readings, configuration, state, and lookup tables
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef GLOBAL_VARIABLES_H
#define GLOBAL_VARIABLES_H

#include <Arduino.h>

///// GLOBAL VARIABLES /////

// ===== ANALOG SENSOR INPUTS =====
// All analog sensors are read through Arduino's ADC (0-1023 raw values, mapped to appropriate ranges)

// Battery Voltage Sensor (Analog Pin A0)
extern float vBatt;              // Current battery voltage in volts (filtered)
extern int vBattRaw;             // Raw battery reading (0-500, representing 0-5V after mapping)
extern int filter_vBatt;         // Filter coefficient out of 64 (8/64 = light filtering, 64 = no filter)
extern int vBattPin;             // Analog input pin for battery voltage
extern float vBattScaler;        // Voltage divider scaling factor

// Fuel Level Sensor (Analog Pin A3)
extern int fuelSensorRaw;        // Raw fuel sensor ADC reading (0-500)
extern int filter_fuel;          // Light filter: 1/64 = very responsive to changes
extern int fuelPin;              // Analog input pin for fuel level sensor

// Coolant/Oil Temperature Thermistor Sensor (Analog Pin A4)
extern float therm;              // Current temperature in Celsius (after lookup table conversion)
extern float thermSensor;        // Voltage reading from thermistor (0-5V)
extern int filter_therm;         // Medium filter: 50/100 for stable temp reading
extern int thermPin;             // Analog input pin for thermistor
extern int thermCAN;             // Temperature formatted for CAN transmission (temp * 10)

// Barometric Pressure Sensor (Analog Pin A5)
extern unsigned long baro;       // Barometric pressure in kPa * 10
extern byte filter_baro;         // Filter coefficient out of 16 (4/16 = moderate filtering)
extern int baroPin;              // Analog input pin for barometric sensor

// Reserved Analog Sensors B and C (future expansion)
extern float sensor_b;           // Reserved sensor B value
extern byte filter_b;            // Filter coefficient for sensor B (12/16)
extern int analogPin6;           // Analog pin 6

extern float sensor_c;           // Reserved sensor C value
extern byte filter_c;            // Filter coefficient for sensor C (12/16)
extern int analogPin7;           // Analog pin 7

// ===== GPS SPEED AND ODOMETER VARIABLES =====
extern unsigned long v_old;      // Previous GPS speed reading (km/h * 100)
extern unsigned long v_new;      // Current GPS speed reading (km/h * 100)
extern unsigned long t_old;      // Previous GPS timestamp (milliseconds)
extern unsigned long t_new;      // Current GPS timestamp (milliseconds)
extern unsigned long v_100;      // Speed value * 100 for integer math precision
extern float v;                  // Current speed in km/h (floating point)
extern bool usingInterrupt;      // Flag indicating if GPS uses interrupt-based reading
extern int lagGPS;               // Time delay since last GPS update (milliseconds)
extern int v_g;                  // GPS speed (alternate variable)
extern float odo;                // Total odometer reading in kilometers (saved to EEPROM)
extern float odoTrip;            // Trip odometer in kilometers (resettable, saved to EEPROM)
extern float distLast;           // Distance traveled since last GPS update (km)
extern byte hour;                // Current GPS hour (UTC, 0-23)
extern byte minute;              // Current GPS minute (0-59)

// ===== STEPPER MOTOR POSITION VARIABLES =====
extern unsigned int spd_g;       // Speedometer target value (mph * 100)
extern unsigned int fuelLevelPct_g;  // Fuel level percentage * 10
extern unsigned int coolantTemp_g;   // Coolant temperature for gauge calculation

// ===== ROTARY ENCODER VARIABLES =====
extern bool stateSW;                     // Current state of encoder switch (1 = not pressed)
extern bool lastStateSW;                 // Previous state of encoder switch
extern unsigned int lastStateChangeTime; // Timestamp of last switch state change (ms)
extern unsigned int debounceDelay;       // Debounce time in milliseconds
extern bool debounceFlag;                // Flag to prevent multiple triggers during debounce
extern bool button;                      // Button press event flag (set when press completes)

// ===== TIMING VARIABLES =====
extern unsigned int timer0, timerDispUpdate, timerCANsend;
extern unsigned int timerSensorRead, timerTachUpdate, timerTachFlash;
extern unsigned int timerCheckGPS, timerGPSupdate, timerAngleUpdate;

// Update rate periods (in milliseconds)
extern unsigned int CANsendRate;         // Send CAN messages every 50ms (20Hz)
extern unsigned int dispUpdateRate;      // Update displays every 75ms (~13Hz)
extern unsigned int sensorReadRate;      // Read analog sensors every 10ms (100Hz for responsive readings)
extern unsigned int tachUpdateRate;      // Update LED tachometer every 50ms (20Hz)
extern unsigned int tachFlashRate;       // Flash shift light every 50ms when over redline
extern unsigned int GPSupdateRate;       // GPS update check rate (might not be needed)
extern unsigned int checkGPSRate;        // Check for GPS data every 1ms
extern unsigned int angleUpdateRate;     // Update motor angles every 20ms (50Hz)
extern unsigned int splashTime;          // Duration of startup splash screens (milliseconds)

// ===== LED TACHOMETER VARIABLES =====
extern unsigned int tachMax;             // RPM at which shift light activates and flashes
extern unsigned int tachMin;             // Minimum RPM to show on tach (below this LEDs are off)
extern bool tachFlashState;              // Current state of shift light flashing (0=off, 1=on)

// ===== CAN BUS ENGINE PARAMETERS =====
// Raw values received from Haltech ECU via CAN bus
extern int rpmCAN;               // Engine RPM (direct value, 0-10000+)
extern int mapCAN;               // Manifold Absolute Pressure in kPa * 10
extern int tpsCAN;               // Throttle Position Sensor percentage * 10
extern int fuelPrsCAN;           // Fuel pressure in kPa * 10 (absolute pressure)
extern int oilPrsCAN;            // Oil pressure in kPa * 10 (absolute pressure)
extern int injDutyCAN;           // Injector duty cycle percentage * 10
extern int ignAngCAN;            // Ignition timing in degrees BTDC * 10
extern int afr1CAN;              // Air/Fuel Ratio * 1000
extern int knockCAN;             // Knock sensor level (higher = more knock detected)
extern int coolantTempCAN;       // Coolant temperature in Kelvin * 10
extern int airTempCAN;           // Intake Air Temperature in Kelvin * 10
extern int fuelTempCAN;          // Fuel temperature in Kelvin * 10
extern int oilTempCAN;           // Oil temperature in Celsius * 10
extern int transTempCAN;         // Transmission temperature in Celsius * 10
extern int fuelCompCAN;          // Fuel composition (ethanol %) * 10
extern int fuelLvlCAN;           // Fuel level percentage (0-100)
extern int baroCAN;              // Barometric pressure in kPa * 10 (sent TO other modules)
extern int spdCAN;               // Vehicle speed sent to CAN bus (km/h * 16 for protocol compatibility)
extern int pumpPressureCAN;      // Fuel pump pressure (test variable)

// ===== PROCESSED ENGINE PARAMETERS FOR DISPLAY =====
extern float oilPrs;             // Oil pressure in kPa (gauge pressure, atmospheric offset removed)
extern float coolantTemp;        // Coolant temperature in Celsius
extern float fuelPrs;            // Fuel pressure in kPa (gauge pressure)
extern float oilTemp;            // Oil temperature in Celsius
extern float fuelLvl;            // Fuel level in gallons (or liters if metric)
extern float battVolt;           // Battery voltage in volts
extern float afr;                // Air/Fuel Ratio
extern float fuelComp;           // Ethanol percentage (0-100%)
extern int RPM;                  // Engine RPM for display
extern int spd;                  // Vehicle speed in km/h * 100 (for integer precision)
extern float spdMph;             // Vehicle speed in miles per hour

// ===== CAN BUS COMMUNICATION BUFFERS =====
extern byte data[8];             // Outgoing CAN message buffer (8 bytes)
extern byte canMessageData[8];   // Received CAN message data
extern unsigned long rxId;       // Received CAN message ID (11-bit or 29-bit)
extern unsigned char len;        // Length of received CAN message (0-8 bytes)
extern unsigned char rxBuf[8];   // Raw receive buffer from CAN controller
extern char msgString[128];      // String buffer for serial debug output

// ===== LOOKUP TABLES =====
// Thermistor Temperature Lookup Table
extern const int thermTable_length;
extern float thermTable_x[];     // Voltage breakpoints (0-5V)
extern float thermTable_l[];     // Temperature values in Celsius

// Fuel Level Lookup Table
extern const int fuelLvlTable_length;
extern float fuelLvlTable_x[];   // Voltage breakpoints
extern float fuelLvlTable_l[];   // Gallons remaining
extern float fuelCapacity;       // Total fuel tank capacity in gallons

// ===== EEPROM STORAGE ADDRESSES =====
extern byte dispArray1Address;      // Display 1 menu selections (4 bytes: addresses 0-3)
extern byte dispArray2Address;      // Display 2 selection (1 byte: address 4)
extern byte clockOffsetAddress;     // Time zone offset for clock (1 byte: address 5)
extern byte odoAddress;             // Total odometer value (4 bytes: addresses 6-9)
extern byte odoTripAddress;         // Trip odometer value (4 bytes: addresses 10-13)
extern byte fuelSensorRawAddress;   // Last fuel sensor reading (for fuel level memory, addresses 14-17)
extern byte unitsAddress;           // Unit system selection: 0=metric, 1=imperial (1 byte: address 18)
extern int *input;                  // Pointer for EEPROM operations
extern int output;                  // Output buffer for EEPROM operations

// ===== MENU NAVIGATION VARIABLES =====
extern byte menuLevel;              // Current menu depth (0=top level, 1=submenu, 2=sub-submenu)
extern byte units;                  // Unit system: 0=metric (km/h, C, bar), 1=imperial (mph, F, PSI)
extern unsigned int nMenuLevel;     // Number of items in current menu level
extern byte dispArray1[4];          // Menu position array for display 1 [level0, level1, level2, level3]
extern byte clockOffset;            // Hours to add to UTC time for local time zone (-12 to +12)
extern byte dispArray2[1];          // Menu selection for display 2 (single level)

// ===== DEMO/TEST VARIABLES =====
extern bool rpmSwitch;              // Direction flag for demo RPM sweep (0=increasing, 1=decreasing)
extern int gRPM;                    // Generated RPM value for demo mode
extern int analog;                  // Test analog value
extern int analogSwitch;            // Direction flag for analog test sweep

#endif // GLOBAL_VARIABLES_H
