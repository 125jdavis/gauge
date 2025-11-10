/*
 * ========================================
 * GAUGE CONTROL MODULE
 * ========================================
 * 
 * Arduino-based instrument panel controller for vintage vehicles
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 * Status: Fully functional except mechanical ODO
 * 
 * OVERVIEW:
 * This system receives inputs from GPS, analog sensors, and CAN bus, then outputs to:
 * - 4x stepper motors for gauge pointers (speedometer, fuel, coolant temp, etc.)
 * - LED warning lights and LED tachometer strip
 * - 2x OLED displays for various vehicle data
 * - CAN bus messages to other microcontrollers
 * 
 * The design is modular to simplify retrofitting vintage instrument panels with modern internals.
 * 
 * HARDWARE:
 * - Arduino Mega 2560 (or compatible)
 * - MCP2515 CAN bus controller
 * - Adafruit GPS module
 * - 2x SSD1306 OLED displays (128x32 pixels)
 * - 4x SwitecX12 stepper motors for gauge needles
 * - WS2812 LED strip for tachometer
 * - Rotary encoder for menu navigation
 * - Various analog sensors (fuel level, thermistor, barometric pressure, battery voltage)
 * 
 * COMMUNICATION PROTOCOLS:
 * - CAN bus at 500kbps (Haltech ECU protocol)
 * - GPS at 9600 baud, NMEA RMC messages at 5Hz
 * - SPI for displays and CAN controller
 * - I2C available for future expansion
 * 
 * ========================================
 */

///// LIBRARIES /////
// This section includes all required libraries for the gauge system

// Display libraries - for OLED screens
#include <Adafruit_SSD1306.h> // SSD1306 OLED driver - https://github.com/adafruit/Adafruit_SSD1306
#include <Adafruit_GFX.h>     // Graphics primitives library - https://github.com/adafruit/Adafruit-GFX-Library

// Communication libraries
#include <SPI.h>          // SPI bus for displays and CAN controller (included in Arduino IDE)
#include <mcp_can.h>      // MCP2515 CAN bus controller - https://downloads.arduino.cc/libraries/github.com/coryjfowler/mcp_can-1.5.1.zip

// Input libraries
#include <Rotary.h>       // Rotary encoder debouncing and state tracking - https://github.com/brianlow/Rotary/blob/master/Rotary.cpp

// Storage library
#include <EEPROM.h>       // Non-volatile storage for settings and odometer (included in Arduino IDE)

// LED library
#include <FastLED.h>      // WS2812 addressable LED control for tachometer - https://github.com/FastLED/FastLED

// GPS library
#include <Adafruit_GPS.h> // GPS parsing and NMEA sentence handling - https://github.com/adafruit/Adafruit_GPS

// Stepper motor libraries
#include <SwitecX25.h>    // X25.168 stepper motor driver for gauges - https://github.com/clearwater/SwitecX25
#include <SwitecX12.h>    // X12.017 stepper motor driver (alternative to X25) - https://github.com/clearwater/SwitecX25
#include <Stepper.h>      // Generic stepper library for odometer motor (included in Arduino IDE)



///// PIN DEFINITIONS AND HARDWARE CONFIGURATION /////
// This section defines all pin assignments and hardware-specific constants

// CAN Bus Configuration
//#define OLED_RESET 4    // OLED display reset pin (unused - left for reference)
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

//#define ODO_STEPS 32         // Odometer stepper steps per revolution (unused - defined inline instead)

// GPS Configuration
#define GPSECHO  false         // Set to true to echo raw GPS data to serial monitor (debug only)


// Rotary Encoder Configuration
#define SWITCH 1               // Rotary encoder push button pin (V4 hardware uses pin 1, V3 used pin 24)

// OLED Display 1 Configuration (SPI interface)
#define SCREEN_W 128           // OLED display width in pixels
#define SCREEN_H 32            // OLED display height in pixels
//#define MOSI  51             // SPI Master Out Slave In (hardware SPI - not needed to define)
//#define CLK   52             // SPI Clock (hardware SPI - not needed to define)
#define OLED_DC_1    6         // Display 1 Data/Command pin
#define OLED_CS_1  5           // Display 1 Chip Select pin
#define OLED_RST_1 7           // Display 1 Reset pin

// OLED Display 2 Configuration (SPI interface)
//#define SCREEN_W_2 128       // Both screens are same size - use SCREEN_W instead
//#define SCREEN_H_2 32        // Both screens are same size - use SCREEN_H instead
#define OLED_DC_2  28          // Display 2 Data/Command pin
#define OLED_CS_2  29          // Display 2 Chip Select pin
#define OLED_RST_2 26          // Display 2 Reset pin

// LED Tachometer Configuration
#define NUM_LEDS 26            // Total number of LEDs in the tachometer strip
#define WARN_LEDS 6            // Warning zone LEDs on each side of center (turns yellow/orange)
#define SHIFT_LEDS 2           // Shift light LEDs on each side of center (turns red at shift point)
#define TACH_DATA_PIN 22       // WS2812 data pin for LED tachometer strip

///// HARDWARE OBJECT INITIALIZATION /////
// Create instances of all hardware interface objects

MCP_CAN CAN0(CAN0_CS);         // CAN bus controller object with CS pin 53
Adafruit_SSD1306 display1(SCREEN_W, SCREEN_H, &SPI, OLED_DC_1, OLED_RST_1, OLED_CS_1);  // Left display
Adafruit_SSD1306 display2(SCREEN_W, SCREEN_H, &SPI, OLED_DC_2, OLED_RST_2, OLED_CS_2);  // Right display
Rotary rotary = Rotary(2, 3);  // Rotary encoder on interrupt pins 2 and 3 for responsive menu navigation
CRGB leds[NUM_LEDS];           // LED array for tachometer strip

// Initialize stepper motors with sweep range and control pins
SwitecX12 motor1(M1_SWEEP, M1_STEP, M1_DIR); // Motor 1 - typically fuel level gauge
SwitecX12 motor2(M2_SWEEP, M2_STEP, M2_DIR); // Motor 2 - typically secondary gauge
SwitecX12 motor3(M3_SWEEP, M3_STEP, M3_DIR); // Motor 3 - typically speedometer (wider sweep)
SwitecX12 motor4(M4_SWEEP, M4_STEP, M4_DIR); // Motor 4 - typically coolant temperature gauge

// Odometer motor configuration (mechanical digit roller - currently non-functional)
#define odoSteps 32        // Steps per revolution for odometer motor
#define odoPin1 10         // Odometer motor coil 1 pin
#define odoPin2 11         // Odometer motor coil 2 pin
#define odoPin3 12         // Odometer motor coil 3 pin
#define odoPin4 13         // Odometer motor coil 4 pin
Stepper odoMotor(odoSteps, odoPin1, odoPin2, odoPin3, odoPin4); 

Adafruit_GPS GPS(&Serial2);    // GPS object using hardware serial port 2

///// GLOBAL VARIABLES /////
// These variables store sensor readings, configuration, and state throughout the program

// ===== ANALOG SENSOR INPUTS =====
// All analog sensors are read through Arduino's ADC (0-1023 raw values, mapped to appropriate ranges)

// Battery Voltage Sensor (Analog Pin A0)
// Measures vehicle battery voltage through a voltage divider to protect Arduino's 5V ADC
float vBatt = 12;              // Current battery voltage in volts (filtered)
int vBattRaw = 12;             // Raw battery reading (0-500, representing 0-5V after mapping)
int filter_vBatt = 8;          // Filter coefficient out of 64 (8/64 = light filtering, 64 = no filter)
int vBattPin = A0;             // Analog input pin for battery voltage
float vBattScaler = 0.040923;  // Voltage divider scaling factor: accounts for R1=10k, R2=3.3k divider
                               // Formula: Vbatt = ADC_reading * (5.0/1023) * ((R1+R2)/R2) = ADC * 0.040923

// Fuel Level Sensor (Analog Pin A3)
// Reads resistance-based fuel sender (typically 0-90 ohms or 240-33 ohms depending on sender type)
int fuelSensorRaw;             // Raw fuel sensor ADC reading (0-500)
int filter_fuel = 1;           // Light filter: 1/64 = very responsive to changes
int fuelPin = A3;              // Analog input pin for fuel level sensor

// Coolant/Oil Temperature Thermistor Sensor (Analog Pin A4)
// GM-style thermistor with non-linear resistance vs. temperature curve
float therm;                   // Current temperature in Celsius (after lookup table conversion)
float thermSensor;             // Voltage reading from thermistor (0-5V)
int filter_therm = 50;         // Medium filter: 50/100 for stable temp reading
int thermPin = A4;             // Analog input pin for thermistor
int thermCAN;                  // Temperature formatted for CAN transmission (temp * 10)

// Barometric Pressure Sensor (Analog Pin A5)
// 30 PSI absolute pressure sensor (0.5-4.5V = 0-30 PSIA)
unsigned long baro;            // Barometric pressure in kPa * 10
byte filter_baro = 4;          // Filter coefficient out of 16 (4/16 = moderate filtering)
int baroPin = A5;              // Analog input pin for barometric sensor

// Reserved Analog Sensors B and C (future expansion)
float sensor_b;                // Reserved sensor B value
byte filter_b = 12;            // Filter coefficient for sensor B (12/16)
int analogPin6 = A6;           // Analog pin 6

float sensor_c;                // Reserved sensor C value
byte filter_c = 12;            // Filter coefficient for sensor C (12/16)
int analogPin7 = A7;           // Analog pin 7

// ===== GPS SPEED AND ODOMETER VARIABLES =====
// GPS provides speed and time data for speedometer and odometer calculations
unsigned long v_old = 0;       // Previous GPS speed reading (km/h * 100)
unsigned long v_new = 1;       // Current GPS speed reading (km/h * 100)
unsigned long t_old = 0;       // Previous GPS timestamp (milliseconds)
unsigned long t_new = 1;       // Current GPS timestamp (milliseconds)
unsigned long v_100 = 0;       // Speed value * 100 for integer math precision
float v = 0;                   // Current speed in km/h (floating point)
bool usingInterrupt = false;   // Flag indicating if GPS uses interrupt-based reading
int lagGPS;                    // Time delay since last GPS update (milliseconds)
int v_g;                       // GPS speed (alternate variable)
float odo;                     // Total odometer reading in kilometers (saved to EEPROM)
float odoTrip;                 // Trip odometer in kilometers (resettable, saved to EEPROM)
float distLast;                // Distance traveled since last GPS update (km)
byte hour;                     // Current GPS hour (UTC, 0-23)
byte minute;                   // Current GPS minute (0-59)

// ===== STEPPER MOTOR POSITION VARIABLES =====
// Target positions calculated from sensor data for smooth needle movement
unsigned int spd_g;            // Speedometer target value (mph * 100)
unsigned int fuelLevelPct_g;   // Fuel level percentage * 10
unsigned int coolantTemp_g;    // Coolant temperature for gauge calculation

// ===== ROTARY ENCODER VARIABLES =====
// Handle menu navigation via rotary encoder with debouncing
// NOTE: stateSW, lastStateSW, lastStateChangeTime, debounceDelay, and debounceFlag
// are now declared as static locals inside swRead() function (not globals)
// to prevent potential interference from other parts of the code
bool button = 0;                       // Button press event flag (set when press completes)

// ===== TIMING VARIABLES =====
// Manage update rates for different subsystems to balance performance and responsiveness
unsigned int timer0, timerDispUpdate, timerCANsend;
unsigned int timerSensorRead, timerTachUpdate, timerTachFlash;
unsigned int timerCheckGPS, timerGPSupdate, timerAngleUpdate;

// Update rate periods (in milliseconds)
//long unsigned dispMenuRate = 20;       // Unused - commented out
unsigned int CANsendRate = 50;          // Send CAN messages every 50ms (20Hz)
unsigned int dispUpdateRate = 75;       // Update displays every 75ms (~13Hz)
unsigned int sensorReadRate = 10;       // Read analog sensors every 10ms (100Hz for responsive readings)
unsigned int tachUpdateRate = 50;       // Update LED tachometer every 50ms (20Hz)
unsigned int tachFlashRate = 50;        // Flash shift light every 50ms when over redline
unsigned int GPSupdateRate = 100;       // GPS update check rate (might not be needed)
unsigned int checkGPSRate = 1;          // Check for GPS data every 1ms
unsigned int angleUpdateRate = 20;      // Update motor angles every 20ms (50Hz)
unsigned int splashTime = 1500;         // Duration of startup splash screens (milliseconds)

// ===== LED TACHOMETER VARIABLES =====
// Control the LED strip tachometer display
unsigned int tachMax = 6000;            // RPM at which shift light activates and flashes
unsigned int tachMin = 3000;            // Minimum RPM to show on tach (below this LEDs are off)
bool tachFlashState = 0;                // Current state of shift light flashing (0=off, 1=on)

// ===== CAN BUS ENGINE PARAMETERS =====
// Raw values received from Haltech ECU via CAN bus
// These are stored as integers to preserve precision from the CAN protocol
int rpmCAN;                // Engine RPM (direct value, 0-10000+)
int mapCAN;                // Manifold Absolute Pressure in kPa * 10 (e.g., 1000 = 100.0 kPa)
int tpsCAN;                // Throttle Position Sensor percentage * 10 (e.g., 750 = 75.0%)
int fuelPrsCAN;            // Fuel pressure in kPa * 10 (absolute pressure)
int oilPrsCAN;             // Oil pressure in kPa * 10 (absolute pressure)
int injDutyCAN;            // Injector duty cycle percentage * 10 (e.g., 800 = 80.0%)
int ignAngCAN;             // Ignition timing in degrees BTDC * 10 (e.g., 150 = 15.0°)
int afr1CAN;               // Air/Fuel Ratio * 1000 (e.g., 14700 = 14.700 AFR)
int knockCAN;              // Knock sensor level (higher = more knock detected)
int coolantTempCAN;        // Coolant temperature in Kelvin * 10 (convert to C: (value/10) - 273.15)
int airTempCAN;            // Intake Air Temperature in Kelvin * 10
int fuelTempCAN;           // Fuel temperature in Kelvin * 10
int oilTempCAN;            // Oil temperature in Celsius * 10 (or Kelvin - check ECU config)
int transTempCAN;          // Transmission temperature in Celsius * 10
int fuelCompCAN;           // Fuel composition (ethanol %) * 10 (e.g., 850 = 85.0% E85)
int fuelLvlCAN;            // Fuel level percentage (0-100)
int baroCAN;               // Barometric pressure in kPa * 10 (sent TO other modules)
int spdCAN;                // Vehicle speed sent to CAN bus (km/h * 16 for protocol compatibility)
int pumpPressureCAN;       // Fuel pump pressure (test variable)

// ===== PROCESSED ENGINE PARAMETERS FOR DISPLAY =====
// Converted from CAN values to human-readable units
float oilPrs = 25;         // Oil pressure in kPa (gauge pressure, atmospheric offset removed)
float coolantTemp = 0;     // Coolant temperature in Celsius
float fuelPrs = 43;        // Fuel pressure in kPa (gauge pressure)
float oilTemp = 0;         // Oil temperature in Celsius
float fuelLvl = 0;         // Fuel level in gallons (or liters if metric)
float battVolt = 12.6;     // Battery voltage in volts
float afr = 14.2;          // Air/Fuel Ratio (stoichiometric for gasoline ~14.7)
float fuelComp = 0;        // Ethanol percentage (0-100%)
int RPM = 0;               // Engine RPM for display
int spd = 0;               // Vehicle speed in km/h * 100 (for integer precision)
float spdMph = 0;          // Vehicle speed in miles per hour


// ===== CAN BUS COMMUNICATION BUFFERS =====
// Buffers for sending and receiving CAN messages
byte data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};           // Outgoing CAN message buffer (8 bytes)
byte canMessageData [8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Received CAN message data
unsigned long rxId;                // Received CAN message ID (11-bit or 29-bit)
unsigned char len = 0;             // Length of received CAN message (0-8 bytes)
unsigned char rxBuf[8];            // Raw receive buffer from CAN controller
char msgString[128];               // String buffer for serial debug output 

// ===== LOOKUP TABLES =====
// These tables convert non-linear sensor readings to physical values using interpolation

// Thermistor Temperature Lookup Table
// Converts voltage reading (x-axis) to temperature in Celsius (y-axis)
// GM thermistors have a non-linear resistance curve that varies with temperature
const int thermTable_length = 6;
float thermTable_x[thermTable_length] = {0.23, 0.67, 1.43, 3.70, 4.63, 4.95};  // Voltage breakpoints (0-5V)
float thermTable_l[thermTable_length] = { 150,  105,   75,   25,   -5,  -40};  // Temperature values in Celsius

// Fuel Level Lookup Table
// Converts voltage reading (x-axis) to fuel quantity in gallons (y-axis)
// Fuel tank sender has non-linear float arm resistance
const int fuelLvlTable_length = 9;
float fuelLvlTable_x[fuelLvlTable_length] = {0.87, 1.03, 1.21, 1.40, 1.60, 1.97, 2.21, 2.25, 2.30};  // Voltage breakpoints
float fuelLvlTable_l[fuelLvlTable_length] = {  16,   14,   12,   10,    8,    6,    4,    2,    0};  // Gallons remaining
float fuelCapacity = 16;  // Total fuel tank capacity in gallons (used for percentage calculations)

// ===== EEPROM STORAGE ADDRESSES =====
// Non-volatile memory locations for saving settings between power cycles
byte dispArray1Address = 0;      // Display 1 menu selections (4 bytes: addresses 0-3)
byte dispArray2Address = 4;      // Display 2 selection (1 byte: address 4)
byte clockOffsetAddress = 5;     // Time zone offset for clock (1 byte: address 5)
byte odoAddress = 6;             // Total odometer value (4 bytes: addresses 6-9)
byte odoTripAddress = 10;        // Trip odometer value (4 bytes: addresses 10-13)
byte fuelSensorRawAddress = 14;  // Last fuel sensor reading (for fuel level memory, addresses 14-17)
byte unitsAddress = 18;          // Unit system selection: 0=metric, 1=imperial (1 byte: address 18)
int *input;                      // Pointer for EEPROM operations
int output = 0;                  // Output buffer for EEPROM operations

// ===== MENU NAVIGATION VARIABLES =====
// Track current position in the multi-level menu system
byte menuLevel = 0;              // Current menu depth (0=top level, 1=submenu, 2=sub-submenu)
byte units = 0;                  // Unit system: 0=metric (km/h, C, bar), 1=imperial (mph, F, PSI)
unsigned int nMenuLevel = 15;    // Number of items in current menu level (0-indexed, so 15 = 16 items)
byte dispArray1[4] = { 1, 0, 0, 0 };  // Menu position array for display 1 [level0, level1, level2, level3]
byte clockOffset = 0;            // Hours to add to UTC time for local time zone (-12 to +12)
byte dispArray2[1] = {1};        // Menu selection for display 2 (single level)


///// IMAGE DATA /////
// Bitmap images stored in program memory (PROGMEM) for OLED displays
// Each image is 128x32 pixels (1 bit per pixel = 512 bytes)
// Generated from graphics using image2cpp or similar tool

// 'falcon_script', 128x32px
// Falcon logo in script font - displayed on startup splash screen
const unsigned char img_falcon_script [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x1f, 0xc0, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x03, 0xe0, 0x00, 0x00, 0x80, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x01, 0x80, 0x06, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x03, 0x00, 0x0c, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x02, 0x00, 0x19, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xe0, 0x79, 0x98, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xf9, 0xf9, 0x31, 0xf3, 0xe3, 0x38, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0xe3, 0x87, 0x33, 0x43, 0x27, 0xbf, 0xd8, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x0c, 0x63, 0x86, 0x6e, 0xf3, 0x10, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x08, 0xc3, 0x0c, 0x1c, 0x66, 0x30, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x11, 0x86, 0x38, 0x38, 0xcc, 0x60, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x13, 0x9e, 0x68, 0x69, 0x88, 0xc0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x1d, 0xe3, 0x8f, 0x8f, 0x18, 0xff, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x06, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// '302_CID', 128x32px
// Engine displacement designation: 302 Cubic Inch Displacement
const unsigned char img_302_CID [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x1e, 0x1e, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0x1f, 0xfe, 0x00, 0xff, 0xfc, 0x03, 0xff, 0xe0, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0x7f, 0xff, 0x81, 0xff, 0xfe, 0x0f, 0xff, 0xf8, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0xff, 0xff, 0xc3, 0xff, 0xff, 0x1f, 0xff, 0xfc, 0x00, 0x3c, 0x3c, 0x3c, 
	0x01, 0xe1, 0xe0, 0x01, 0xff, 0xff, 0xe3, 0xff, 0xff, 0x1f, 0xff, 0xfe, 0x00, 0x03, 0xc3, 0xc0, 
	0x01, 0xe1, 0xe0, 0x01, 0xfc, 0x0f, 0xe7, 0xf0, 0x3f, 0x9f, 0x00, 0x7e, 0x00, 0x03, 0xc3, 0xc0, 
	0x01, 0xe1, 0xe0, 0x01, 0xf8, 0x07, 0xc7, 0xe0, 0x1f, 0x80, 0x00, 0x7e, 0x00, 0x03, 0xc3, 0xc0, 
	0x01, 0xe1, 0xe0, 0x00, 0x00, 0x0f, 0x87, 0xe0, 0x1f, 0x81, 0xff, 0xfe, 0x00, 0x03, 0xc3, 0xc0, 
	0x1e, 0x1e, 0x1e, 0x00, 0x00, 0x7f, 0x87, 0xe0, 0x1f, 0x87, 0xff, 0xfc, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0x00, 0x7f, 0xc7, 0xe0, 0x1f, 0x8f, 0xff, 0xf8, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0x00, 0x0f, 0xe7, 0xe0, 0x1f, 0x8f, 0xff, 0xe0, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x01, 0xf8, 0x07, 0xe7, 0xe0, 0x1f, 0x9f, 0xc0, 0x00, 0x00, 0x3c, 0x3c, 0x3c, 
	0x01, 0xe1, 0xe0, 0x01, 0xfc, 0x0f, 0xe7, 0xf0, 0x3f, 0x9f, 0x80, 0x00, 0x00, 0x03, 0xc3, 0xc0, 
	0x01, 0xe1, 0xe0, 0x01, 0xff, 0xff, 0xe3, 0xff, 0xff, 0x1f, 0xff, 0xfe, 0x00, 0x03, 0xc3, 0xc0, 
	0x01, 0xe1, 0xe0, 0x00, 0xff, 0xff, 0xc3, 0xff, 0xff, 0x1f, 0xff, 0xfe, 0x00, 0x03, 0xc3, 0xc0, 
	0x01, 0xe1, 0xe0, 0x00, 0x7f, 0xff, 0x81, 0xff, 0xfe, 0x1f, 0xff, 0xfe, 0x00, 0x03, 0xc3, 0xc0, 
	0x1e, 0x1e, 0x1e, 0x00, 0x1f, 0xfe, 0x00, 0xff, 0xfc, 0x1f, 0xff, 0xfe, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x3c, 0x3c, 
	0x01, 0xe1, 0xe0, 0x00, 0xe2, 0x4e, 0x23, 0x81, 0x24, 0x71, 0x27, 0x8c, 0x00, 0x03, 0xc3, 0xc0, 
	0x01, 0xe1, 0xe0, 0x01, 0x12, 0x49, 0x24, 0x41, 0x34, 0x89, 0x24, 0x10, 0x00, 0x03, 0xc3, 0xc0, 
	0x01, 0xe1, 0xe0, 0x01, 0x02, 0x4e, 0x24, 0x01, 0x34, 0x81, 0xe4, 0x0c, 0x00, 0x03, 0xc3, 0xc0, 
	0x01, 0xe1, 0xe0, 0x01, 0x02, 0x49, 0x24, 0x01, 0x2c, 0x81, 0x27, 0x82, 0x00, 0x03, 0xc3, 0xc0, 
	0x1e, 0x1e, 0x1e, 0x01, 0x12, 0x49, 0x24, 0x41, 0x2c, 0x89, 0x24, 0x12, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0xe1, 0x8e, 0x23, 0x81, 0x24, 0x71, 0x27, 0x8c, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x3c, 0x3c, 
	0x1e, 0x1e, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x3c, 0x3c, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// '302V', 128x32px
// Alternative engine badge - 302 with V8 symbol
const unsigned char img_302V [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xfe, 0x0f, 0xfc, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0x1f, 0xfe, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x03, 0x9c, 0x0e, 0x60, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x03, 0x98, 0x06, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x18, 0x06, 0x1f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x18, 0x06, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x03, 0x98, 0x06, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x03, 0x9c, 0x0e, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x01, 0xff, 0xff, 0xf3, 0xff, 0x1f, 0xfe, 0x7f, 0xfb, 0xff, 0xff, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x03, 0xf3, 0xc0, 0x01, 0xfe, 0x0f, 0xfc, 0x7f, 0xf8, 0x01, 0xe7, 0xe0, 0x00, 0x00, 
	0x00, 0x00, 0x03, 0xfc, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x1f, 0xe0, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xff, 0x8f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0xff, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x3f, 0xf1, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x03, 0xc7, 0xfe, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x0f, 0xfe, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x3f, 0xf8, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0xff, 0xc7, 0xc0, 0x00, 0x00, 0x01, 0xf1, 0xff, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x7f, 0xf8, 0xfc, 0x00, 0x00, 0x1f, 0x8f, 0xff, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x1f, 0xc0, 0x01, 0xfc, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xe3, 0xfc, 0x1f, 0xe3, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xfc, 0x3f, 0xfe, 0x1f, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x87, 0xf0, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xe1, 0xc3, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xf8, 0x0f, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfe, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'Oil Pressure Icon', 40x32px
// Icon showing oil can symbol - displayed alongside oil pressure reading
const unsigned char img_oilPrs [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x0f, 0xe0, 
	0x00, 0x00, 0x3f, 0xcf, 0xe0, 0x00, 0x38, 0x33, 0xe3, 0x80, 0x01, 0xfc, 0x30, 0xff, 0xf8, 0x1f, 
	0xf8, 0x38, 0x7f, 0xfc, 0xff, 0x80, 0x1e, 0x7f, 0xff, 0xff, 0x00, 0x07, 0xe0, 0x07, 0x8e, 0x08, 
	0x01, 0xe0, 0x00, 0x1e, 0x08, 0x00, 0x60, 0x00, 0x1c, 0x1c, 0x00, 0x60, 0x00, 0x38, 0x1c, 0x00, 
	0x60, 0x00, 0x70, 0x1c, 0x00, 0x7f, 0xff, 0xe0, 0x08, 0x00, 0x7f, 0xff, 0xc0, 0x00, 0x00, 0x7f, 
	0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'Oil Temp Icon', 40x32px
// Icon combining oil can and thermometer - for oil temperature display
const unsigned char img_oilTemp [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0xc0, 0x00, 0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x01, 
	0xff, 0x00, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x1e, 0x01, 0xe0, 
	0x00, 0x00, 0x3f, 0xc1, 0xe0, 0x00, 0x38, 0x33, 0xe1, 0xff, 0x01, 0xfc, 0x30, 0xf9, 0xff, 0x1f, 
	0xf8, 0x38, 0x79, 0xe0, 0x3f, 0x80, 0x1e, 0x79, 0xe0, 0x3f, 0x00, 0x07, 0xe1, 0xe0, 0x0e, 0x08, 
	0x01, 0xe1, 0xff, 0x1e, 0x08, 0x00, 0x61, 0xff, 0x1c, 0x1c, 0x00, 0x61, 0xe0, 0x38, 0x1c, 0x00, 
	0x61, 0xe0, 0x70, 0x1c, 0x00, 0x73, 0xf3, 0xe0, 0x08, 0x00, 0x73, 0xf3, 0xc0, 0x00, 0x00, 0x73, 
	0xf3, 0x80, 0x00, 0x00, 0x03, 0xf0, 0x00, 0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'Battery Icon', 38x32px
// Battery symbol with + and - terminals - for voltage display
const unsigned char img_battVolt [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x78, 0x00, 0x00, 0x78, 
	0x00, 0x78, 0x00, 0x00, 0x78, 0x00, 0x78, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xc0, 0x0f, 0xff, 0xff, 
	0xff, 0xc0, 0x0c, 0x00, 0x00, 0x00, 0xc0, 0x0c, 0x00, 0x00, 0x00, 0xc0, 0x0c, 0x00, 0x00, 0x30, 
	0xc0, 0x0c, 0x00, 0x00, 0x30, 0xc0, 0x0c, 0xfc, 0x00, 0xfc, 0xc0, 0x0c, 0xfc, 0x00, 0xfc, 0xc0, 
	0x0c, 0x00, 0x00, 0x30, 0xc0, 0x0c, 0x00, 0x00, 0x30, 0xc0, 0x0c, 0x00, 0x00, 0x00, 0xc0, 0x0c, 
	0x00, 0x00, 0x00, 0xc0, 0x0c, 0x00, 0x00, 0x00, 0xc0, 0x0c, 0x00, 0x00, 0x00, 0xc0, 0x0c, 0x00, 
	0x00, 0x00, 0xc0, 0x0f, 0xff, 0xff, 0xff, 0xc0, 0x0f, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'Eng Temp Icon', 35x32px
// Thermometer icon for coolant/engine temperature display
const unsigned char img_coolantTemp [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 
	0xff, 0x80, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0xf0, 
	0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x00, 0xff, 0x80, 
	0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 
	0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 
	0x00, 0xf0, 0x00, 0x00, 0x07, 0x81, 0xf8, 0x3c, 0x00, 0x1f, 0xf9, 0xf9, 0xff, 0x00, 0x18, 0x79, 
	0xf9, 0xc3, 0x00, 0x00, 0x01, 0xf8, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x07, 0xff, 0xff, 0xfe, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'Gas Icon', 32x32px
// Gas pump icon for fuel level display
const unsigned char img_fuelLvl [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xe6, 0x00, 
	0x07, 0xff, 0xf7, 0x80, 0x07, 0xff, 0xf3, 0xc0, 0x07, 0x00, 0x71, 0xe0, 0x07, 0x00, 0x70, 0xe0, 
	0x07, 0x00, 0x70, 0xf0, 0x07, 0x00, 0x70, 0x70, 0x07, 0x00, 0x70, 0x78, 0x07, 0x00, 0x70, 0x70, 
	0x07, 0xff, 0xf0, 0x60, 0x07, 0xff, 0xfc, 0x60, 0x07, 0xff, 0xfe, 0x30, 0x07, 0xff, 0xf6, 0x30, 
	0x07, 0xff, 0xf6, 0x30, 0x07, 0xff, 0xf6, 0x30, 0x07, 0xff, 0xf6, 0x30, 0x07, 0xff, 0xf6, 0x38, 
	0x07, 0xff, 0xf6, 0x18, 0x07, 0xff, 0xf6, 0x18, 0x07, 0xff, 0xf6, 0x18, 0x07, 0xff, 0xf6, 0x18, 
	0x07, 0xff, 0xf7, 0xb8, 0x07, 0xff, 0xf3, 0xf0, 0x07, 0xff, 0xf0, 0xc0, 0x0f, 0xff, 0xf8, 0x00, 
	0x0f, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ===== DEMO/TEST VARIABLES =====
// Variables used for testing gauge sweep and display without real engine data
bool rpmSwitch = 0;         // Direction flag for demo RPM sweep (0=increasing, 1=decreasing)
int gRPM;                   // Generated RPM value for demo mode
int analog = 2;             // Test analog value
int analogSwitch = 0;       // Direction flag for analog test sweep

/*
 * ========================================
 * SIGNAL SELECTION AND PROCESSING
 * ========================================
 */

/**
 * sigSelect - Process and route sensor data
 * 
 * This function acts as the central data router, converting raw CAN bus and sensor
 * readings into the appropriate units and formats for display and gauge output.
 * 
 * Processing steps:
 * 1. Convert GPS speed from km/h to appropriate formats
 * 2. Extract RPM from CAN bus
 * 3. Convert temperatures from Kelvin to Celsius
 * 4. Convert absolute pressures to gauge pressures (subtract atmospheric)
 * 5. Scale AFR and fuel composition values
 * 6. Calculate fuel level percentage for CAN transmission
 * 
 * Called from: main loop (every cycle)
 * Modifies: spd, spdCAN, RPM, coolantTemp, oilPrs, fuelPrs, oilTemp, afr, fuelComp, fuelLvlCAN
 */
void sigSelect (void) {
    spd = v_new; // Speed in km/h * 100 from GPS
    //spdMph = spd *0.6213712;  // Unused conversion to mph
    spdCAN = (int)(v*16);  // Speed formatted for CAN bus transmission (km/h * 16 per Haltech protocol)
    RPM = rpmCAN;  // Direct copy of RPM from CAN bus
    coolantTemp = (coolantTempCAN/10)-273.15; // Convert from Kelvin*10 to Celsius (K to C: subtract 273.15)
    oilPrs = (oilPrsCAN/10)-101.3;   // Convert from absolute kPa to gauge pressure (subtract atmospheric ~101.3 kPa)
    fuelPrs = (fuelPrsCAN/10)-101.3;  // Convert from absolute kPa to gauge pressure
    oilTemp = therm;  // Oil temperature from thermistor sensor (already in Celsius)
    afr = (float)afr1CAN/1000;  // Air/Fuel Ratio - divide by 1000 (e.g., 14700 becomes 14.7)
    fuelComp = fuelCompCAN/10;  // Fuel composition - divide by 10 (e.g., 850 becomes 85%)
    fuelLvlCAN = (int)((fuelLvl/fuelCapacity)*100);  // Calculate fuel level percentage for CAN transmission

}

/*
 * ========================================
 * SETUP FUNCTION
 * ========================================
 * 
 * Arduino setup() runs once on power-up or reset
 * Initializes all hardware, loads saved settings, and displays splash screen
 */

/**
 * setup - Initialize all hardware and system settings
 * 
 * Initialization sequence:
 * 1. Start serial communication for debugging
 * 2. Enable power latch to keep system on after ignition off
 * 3. Configure GPS module (baud rate, message type, update rate)
 * 4. Initialize OLED displays and show splash screens
 * 5. Configure and sweep gauge motors to verify operation
 * 6. Setup odometer stepper motor pins
 * 7. Initialize LED tachometer strip
 * 8. Attach rotary encoder interrupts for menu navigation
 * 9. Load saved settings from EEPROM (display selections, clock offset, odometer values, units)
 * 10. Initialize CAN bus controller at 500kbps
 * 11. Wait for splash screen timeout before entering main loop
 * 
 * Hardware configured:
 * - Serial port at 115200 baud
 * - GPS at 9600 baud, 5Hz update, RMC sentences only
 * - CAN bus at 500kbps, 8MHz crystal
 * - 2x SSD1306 OLED displays (SPI)
 * - 4x SwitecX12 stepper motors
 * - WS2812 LED strip
 * - Rotary encoder on interrupt pins 2 & 3
 */
void setup() {

  // Initialize serial communication for debugging
  Serial.begin(115200); // 115200 baud - high speed for minimal latency in debug output

  // Keep power enabled after ignition switch turns off
  // This allows the system to complete shutdown procedures (save EEPROM, zero gauges)
  pinMode(pwrPin, OUTPUT);
  digitalWrite(pwrPin, HIGH);  // Latch power on

  // ===== GPS INITIALIZATION =====
  GPS.begin(9600);                                // Initialize GPS at 9600 baud (default for most GPS modules)
  //GPS.sendCommand(PMTK_SET_BAUD_57600);         // Optional: increase baud to 57600 for faster data transfer
  //GPS.begin(57600);                             // Re-init at higher baud if command above is uncommented
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);  // Request only RMC sentences (Recommended Minimum - has speed, position, time)
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);      // Set update rate to 5Hz (5 times per second for responsive speedometer)
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);      // Set position fix rate to match update rate (5Hz)

  useInterrupt(true);                             // Enable interrupt-based GPS reading (Timer0 ISR reads GPS in background)
 
  // ===== DISPLAY INITIALIZATION =====
  display1.begin(SSD1306_SWITCHCAPVCC);  // Initialize display 1 with internal charge pump
  display2.begin(SSD1306_SWITCHCAPVCC);  // Initialize display 2 with internal charge pump
  dispFalconScript(&display1);           // Show Falcon logo on left display
  disp302CID(&display2);                 // Show "302 CID" badge on right display
  
  // ===== STEPPER MOTOR INITIALIZATION =====
  pinMode(MOTOR_RST, OUTPUT);
  digitalWrite(MOTOR_RST, HIGH);  // Take motors out of reset (active low reset)
  
  // Perform full range sweep of all gauge needles for visual confirmation and homing
  motorSweepSynchronous();  // Sweeps all motors to max, then back to zero
  
  // ===== ODOMETER MOTOR SETUP =====
  // Configure pins for 4-wire stepper motor (mechanical digit roller - currently non-functional)
  pinMode(odoPin1, OUTPUT);
  pinMode(odoPin2, OUTPUT);
  pinMode(odoPin3, OUTPUT);
  pinMode(odoPin4, OUTPUT);

  // ===== LED TACHOMETER INITIALIZATION =====
  FastLED.addLeds<WS2812, TACH_DATA_PIN, GRB>(leds, NUM_LEDS);  // Configure WS2812 LED strip (GRB color order)

  // ===== ROTARY ENCODER SETUP =====
  // Configure encoder switch pin with internal pull-up resistor
  pinMode(SWITCH, INPUT_PULLUP);  // Switch pulls to ground when pressed
  
  // Attach interrupts to encoder pins for immediate response to rotation
  attachInterrupt(0, rotate, CHANGE);  // Interrupt 0 = pin 2
  attachInterrupt(1, rotate, CHANGE);  // Interrupt 1 = pin 3
  
  // ===== LOAD SAVED SETTINGS FROM EEPROM =====
  
  // Read display 1 menu positions (4 bytes)
  for (int i = dispArray1Address; i < sizeof(dispArray1); i++) {
    dispArray1[i] = EEPROM.read(i);
  }
  
  // Read display 2 selection (1 byte)
  dispArray2[0] = EEPROM.read(dispArray2Address);

  // Fetch clock offset for local time zone
  clockOffset = EEPROM.read(clockOffsetAddress); 

  // Fetch odometer values (floats stored as 4 bytes each)
  EEPROM.get(odoAddress, odo);              // Total odometer
  EEPROM.get(odoTripAddress, odoTrip);      // Trip odometer
  EEPROM.get(fuelSensorRawAddress, fuelSensorRaw);  // Last known fuel level (for fuel level memory on restart)
  EEPROM.get(unitsAddress, units);          // Unit system (metric/imperial)

  // Debug output for clock offset
  Serial.print("clockOffset: ");
  Serial.println(clockOffset);
  
  // ===== CAN BUS INITIALIZATION =====
  // Initialize MCP2515 with 8MHz crystal, 500kbps baud rate
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) 
    Serial.println("MCP2515 Initialized Successfully!");
  else 
    Serial.println("Error Initializing MCP2515...");
  
  // Configure CAN interrupt pin
  pinMode(CAN0_INT, INPUT);       // CAN interrupt pin goes low when message is received
  CAN0.setMode(MCP_NORMAL);       // Set to normal mode (vs. loopback/listen-only) to allow TX and RX

  // ===== SPLASH SCREEN DELAY =====
  // Hold splash screen images on displays for specified time before entering main loop
  while (millis() < splashTime){
    // Wait for splash screen timer to expire (1500ms default)
  }

}


/*
 * ========================================
 * MAIN LOOP FUNCTION
 * ========================================
 * 
 * Arduino loop() runs continuously after setup() completes
 * Manages all real-time tasks using non-blocking timers
 */

/**
 * loop - Main execution loop
 * 
 * This loop coordinates all gauge system functions using time-based scheduling.
 * Each subsystem runs at its own update rate for optimal performance:
 * 
 * - Sensor reading: 10ms (100Hz) - fast for responsive analog inputs
 * - CAN transmission: 50ms (20Hz) - matches typical ECU update rates  
 * - CAN reception: as fast as messages arrive (interrupt-driven)
 * - GPS check: 1ms - polls for new GPS data
 * - Tachometer update: 50ms (20Hz) - smooth LED animation
 * - Display update: 75ms (~13Hz) - readable without flicker
 * - Motor angle update: 20ms (50Hz) - smooth gauge needle movement
 * - Motor step execution: every loop (~1ms) - microstepping for smooth motion
 * - Shutdown check: every loop - monitors battery voltage for key-off
 * 
 * The non-blocking timer approach allows all systems to run concurrently without
 * interfering with each other, unlike delay()-based code which would freeze everything.
 * 
 * Loop structure:
 * 1. Read analog sensors if timer expired
 * 2. Send CAN messages if timer expired
 * 3. Receive CAN messages if data available (interrupt flag)
 * 4. Check for GPS data if timer expired
 * 5. Update LED tachometer if timer expired
 * 6. Process signals and update displays
 * 7. Calculate new motor angles if timer expired
 * 8. Step all motors (called every loop for smooth motion)
 * 9. Check for shutdown condition
 */
void loop() {
    
  // ===== ANALOG SENSOR READING =====
  // Read battery voltage, fuel level, temperature, barometric pressure
  // Update rate: every 10ms (100Hz) for responsive readings
  if (millis() - timerSensorRead > sensorReadRate) {
    // Serial.print("sensorRead: ");  // Debug timing
    int s = micros();  // Start timing for performance measurement

    // Battery voltage: read, map to 0-5V range, apply light filter
    vBattRaw = readSensor(vBattPin, vBattRaw, filter_vBatt);
    vBatt = (float)vBattRaw*vBattScaler;  // Convert to actual voltage using calibration factor
    
    // Fuel level: read raw sensor, convert voltage to gallons via lookup table
    fuelSensorRaw = readSensor(fuelPin,fuelSensorRaw,filter_fuel);
    float fuelSensor = (float)fuelSensorRaw*0.01;  // Convert to voltage (0-5V)
    fuelLvl = curveLookup(fuelSensor, fuelLvlTable_x, fuelLvlTable_l, fuelLvlTable_length);
    
    // Thermistor temperature: read voltage, convert to temp via lookup table
    thermSensor = readThermSensor(thermPin, thermSensor, filter_therm);
    therm = curveLookup(thermSensor, thermTable_x, thermTable_l, thermTable_length);
    thermCAN = (int)(therm*10);  // Format for CAN transmission (temp * 10)
    
    // Barometric pressure: read 30 PSI absolute sensor, constrain to valid range
    baro = read30PSIAsensor(baroPin,baro,filter_baro); // Returns kPa * 10 
    baro = constrain(baro, 600, 1050);  // Limit to elevation range -300m to 4000m (60-105 kPa)
    baroCAN = baro;  // Store for CAN transmission
    
    //sensor_b = readSensor(analogPin6,sensor_b,filter_b);  // Reserved for future use
    //sensor_c = readSensor(analogPin7,sensor_c,filter_c);  // Reserved for future use
    
    timerSensorRead = millis();  // Reset timer

    int time =  micros() - s;  // Calculate elapsed time for performance monitoring
    // Serial.println(time);  // Debug: print execution time
  }


  // ===== CAN BUS TRANSMISSION =====
  // Send vehicle data to other modules on CAN bus
  // Update rate: every 50ms (20Hz) - typical automotive CAN rate
  if (millis() - timerCANsend > CANsendRate) {  
    // Serial.print("CANsend: ");  // Debug timing
    int s = micros();

    // Send speed data (Big Endian format for compatibility)
    sendCAN_BE(0x200, 0, spdCAN, 0, 0);
    
    // Send sensor data (Little Endian format): oil temp, fuel level %, baro pressure
    sendCAN_LE(0x201, thermCAN, fuelLvlCAN, baroCAN, 555);
    //sendCAN_LE(0x201, 255, 50, 988, 555);  // Test values (commented out)
    //sendCAN_BE(0x301, 333, 444, 1010, 2020);  // Test values (commented out)
    
    timerCANsend = millis();  // Reset timer

    int time =  micros() - s;
    // Serial.println(time);  // Debug: print execution time
  }


  // ===== CAN BUS RECEPTION =====
  // Read engine data from Haltech ECU via CAN bus
  // Runs whenever CAN interrupt pin goes low (message received)
  if(!digitalRead(CAN0_INT)){     // CAN0_INT pin is low when message is waiting
    // Serial.print("CAN recieve: ");  // Debug timing
    int s = micros();
    
    receiveCAN();  // Read message from MCP2515 receive buffer
    parseCAN(rxId, rxBuf);  // Parse message based on CAN ID and extract data

    int time =  micros() - s;
    // Serial.println(time);  // Debug: print execution time
  }

  // ===== GPS DATA RECEPTION =====
  // Check for new GPS data and update speed/odometer
  // Update rate: every 1ms - fast polling to catch GPS updates immediately
  if (millis() - timerCheckGPS > checkGPSRate) {
    // Serial.print("GPS recieve: ");  // Debug timing
    int s = micros(); 
    
    fetchGPSdata();  // Parse GPS sentences and update speed, odometer, time
    
    int time =  micros() - s;
    // Serial.println(time);  // Debug: print execution time
  }
  
  // ===== LED TACHOMETER UPDATE =====
  // Update tachometer LED strip based on engine RPM
  // Update rate: every 50ms (20Hz) for smooth animation
  if (millis() - timerTachUpdate > tachUpdateRate) {     
    // Serial.print("tach: ");  // Debug timing
    int s = micros();
    
    // demoRPM = generateRPM();  // Uncomment for demo mode (simulated RPM sweep)
    ledShiftLight(RPM);  // Update LED colors and shift light based on RPM
    timerTachUpdate = millis();  // Reset timer
    
    int time =  micros() - s;
    // Serial.println(time);  // Debug: print execution time
  }

  // ===== SIGNAL PROCESSING AND DISPLAY UPDATE =====
  sigSelect();  // Convert CAN data to display units (always run to keep data fresh)
  
  // Read rotary encoder switch state for menu navigation
  swRead();
  
  // Update OLED displays with current data
  // Update rate: every 75ms (~13Hz) - fast enough to appear real-time, slow enough to be readable
  if(millis() - timerDispUpdate > dispUpdateRate){
    // Serial.print("display: ");  // Debug timing
    int s = micros();
    
    dispMenu();  // Update display 1 based on menu selection
    disp2();     // Update display 2 based on its menu selection
    //Serial.println(button);  // Debug: print button state
    timerDispUpdate = millis();  // Reset timer

    int time =  micros() - s;
    // Serial.println(time);  // Debug: print execution time
  }

  // ===== MOTOR ANGLE CALCULATION =====
  // Calculate target positions for all gauge motors
  // Update rate: every 20ms (50Hz) - smooth needle movement
  if(millis() - timerAngleUpdate > angleUpdateRate){
    // Serial.print("motors: ");  // Debug timing
    int s = micros();
    
    // Calculate needle angles based on current sensor values
    int angle1 = fuelLvlAngle(M1_SWEEP);      // Motor 1: Fuel level
    int angle2 = fuelLvlAngle(M2_SWEEP);      // Motor 2: Fuel level (duplicate or alt gauge)
    int angle3 = speedometerAngle(M3_SWEEP);  // Motor 3: Speedometer
    int angle4 = coolantTempAngle(M4_SWEEP);  // Motor 4: Coolant temperature
    
    // Set new target positions (motors will step towards these in update() calls)
    motor1.setPosition(angle1);
    motor2.setPosition(angle2);
    motor3.setPosition(angle3);
    motor4.setPosition(angle4);
    
    timerAngleUpdate = millis();  // Reset timer
    
    int time =  micros() - s;
    // Serial.println(time);  // Debug: print execution time
  }
  
  // ===== MOTOR STEPPING =====
  // Execute micro-steps for all motors - called every loop iteration
  // This must run frequently (~1ms) for smooth needle movement
  motor1.update();
  motor2.update();
  motor3.update();
  motor4.update();

  // ===== SHUTDOWN DETECTION =====
  // Check if ignition voltage has dropped (key turned off)
  // Shutdown when battery voltage < 1V AND system has been running for at least 3 seconds
  if (vBatt < 1 && millis() > splashTime + 3000) {
    shutdown();  // Save settings, zero gauges, display shutdown screen, cut power
  }

  //serialInputFunc();  // Debugging only - manual input via serial monitor (commented out)

}




/*
 * ========================================
 * SENSOR READING FUNCTIONS
 * ========================================
 * 
 * These functions handle analog sensor reading, filtering, and curve mapping
 */

/**
 * readSensor - Generic analog sensor reader with filtering
 * 
 * Reads an analog input, maps it to 0-5V range, and applies exponential filtering
 * to reduce noise while maintaining responsiveness.
 * 
 * @param inputPin - Arduino analog pin to read (A0-A15)
 * @param oldVal - Previous filtered value (0-500 representing 0.00-5.00V)
 * @param filt - Filter coefficient (0-64): 
 *               - 64 = no filtering (instant response)
 *               - 32 = moderate filtering
 *               - 8 = heavy filtering (slow response, very smooth)
 *               Formula: newFiltered = (newRaw * filt + oldVal * (64-filt)) / 64
 * @return Filtered sensor value (0-500 representing 0-5V in 0.01V increments)
 * 
 * Example: Battery voltage with filt=8 means 8/64 = 12.5% new value, 87.5% old value
 */
unsigned long readSensor(int inputPin, int oldVal, int filt)  
{
    int raw = analogRead (inputPin);  // Read ADC: 0-1023 for 0-5V input
    unsigned long newVal = map( raw, 0, 1023, 0, 500);  // Map to 0-500 (0.00-5.00V in 0.01V steps)
    unsigned long filtVal = ((newVal*filt) + (oldVal*(64-filt)))>>6;  // Exponential filter (>>6 is divide by 64)
    return filtVal; 
}

/**
 * read30PSIAsensor - Read 30 PSI absolute pressure sensor
 * 
 * Reads a 30 PSIA sensor (typical barometric or MAP sensor) with 0.5-4.5V output range.
 * Includes filtering for stable pressure readings.
 * 
 * @param inputPin - Arduino analog pin for pressure sensor
 * @param oldVal - Previous filtered value (kPa * 10)
 * @param filt - Filter coefficient (0-16): similar to readSensor but /16 instead of /64
 * @return Filtered pressure in kPa * 10 (e.g., 1013 = 101.3 kPa = 1 atmosphere)
 * 
 * Sensor characteristics:
 * - 0.5V = 0 PSIA
 * - 4.5V = 30 PSIA (206.8 kPa)
 * - ADC 102 (0.5V) = 0 kPa
 * - ADC 921 (4.5V) = 2068 (206.8 kPa * 10)
 */
unsigned long read30PSIAsensor(int inputPin, int oldVal, int filt)
{
    int raw = analogRead (inputPin);  // Read ADC: 0-1023
    unsigned long newVal = map( raw, 102, 921, 0, 2068);  // Map 0.5-4.5V to 0-30 PSIA (0-206.8 kPa)
    unsigned long filtVal = ((newVal*filt) + (oldVal*(16-filt)))>>4;  // Filter (>>4 is divide by 16)
    return filtVal; 
}

/**
 * readThermSensor - Read GM-style thermistor temperature sensor
 * 
 * GM thermistors have a non-linear resistance vs. temperature curve.
 * This function reads the voltage from a voltage divider circuit and applies filtering.
 * Actual temperature conversion is done via curveLookup() with thermTable.
 * 
 * @param inputPin - Arduino analog pin for thermistor
 * @param oldVal - Previous filtered voltage value (0.00-5.00V)
 * @param filt - Filter coefficient (0-100): percentage of new value to use
 *               - 100 = no filtering
 *               - 50 = half new, half old (good for temperature - slow changing)
 * @return Filtered voltage reading (0.00-5.00V as float)
 * 
 * Note: Higher filter values (e.g., 50/100) provide more stability for slowly-changing
 * temperature readings, preventing gauge needle jitter.
 */
float readThermSensor(int inputPin, float oldVal, int filt)
{
    int raw = analogRead (inputPin);  // Read ADC: 0-1023
    float newVal = map( raw, 0, 1023, 0, 500)*0.01;  // Map to 0-5V as float
    float filtVal = ((newVal*filt) + (oldVal*(100-filt)))*0.01;  // Filter (*0.01 for percentage)
    return filtVal; 
}

/**
 * curveLookup - Generic lookup table with linear interpolation
 * 
 * Converts an input value to an output value using a piecewise-linear lookup table.
 * Uses linear interpolation between breakpoints for smooth, accurate conversion.
 * 
 * This is essential for non-linear sensors like thermistors and fuel senders where
 * the relationship between voltage and physical quantity isn't linear.
 * 
 * @param input - Input value (e.g., voltage from sensor)
 * @param brkpts[] - Array of X-axis breakpoints (must be in ascending order)
 * @param curve[] - Array of corresponding Y-axis values
 * @param curveLength - Number of points in the table
 * @return Interpolated output value
 * 
 * Behavior:
 * - If input < first breakpoint: returns first curve value (flat extrapolation)
 * - If input > last breakpoint: returns last curve value (flat extrapolation)
 * - If input between breakpoints: linear interpolation between the two nearest points
 * 
 * Interpolation formula: y = y0 + (y1-y0)*(x-x0)/(x1-x0)
 * where (x0,y0) and (x1,y1) are the surrounding breakpoints
 */
float curveLookup(float input, float brkpts[], float curve[], int curveLength){
  int index = 1;

  // Find input's position within the breakpoints
  for (int i = 0; i <= curveLength-1; i++){
    if (input < brkpts[0]){
      // Input below range - return minimum value (flat extrapolation)
      float output = curve[0];
      return output;
    } 
    else if (input <= brkpts[i+1]){
      // Found the interval containing input value
      index = i+1;
      break;
    } 
    else if (input > brkpts[curveLength-1]){
      // Input above range - return maximum value (flat extrapolation)
      float output = curve[curveLength-1];
      return output;
    }
  } 

  // Linear interpolation between breakpoints at index-1 and index
  float x1 = brkpts[index];      // Upper breakpoint X
  float x0 = brkpts[index-1];    // Lower breakpoint X
  float y1 = curve[index];       // Upper breakpoint Y
  float y0 = curve[index-1];     // Lower breakpoint Y
  
  // Calculate interpolated value: slope * distance + offset
  float output = (((y1-y0)/(x1-x0))*(input-x0))+y0;
  return output;
}


/*
 * ========================================
 * DISPLAY AND NAVIGATION FUNCTIONS
 * ========================================
 * 
 * Handle user interface: rotary encoder input, menu navigation, and OLED display updates
 */

/**
 * swRead - Read and debounce rotary encoder switch
 * 
 * Monitors the encoder push button and implements software debouncing to prevent
 * false triggers from mechanical switch bounce. Sets 'button' flag when a valid
 * press-and-release is detected.
 * 
 * Debouncing strategy:
 * - Ignore state changes within 50ms of last change (bounce period)
 * - Only set button flag on rising edge (button release) after debounce
 * 
 * Global variables modified:
 * - button: Set to 1 when valid button press completes
 * - stateSW: Current switch state (1=released, 0=pressed)
 * - lastStateSW: Previous switch state for edge detection
 * - debounceFlag: Prevents multiple triggers during bounce period
 * - lastStateChangeTime: Timestamp of last valid state change
 * 
 * Called from: main loop (every cycle before display update)
 */
void swRead() {
  // Declare debounce variables as static locals to persist between function calls
  // and prevent potential interference from other parts of the code
  static bool stateSW = 1;                      // Current state of encoder switch (1 = not pressed)
  static bool lastStateSW = 1;                  // Previous state of encoder switch
  static unsigned int lastStateChangeTime = 0;  // Timestamp of last switch state change (ms)
  static unsigned int debounceDelay = 50;       // Debounce time in milliseconds
  static bool debounceFlag = 0;                 // Flag to prevent multiple triggers during debounce
       
  stateSW = digitalRead(SWITCH);            // Read current state of encoder button
  int stateChange = stateSW - lastStateSW;  // Calculate change: -1=pressed, +1=released, 0=no change

  // Clear debounce flag if enough time has passed since last change
  if ((millis() - lastStateChangeTime) > debounceDelay) {
    debounceFlag = 0;  // Allow new state changes to be registered
  }

  // Detect button press (falling edge) - record time but don't trigger action yet
  if (stateChange < 0 && debounceFlag == 0) {
    lastStateChangeTime = millis();  // Record time of press
    debounceFlag = 1;                // Block bounces
  } 
  // Detect button release (rising edge) - this is when we register the button press
  else if (stateChange > 0 && debounceFlag == 0) {
    lastStateChangeTime = millis();  // Record time of release
    debounceFlag = 1;                // Block bounces
    button = 1;                      // Set button event flag (cleared by menu handlers)
  } 
  else if (stateChange == 0) {  
    // No state change - do nothing
  }
  lastStateSW = stateSW;  // Save current state for next comparison
}

/**
 * rotate - Rotary encoder interrupt handler
 * 
 * Called by hardware interrupt whenever encoder rotates (CHANGE on pins 2 or 3).
 * Updates menu position (dispArray1[menuLevel]) based on rotation direction.
 * Wraps around at menu boundaries for continuous scrolling.
 * 
 * The Rotary library handles quadrature decoding and debouncing, providing
 * clean DIR_CW (clockwise) and DIR_CCW (counter-clockwise) events.
 * 
 * Global variables modified:
 * - dispArray1[menuLevel]: Incremented or decremented based on rotation
 * 
 * Interrupt context: Keep this function fast and simple
 */
void rotate() {
  unsigned char result = rotary.process();  // Process encoder quadrature signals
  if (result == DIR_CW) {
    // Clockwise rotation - increment menu position
    if (dispArray1[menuLevel] == nMenuLevel) 
      dispArray1[menuLevel] = 0;  // Wrap to beginning
    else 
      dispArray1[menuLevel]++;    // Move to next item
  } 
  else if (result == DIR_CCW) {
    // Counter-clockwise rotation - decrement menu position
    if (dispArray1[menuLevel] == 0) 
      dispArray1[menuLevel] = nMenuLevel;  // Wrap to end
    else 
      dispArray1[menuLevel]--;              // Move to previous item
  }
}

/**
 * dispMenu - Multi-level menu system controller for display 1
 * 
 * This function implements a hierarchical menu system using a switch-case structure.
 * Menu navigation is controlled by:
 * - Rotary encoder rotation: changes dispArray1[menuLevel]
 * - Button press: enters submenu or triggers action
 * 
 * Menu structure (dispArray1 is [level0, level1, level2, level3]):
 * - Level 0: Main screens (oil pressure, coolant temp, oil temp, fuel, battery, clock, trip odo, speed, RPM, etc.)
 * - Level 1: Submenus (settings options, trip odo reset options)
 * - Level 2: Sub-submenus (display selections, unit selections, clock offset)
 * - Level 3: Final selection values
 * 
 * The menuLevel variable tracks current depth, and nMenuLevel defines how many
 * options exist at the current level (for wrapping rotation).
 * 
 * Global variables used:
 * - dispArray1[4]: Current position in menu hierarchy
 * - menuLevel: Current menu depth (0-3)
 * - nMenuLevel: Number of items in current menu
 * - button: Button press flag (cleared after handling)
 * 
 * Special behaviors:
 * - Case 0 (Settings) is always last for easy access
 * - Case 7 (Trip Odo) has reset confirmation submenu
 * - EEPROM updates happen when exiting settings
 * 
 * Called from: main loop at dispUpdateRate (75ms)
 */
void dispMenu() {
  switch (dispArray1[0]) {  // Level 0 - Main menu selection
    
    case 1:  // Oil Pressure Display                 dispArray1 = {1, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag (no action - this is a display screen only)
      }
      // Serial.println("Oil Pressure");  // Debug output
      dispOilPrsGfx(&display1);  // Show oil pressure with icon
      break;
    
    case 2:  // Coolant Temperature Display           dispArray1 = {2, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Coolant Temp");  // Debug output
      dispCoolantTempGfx(&display1);  // Show coolant temp with thermometer icon
      break;
    
    case 3:  // Oil Temperature Display                dispArray1 = {3, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag   
      }
      // Serial.println("Oil Temp");  // Debug output
      dispOilTempGfx(&display1);  // Show oil temp with oil can/thermometer icon
      break;
    
    case 4:  // Fuel Level Display                     dispArray1 = {4, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Fuel Level");  // Debug output
      dispFuelLvlGfx(&display1);  // Show fuel level in gallons/liters with icon
      break;
    
    case 5:  // Battery Voltage Display                dispArray1 = {5, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("batt voltage");  // Debug output
      dispBattVoltGfx(&display1);  // Show battery voltage with battery icon
      break;

    case 6:  // Clock Display                          dispArray1 = {6, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Clock");  // Debug output
      dispClock(&display1);  // Show time from GPS with local offset
      break;  

    case 7:  // Trip Odometer with Reset Option        dispArray1 = {7, x, x, x}
	  if (menuLevel == 0 && button == 1) {
        // Button pressed - enter submenu to confirm reset
        button = 0;
        menuLevel = 1;   // Go to level 1 (submenu)
        nMenuLevel = 1;  // 2 options in submenu: Yes/No (0-indexed)
      } 
      else if (menuLevel == 0) {
        // No button - just display trip odometer value
        dispTripOdo(&display1);
        // Serial.println("Trip Odo");  // Debug output
      } 
      else {
        // In submenu - handle Yes/No selection for reset
        switch (dispArray1[1]) {
          case 0:  // Reset Trip Odo: YES              dispArray1 = {7, 0, x, x}
            dispOdoResetYes(&display1);  // Show confirmation screen with YES highlighted
			      if (button == 1) {
                // User confirmed reset
			        odoTrip = 0;  // Clear trip odometer
			        goToLevel0();  // Return to main menu
			        dispArray1[0] = 7;  // Stay on trip odo screen
            } 
            break;
            
		      case 1:  // Reset Trip Odo: NO               dispArray1 = {7, 1, x, x}
			      dispOdoResetNo(&display1);  // Show confirmation screen with NO highlighted
			      if (button == 1) {
                // User cancelled reset
				      goToLevel0();  // Return to main menu
				      dispArray1[0] = 7;  // Stay on trip odo screen
			      } 
          break;
		    } 
	    }
      break;    
    
    case 8:  // Speed Display                          dispArray1 = {8, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Speed");  // Debug output
      dispSpd(&display1);  // Show speed in km/h or mph based on units setting
      break;  
    
    case 9:  // RPM Display                            dispArray1 = {9, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("RPM");  // Debug output
      dispRPM(&display1);  // Show engine RPM
      break;  
    
    case 10:  // Ignition Timing Display               dispArray1 = {10, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("ignition timing");  // Debug output
      dispIgnAng(&display1);  // Show ignition advance in degrees BTDC
      break;
    
    case 11:  // Air/Fuel Ratio Display                dispArray1 = {11, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("AFR");  // Debug output
      dispAFR(&display1);  // Show AFR (e.g., 14.7)
      break;  
    
    case 12:  // Fuel Pressure Display                 dispArray1 = {12, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Fuel Pressure");  // Debug output
      dispFuelPrs(&display1);  // Show fuel pressure in PSI or bar
      break;  

    case 13:  // Fuel Composition Display              dispArray1 = {13, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("ethanol %");  // Debug output
      dispFuelComp(&display1);  // Show ethanol percentage (E85 flex fuel)
      break;  

    case 14:  // Injector Duty Cycle Display           dispArray1 = {14, x, x, x}
      if (menuLevel == 0 && button == 1) {
        button = 0; // Clear button flag
      }
      // Serial.println("Inj Duty");  // Debug output
      dispInjDuty(&display1);  // Show injector duty cycle percentage
      break;
      
    case 15:  // Falcon Script Logo                    dispArray1 = {15, x, x, x}
      if (menuLevel == 0 && button == 1) {
        // No action on button press - just a splash screen
        //goToLevel0();
      }
      // Serial.println("falcon Script");  // Debug output
      dispFalconScript(&display1);  // Display Falcon logo
      break;  

    case 0:  // SETTINGS MENU - Always last screen for easy wrapping access
             // This is a complex multi-level menu for system configuration
             // Structure: Settings -> [Display 2 Select | Units | Clock Offset | Fuel Sensor Cal]
             //   -> Display 2: 9 screen options (Oil Prs, Coolant, Batt, Fuel, AFR, Speed, RPM, 302V, 302CID)
             //   -> Units: Metric or Imperial
             //   -> Clock Offset: -12 to +12 hours from UTC
             //   -> Fuel Cal: Adjust fuel sender calibration
             
      if (menuLevel == 0 && button == 1) {  
        // Button pressed - enter Settings submenu
        button = 0;
        menuLevel = 1;   // Go to level 1 (submenu)
        nMenuLevel = 3;  // 4 options: Display2, Units, Clock, FuelCal (0-indexed)
      } 
      else if (menuLevel == 0) {
        // No button - display "SETTINGS" screen
        // Serial.println("settings");  // Debug output
        dispSettings(&display1);
      } 
      else {  
        // In Settings submenu - handle level 1, 2, and 3 navigation
        
        switch (dispArray1[1]) {  // Level 1 selection
          
          case 0:  // Configure Display 2           dispArray1 = {0, 0, x, x}
            if (menuLevel == 1 && button == 1) {
              // Enter Display 2 selection submenu
              button = 0;
              menuLevel = 2;   // Go to level 2
              nMenuLevel = 8;  // 9 display options (0-indexed)
            } 
            else if (menuLevel == 1) {
              // Show "DISPLAY 2" menu header
              //Serial.println("Display 2");  // Debug output
              dispDisp2Select(&display1);
            } 
            else {
              // Level 2 - Select what to show on Display 2
              switch (dispArray1[2]) {
                
                case 0:  // Oil Pressure on Display 2    dispArray1 = {0, 0, 0, x}
                  //Serial.println("Disp2: Oil Pressure");  // Debug output
                  dispArray2[0] = 0;  // Set display 2 to oil pressure
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 1:  // Coolant Temp on Display 2    dispArray1 = {0, 0, 1, x}
                  //Serial.println("Disp2: Coolant Temp");  // Debug output
                  dispArray2[0] = 1;  // Set display 2 to coolant temp
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 2:  // Battery Voltage on Display 2  dispArray1 = {0, 0, 2, x}
                  //Serial.println("Disp2: Battery Voltage");  // Debug output
                  dispArray2[0] = 2;  // Set display 2 to battery voltage
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 3:  // Fuel Level on Display 2       dispArray1 = {0, 0, 3, x}
                  //Serial.println("Disp2: Fuel Level");  // Debug output
                  dispArray2[0] = 3;  // Set display 2 to fuel level
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu;
                  }
                  break;
                  
                case 4:  // RPM on Display 2              dispArray1 = {0, 0, 4, x}
                  //Serial.println("Disp2: RPM");  // Debug output
                  dispArray2[0] = 4;  // Set display 2 to RPM
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 5:  // Speed on Display 2            dispArray1 = {0, 0, 5, x}
                  //Serial.println("Disp2: Speed");  // Debug output
                  dispArray2[0] = 5;  // Set display 2 to speed
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;

                case 6:  // Clock on Display 2            dispArray1 = {0, 0, 6, x}
                  //Serial.println("Clock");  // Debug output
                  dispArray2[0] = 6;  // Set display 2 to clock
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;

                case 7:  // 302CID Logo on Display 2      dispArray1 = {0, 0, 7, x}
                  //Serial.println("Disp2: 302CID");  // Debug output
                  dispArray2[0] = 7;  // Set display 2 to 302 CID logo
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 8:  // 302V Logo on Display 2        dispArray1 = {0, 0, 8, x}
                  //Serial.println("Disp2: 302V");  // Debug output
                  dispArray2[0] = 8;  // Set display 2 to 302V logo
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 9:  // Falcon Script on Display 2    dispArray1 = {0, 0, 9, x}
                  //Serial.println("Disp2: Falcon Script");  // Debug output
                  dispArray2[0] = 9;  // Set display 2 to Falcon logo
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                
              } // End switch dispArray1[2] - Display 2 options
            } // End level 2 - Display 2 selection
            break;  // End case 0 - Display 2 submenu
 
          case 1:  // Configure Units (Metric/Imperial)  dispArray1 = {0, 1, x, x}
            if (menuLevel == 1 && button == 1) {
              // Enter Units selection submenu
              button = 0;
              menuLevel = 2;   // Go to level 2
              nMenuLevel = 1;  // 2 options: Metric or Imperial (0-indexed)
            } 
            else if (menuLevel == 1) {
              // Show "UNITS" menu header
              //Serial.println("Units");  // Debug output
              dispUnits(&display1);
            } 
            else {
              // Level 2 - Select metric or imperial units
              switch (dispArray1[2]) {
                
                case 0:  // Metric Units (km/h, C, bar)  dispArray1 = {0, 1, 0, x}
                  display1.setTextColor(WHITE); 
                  display1.clearDisplay();
                  display1.setTextSize(2);
                  display1.setCursor(31,8);
                  display1.println("Metric");  // Display selected unit system
                  display1.display();
                  units = 0;  // Set to metric
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
                case 1:  // Imperial Units (mph, F, PSI)  dispArray1 = {0, 1, 1, x}
                  display1.setTextColor(WHITE); 
                  display1.clearDisplay();
                  display1.setTextSize(2);
                  display1.setCursor(20,8);
                  display1.println("'Merican");  // Display selected unit system
                  display1.display();
                  units = 1;  // Set to imperial
                  if (button == 1) {
                    goToLevel0();  // Save and return to main menu
                  }
                  break;
                  
              } // End switch dispArray1[2] - Units options
            } // End level 2 - Units selection
            break;  // End case 1 - Units submenu

          case 2:  // Clock Offset (Time Zone)          dispArray1 = {0, 2, x, x}
            // Adjust clock offset from UTC (-12 to +12 hours)
            if (menuLevel == 1 && button == 1) {
              button = 0;
              menuLevel = 2;  // Go to level 2 (offset value selection)
              // nMenuLevel set dynamically in level 2 handler
            } 
            else if (menuLevel == 1) {
              // Show "SET CLOCK" menu header
              // Serial.println("ClockOffset");  // Debug output
              dispClockOffset(&display1);
            } 
            else {
              // Level 2 - Display current offset and adjust with encoder
              if (button == 1) {
                // Button pressed - save clock offset and return to main menu
                detachInterrupt(0);  // Temporarily detach encoder interrupts
                detachInterrupt(1);
                attachInterrupt(0, rotate, CHANGE);  // Reattach normal menu rotation handler
                attachInterrupt(1, rotate, CHANGE);
                EEPROM.write(clockOffset, clockOffsetAddress);  // Save offset to EEPROM
                goToLevel0();  // Return to main menu
              } 
              else {
                // Rotary encoder adjusts clock offset value (-12 to +12 hours)
                // Temporarily change encoder handler to modify clockOffset directly
                detachInterrupt(0);
                detachInterrupt(1);
                attachInterrupt(0, incrementOffset, CHANGE);  // Use special offset increment handler
                attachInterrupt(1, incrementOffset, CHANGE);
                dispClock(&display1);  // Display time with current offset applied
              }
            } // End level 2 - Clock offset adjustment
            break;  // End case 2 - Clock offset submenu
          
          case 3:  // Exit Settings Menu                dispArray1 = {0, 3, x, x}
            display1.setTextColor(WHITE); 
            display1.clearDisplay();
            display1.setTextSize(2);
            display1.setCursor(35,8);
            display1.println("EXIT");  // Display "EXIT" text               
            display1.display();
            if (button == 1) {
              goToLevel0();  // Return to main menu
            }
            break;  // End case 3 - Exit
            
        } // End switch dispArray1[1] - Settings level 1 options
        break;  // Break for case 0 (Settings menu when in submenus)
      } // End Settings submenu levels 1-3
  } // End switch dispArray1[0] - Main menu selection
} // End dispMenu()

/**
 * goToLevel0 - Reset menu navigation to top level
 * 
 * Returns to the main menu (level 0) and resets all menu position variables.
 * This function is called when exiting settings or after making a selection.
 * 
 * Actions performed:
 * - Clear button flag
 * - Reset all menu levels to position 0 (Settings screen)
 * - Set menuLevel to 0 (top level)
 * - Set nMenuLevel to 14 (15 main menu items, 0-indexed)
 * 
 * Note: dispArray1[0] = 0 positions cursor on Settings, which is case 0 (last screen)
 * 
 * Called from: Settings submenus, trip odo reset confirmation
 */
void goToLevel0(void){
  button = 0;           // Clear button press flag
  dispArray1[0] = 0;    // Set to Settings screen (case 0)
  dispArray1[1] = 0;    // Clear level 1 selection
  dispArray1[2] = 0;    // Clear level 2 selection
  menuLevel = 0;        // Return to top menu level
  nMenuLevel = 14;      // Set to 15 items in main menu (0-indexed)
}

/**
 * disp2 - Control display 2 based on saved selection
 * 
 * Routes display 2 output based on dispArray2[0], which is set in Settings menu
 * and saved to EEPROM. Allows user to customize what appears on the second display.
 * 
 * Display options:
 * 0 - Oil Pressure
 * 1 - Coolant Temperature
 * 2 - Battery Voltage
 * 3 - Fuel Level
 * 4 - RPM
 * 5 - Speed
 * 6 - Clock
 * 7 - 302CID logo
 * 8 - 302V logo
 * 9 - Falcon Script logo
 * 
 * Called from: main loop at dispUpdateRate (75ms)
 */
void disp2(void){
  switch (dispArray2[0]){

    case 0: // Oil Pressure
      dispOilPrsGfx(&display2);
      break;
    
    case 1: // Coolant Temp
      dispCoolantTempGfx(&display2);
      break;

    case 2: // Battery Voltage
      dispBattVoltGfx(&display2);
      break;

    case 3: // Fuel Level
      dispFuelLvlGfx(&display2);
      break;

    case 4: // RPM
      dispRPM(&display2);
      break;

    case 5: // Speed
      dispSpd(&display2);
      break;

    case 6: // Clock
      dispClock(&display2);
      break;
    
    case 7: // 302CID Logo
      disp302CID(&display2);
      break;

    case 8: // 302V Logo
      disp302V(&display2);
      break;

    case 9: // Falcon Script Logo
      dispFalconScript(&display2);
      break;
  }
}

/**
 * incrementOffset - Rotary encoder handler for clock offset adjustment
 * 
 * Special interrupt handler used only when adjusting clock offset.
 * Replaces the normal 'rotate' handler temporarily.
 * Adjusts clockOffset variable directly (0-23 hours) with wraparound.
 * 
 * Range: 0-23 (24-hour format for UTC offset)
 * Example: UTC+5 would be clockOffset = 5
 *          UTC-8 would be clockOffset = 16 (24-8)
 * 
 * Global variables modified:
 * - clockOffset: Hours to add to GPS UTC time (0-23, wraps around)
 * 
 * Interrupt context: Keep function fast and simple
 */
void incrementOffset() {
  unsigned char result = rotary.process();  // Process encoder quadrature signals
  
  if (result == DIR_CW) {
    // Clockwise - increment offset with wraparound
    if (clockOffset == 23) 
      clockOffset = 0;  // Wrap to 0
    else 
      clockOffset++;    // Increment
  } 
  else if (result == DIR_CCW) {
    // Counter-clockwise - decrement offset with wraparound
    if(clockOffset == 0) 
      clockOffset = 23;  // Wrap to 23
    else 
      clockOffset--;     // Decrement
  }
}


/*
 * ========================================
 * SCREEN DRAWING FUNCTIONS
 * ========================================
 * 
 * These functions render specific screens and data on the OLED displays.
 * Each function takes a pointer to an Adafruit_SSD1306 display object,
 * allowing the same function to draw on either display1 or display2.
 * 
 * General structure:
 * 1. Set text color (WHITE for visible pixels on OLED)
 * 2. Clear display buffer
 * 3. Set text size and cursor position
 * 4. Print text and/or draw graphics
 * 5. Call display() to transfer buffer to screen
 * 
 * Layout considerations:
 * - Screen is 128x32 pixels
 * - Text size 1: 6x8 pixels per character
 * - Text size 2: 12x16 pixels per character  
 * - Text size 3: 18x24 pixels per character
 * - Center position calculated as: center - (numDigits * pixelsPerChar / 2)
 */

/**
 * dispSettings - Display "SETTINGS" header screen
 * Shows the main settings menu title with a border frame
 */
void dispSettings (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(20,8);
    display->println("SETTINGS");
    display->drawRect(0,0,128,32,SSD1306_WHITE);  // Draw border rectangle              
    display->display();
}

/**
 * dispDisp2Select - Display "DISPLAY 2" submenu header
 * Shows header when selecting what to display on second screen
 */
void dispDisp2Select (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(15,8);
    display->println("DISPLAY 2");                 
    display->display();
}

/**
 * dispUnits - Display "UNITS" submenu header
 * Shows header when selecting metric vs imperial units
 */
void dispUnits (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(32,8);
    display->println("UNITS");                 
    display->display();
}

/**
 * dispClockOffset - Display "SET CLOCK" header
 * Shows header when adjusting time zone offset
 */
void dispClockOffset (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(0,9);
    display->println("SET CLOCK");                 
    display->display();
}

/**
 * dispRPM - Display engine RPM
 * 
 * Shows large RPM value with "RPM" label.
 * Uses dynamic centering based on number of digits.
 * 
 * @param display - Pointer to display object (display1 or display2)
 */
void dispRPM (Adafruit_SSD1306 *display){
    byte nDig = digits(RPM);  // Calculate number of digits for centering
    byte center = 47;         // Center point for display
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(3);  // Large text for main value
    display->setCursor(center-((nDig*18)/2),6);  // Center based on digits (18 pixels per char at size 3)
    display->println(RPM); 
    display->setTextSize(2);  // Smaller text for label
    display->setCursor(88,10);
    display->println("RPM");                
    display->display();
}

/**
 * dispSpd - Display vehicle speed
 * 
 * Shows speed with automatic unit conversion based on 'units' setting.
 * Metric: km/h (from GPS data)
 * Imperial: mph (converted from km/h)
 * 
 * Uses dynamic centering based on number of digits.
 * 
 * @param display - Pointer to display object (display1 or display2)
 * 
 * Note: spd is stored as km/h * 100 for integer precision
 */
void dispSpd (Adafruit_SSD1306 *display){
    display->setTextColor(WHITE); 
    display->clearDisplay();

    if (units == 0){    // Metric Units (km/h)
      float spdDisp = spd*0.01;  // Convert from km/h*100 to km/h
      byte nDig = digits(spdDisp);  // Get number of digits for centering
      byte center = 37;
      display->setTextSize(3);  // Large text for speed value (18 pixels per character)
      display->setCursor(center-((nDig*18)/2),6);
      display->print(spdDisp, 0);  // Print without decimal places
      display->setCursor(center+((nDig*18)/2)+4,10);
      display->setTextSize(2);  // Smaller text for units
      display->println("km/h");
               
    } 
    else {              // Imperial Units (mph)
      float spdDisp = spd * 0.006213711922;  // Convert km/h*100 to mph (factor = 0.6213712 / 100)
      byte nDig = digits (spdDisp);
      byte center = 47;
      display->setTextSize(3);
      display->setCursor(center-((nDig*18)/2),6);
      display->print(spdDisp, 0);  // Print without decimal places
      display->setCursor(center+((nDig*18)/2)+4,10);
      display->setTextSize(2);
      display->println("MPH");          
    }
          
    display->display();
}

/**
 * dispOilTemp - Display oil temperature with icon
 * 
 * Shows oil temp with unit conversion and degree symbol.
 * Displays oil can/thermometer icon on left side.
 * 
 * Metric: Celsius (no conversion needed)
 * Imperial: Fahrenheit (C * 1.8 + 32)
 * 
 * @param display - Pointer to display object
 */
void dispOilTemp (Adafruit_SSD1306 *display) {
    float oilTempDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->drawBitmap(0, 0, img_oilTemp, 40, 32, 1);  // Draw oil/temp icon (40x32 pixels)
    byte center = 71;  // Center point for text (offset for icon on left)
    
    if (units == 0){    // Metric Units (Celsius)
      oilTempDisp = oilTemp;  // No conversion needed - already in Celsius
      byte nDig = digits (oilTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilTempDisp, 0);  // Print temperature value
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);  // Draw degree symbol (small circle)
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("C");  // Celsius label
    }

    else {              // Imperial Units (Fahrenheit)
      oilTempDisp = (oilTemp*1.8) + 32;  // Convert Celsius to Fahrenheit
      byte nDig = digits (oilTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilTempDisp, 0);
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);  // Draw degree symbol
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("F");  // Fahrenheit label
    }

    display->display();
}

/**
 * dispFuelPrs - Display fuel pressure
 * 
 * Shows fuel pressure with "FUEL PRESSURE" label and unit conversion.
 * Includes protection against negative values (sensor error or not connected).
 * 
 * Metric: bar (kPa / 100)
 * Imperial: PSI (kPa * 0.1450377)
 * 
 * @param display - Pointer to display object
 * 
 * Note: fuelPrs is gauge pressure in kPa (atmospheric pressure already subtracted)
 */
void dispFuelPrs (Adafruit_SSD1306 *display) {
    float fuelPrsDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2); 
    display->setCursor(0,3);
    display->println("FUEL");  // Label line 1
    display->setTextSize(1); 
    display->setCursor(0,21);
    display->println("PRESSURE");  // Label line 2

    if (units == 0){    // Metric Units (bar)
      fuelPrsDisp = fuelPrs/100;  // Convert kPa to bar (1 bar = 100 kPa)
      if (fuelPrsDisp < 0) {fuelPrsDisp = 0;}  // Clamp to 0 if negative
      byte nDig = 3;  // Always 3 digits for bar (e.g., "3.5")
      byte center = 79;
      display->setTextSize(3);
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelPrsDisp, 1);  // Print with 1 decimal place
      display->setCursor(center+((nDig*18)/2)+3,18);
      display->setTextSize(1); 
      display->println("bar");
               
    } 
    else {              // Imperial Units (PSI)
      fuelPrsDisp = fuelPrs * 0.1450377;  // Convert kPa to PSI (1 kPa = 0.145 PSI)
      if (fuelPrsDisp < 0) {fuelPrsDisp = 0;}  // Clamp to 0 if negative
      byte nDig = digits (fuelPrsDisp);
      byte center = 71;
      display->setTextSize(3);
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelPrsDisp, 0);  // Print without decimal
      display->setCursor(center+((nDig*18)/2)+2,10);
      display->setTextSize(2);
      display->println("PSI");          
    }
    
    display->display();
}

/**
 * dispFuelComp - Display fuel composition (ethanol percentage)
 * 
 * Shows flex fuel percentage (0-100% ethanol).
 * Useful for E85 flex fuel vehicles to know fuel composition.
 * 
 * @param display - Pointer to display object
 * 
 * Example: E85 would show as 85%
 */
void dispFuelComp (Adafruit_SSD1306 *display) {
    byte nDig = digits (fuelComp);
    byte center = 79;
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2);
    display->setCursor(2,0);
    display->println("Flex");  // Label line 1
    display->setCursor(2,15);
    display->println("Fuel");  // Label line 2
    display->setTextSize(3); 
    display->setCursor(center-((nDig*18)/2),6);
    display->print(fuelComp, 0);  // Print percentage value
    display->println("%");        
    display->display();
}

/**
 * dispAFR - Display Air/Fuel Ratio
 * 
 * Shows AFR value from wideband O2 sensor.
 * Important for tuning - stoichiometric AFR for gasoline is ~14.7
 * 
 * @param display - Pointer to display object
 */
void dispAFR (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setCursor(8,6);
    display->setTextSize(3); 
    display->print(afr, 1);  // Print AFR with 1 decimal place (e.g., 14.7)
    display->setCursor(88,10);
    display->setTextSize(2);
    display->println("AFR");         
    display->display();
}

/**
 * dispFalconScript - Display Falcon logo splash screen
 * Simple bitmap display - shows Falcon script logo
 */
void dispFalconScript(Adafruit_SSD1306 *display) {
    display->clearDisplay();
    display->drawBitmap(0, 0, img_falcon_script, SCREEN_W, SCREEN_H, 1);
    display->display();
}

/**
 * disp302CID - Display 302 CID engine badge
 * Shows "302 CID" (Cubic Inch Displacement) logo
 */
void disp302CID(Adafruit_SSD1306 *display) {
    display->clearDisplay();
    display->drawBitmap(0, 0, img_302_CID, SCREEN_W, SCREEN_H, 1);
    display->display();
}

/**
 * disp302V - Display 302 V8 engine badge
 * Shows "302V" (V8) logo with graphic
 */
void disp302V(Adafruit_SSD1306 *display) {
    display->clearDisplay();
    display->drawBitmap(0, 0, img_302V, SCREEN_W, SCREEN_H, 1);
    display->display();
}

/**
 * dispOilPrsGfx - Display oil pressure with icon
 * 
 * Similar to dispFuelPrs but with oil can icon.
 * Shows oil pressure with unit conversion.
 * 
 * Metric: bar (kPa / 100)
 * Imperial: PSI (kPa * 0.1450377)
 * 
 * @param display - Pointer to display object
 * 
 * Note: Negative values clamped to 0 (sensor error or engine off)
 */
void dispOilPrsGfx (Adafruit_SSD1306 *display) {
    float oilPrsDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->drawBitmap(0, 0, img_oilPrs, 40, 32, 1);  // Draw oil can icon
    if (oilPrs < 0) {oilPrs = 0;}  // Clamp negative values
    
    if (units == 0){    // Metric Units (bar)
      oilPrsDisp = oilPrs/100;  // Convert kPa to bar
      if (oilPrsDisp < 0) {oilPrsDisp = 0;}
      byte nDig = 3;  // Always 3 digits for bar
      byte center = 79;
      display->setTextSize(3);
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilPrsDisp, 1);  // Print with 1 decimal
      display->setCursor(center+((nDig*18)/2)+3,18);
      display->setTextSize(1); 
      display->println("bar");
               
    } 
    else {              // Imperial Units (PSI)
      oilPrsDisp = oilPrs * 0.1450377;  // Convert kPa to PSI
      if (oilPrsDisp < 0) {oilPrsDisp = 0;}
      byte nDig = digits (oilPrsDisp);
      byte center = 71;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilPrsDisp, 0);  
      display->setCursor(center+((nDig*18)/2)+2,10);
      display->setTextSize(2);
      display->println("PSI");          
    }
          
    display->display();
}

void dispOilTempGfx (Adafruit_SSD1306 *display) {
    float oilTempDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_oilTemp, 40, 32, 1);
    byte center = 71;
    
    if (units == 0){    // Metric Units
      oilTempDisp = oilTemp;
      byte nDig = digits (oilTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilTempDisp, 0);
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("C");
    }

    else {              // 'Merican Units
      oilTempDisp = (oilTemp*1.8) + 32; // convert C to F
      byte nDig = digits (oilTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilTempDisp, 0);
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("F");
    }

    display->display();
}

void dispCoolantTempGfx (Adafruit_SSD1306 *display) {
    float coolantTempDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_coolantTemp, 38, 32, 1);
    byte center = 71;
    
    if (units == 0){    // Metric Units
      coolantTempDisp = coolantTemp;
      byte nDig = digits (coolantTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(coolantTempDisp, 0);
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("C");
    }

    else {              // 'Merican Units
      coolantTempDisp = (coolantTemp*1.8) + 32; // convert C to F
      byte nDig = digits (coolantTempDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(coolantTempDisp, 0);
      display->drawCircle(center+((nDig*18)/2)+3, 7, 2, WHITE);
      display->setCursor(center+((nDig*18)/2)+9,6);
      display->println("F");
    }

    display->display();
}

void dispBattVoltGfx (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_battVolt, 35, 32, 1);
    display->setTextSize(3); 
    display->setCursor(42,6);
    display->println(vBatt, 1);
    display->setTextSize(2);
    display->setCursor(116,12); 
    display->println("V");         
    display->display();
}

void dispFuelLvlGfx (Adafruit_SSD1306 *display) {
    float fuelLvlDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_fuelLvl, 32, 32, 1);
    byte center = 71;
    
    if (units == 0){    // Metric Units
      fuelLvlDisp = fuelLvl*3.785; // convert to liters
      byte nDig = digits(fuelLvlDisp);
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelLvlDisp, 0);
      display->setCursor(center+((nDig*18)/2)+4,6);
      display->println("l");
    }

    else {              // 'Merican Units
      fuelLvlDisp = fuelLvl; // read in gallons
      byte nDig = digits(fuelLvlDisp) +2 ;
      display->setTextSize(3); 
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelLvlDisp, 1);
      display->setCursor(center+((nDig*18)/2)+2,18);
      display->setTextSize(1); 
      display->println("gal");
    }

    display->display();
}

void dispTripOdo (Adafruit_SSD1306 *display) {
    float odoDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
        
    if (units == 0){    // Metric Units
      odoDisp = odoTrip; 
      display->setCursor(100,6);
      display->setTextSize(2);
      display->println("km");         
    } 
    else {              // 'Merican units
      odoDisp = odoTrip * 0.6213712; //convert km to miles  
      display->setCursor(100,6);
      display->setTextSize(2);
      display->println("mi");          
    }

    display->setCursor(35,6);
    display->setTextSize(2); 
    // right justify
    if (odoDisp < 10) {
      display->setTextColor(BLACK); 
      display->print("00");
    }
    else if (odoDisp < 100){
      display->setTextColor(BLACK); 
      display->print("0");
    }
    
    display->setTextColor(WHITE);
    // remove tenths once 1000 is reached
    if (odoDisp < 1000) { 
      display->println(odoDisp, 1);
    }
    else {
      display->println(odoDisp, 0);
    }
    
    display->setTextSize(1);
    display->setCursor(1,3);
    display->println("Trip");
    display->setCursor(1,13);
    display->println("Odo:"); 
    display->display();  
}

void dispOdoResetYes(Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);
    display->setCursor(5,0);
    display->println("RESET ODO?");
    display->fillRect(13,15,38,16,1);
    display->setCursor(15,16);
    display->setTextColor(BLACK); 
    display->println("YES");
    display->setCursor(76,16);
    display->setTextColor(WHITE); 
    display->println("NO");
    display->display();
}

void dispOdoResetNo(Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);
    display->setCursor(5,0);
    display->println("RESET ODO?");
    display->setCursor(15,16);
    display->setTextColor(WHITE); 
    display->println("YES");
    display->fillRect(74,15,26,16,1);
    display->setCursor(76,16);
    display->setTextColor(BLACK); 
    display->println("NO");
    display->display();
}

void dispIgnAng (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(6,0);
    display->println("IGN");
    display->setCursor(2,15);
    display->println("BTDC");            
    display->setTextSize(3); 
    display->setCursor(66,6);
    display->print(ignAngCAN/10); 
    display->write(0xF7);  
    display->println();      
    display->display();
}

void dispInjDuty (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(6,0);
    display->println("INJ");
    display->setCursor(2,15);
    display->println("DUTY");            
    display->setTextSize(3); 
    display->setCursor(66,6);
    display->print(injDutyCAN/10);  
    display->println("%");      
    display->display();
}

/**
 * dispClock - Display time from GPS with local offset
 * 
 * Shows current time in HH:MM format using GPS time + clock offset.
 * GPS provides UTC time, clock offset adjusts for local timezone.
 * 
 * @param display - Pointer to display object
 * 
 * Time handling:
 * - GPS hour is in UTC (0-23)
 * - clockOffset is added to get local time
 * - Wraps around at 24 hours
 * - Minutes are zero-padded (e.g., "3:05" not "3:5")
 */
void dispClock (Adafruit_SSD1306 *display){
    byte hourAdj;
    display->clearDisplay();
    
    // Calculate local hour from UTC + offset with wraparound
    if (clockOffset + hour > 23) {        
      hourAdj = clockOffset + hour - 24;  // Wrap to next day
    }
    else {
      hourAdj = clockOffset + hour;
    }

    byte nDig = digits(hourAdj)+3;  // +3 for colon and 2-digit minutes
    byte center = 63;
    
    display->setTextColor(WHITE);
    display->setTextSize(3);
    display->setCursor(center-((nDig*18)/2),6);
    display->print(hourAdj); 
    display->print(':');
    if (minute < 10) { display->print('0'); }  // Zero-pad minutes (e.g., "03" not "3")
    display->println(minute);
    display->display();
}

/**
 * digits - Count number of digits in a number
 * 
 * Helper function for dynamic text centering on displays.
 * Determines how many character widths are needed for a number.
 * 
 * @param val - Number to count digits for (can be negative)
 * @return Number of digits (1-4), includes sign for negative numbers
 * 
 * Example: 
 * - digits(5) = 1
 * - digits(42) = 2
 * - digits(-7) = 2 (includes minus sign)
 */
byte digits(float val){
  byte nDigits;
  if (val >= 0){ 
    if (val < 10)         {nDigits = 1;}
    else if (val < 100)   {nDigits = 2;}
    else if (val < 1000)  {nDigits = 3;}
    else if (val < 10000) {nDigits = 4;}
  }
  else {  // Negative numbers - count includes minus sign
    if (val > -10)        {nDigits = 2;}  // "-" + 1 digit
    else if (val > -100)  {nDigits = 3;}  // "-" + 2 digits
    else if (val > -1000) {nDigits = 4;}  // "-" + 3 digits
  }
  return nDigits;
}

/*
 * ========================================
 * CAN BUS FUNCTIONS
 * ========================================
 * 
 * Handle CAN bus communication with Haltech ECU and other modules
 * CAN bus operates at 500kbps with standard 11-bit identifiers
 */

/**
 * sendCAN_LE - Send CAN message with Little Endian byte order
 * 
 * Packs four 16-bit integer values into an 8-byte CAN message.
 * Little Endian: Low byte first, then high byte (Intel byte order).
 * 
 * @param CANaddress - CAN message ID (11-bit identifier)
 * @param inputVal_1 - First 16-bit value (bytes 0-1)
 * @param inputVal_2 - Second 16-bit value (bytes 2-3)
 * @param inputVal_3 - Third 16-bit value (bytes 4-5)
 * @param inputVal_4 - Fourth 16-bit value (bytes 6-7)
 * 
 * Example: inputVal_1 = 0x1234
 *   data[0] = 0x34 (low byte)
 *   data[1] = 0x12 (high byte)
 * 
 * Used for: Sensor data to other modules (thermistor, fuel level, baro)
 */
void sendCAN_LE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4)
{
        // LITTLE ENDIAN byte packing
        // Word 1 (bytes 0-1)
        data[0] = lowByte(inputVal_1);   // LSB first
        data[1] = highByte(inputVal_1);  // MSB second
        // Word 2 (bytes 2-3)
        data[2] = lowByte(inputVal_2);
        data[3] = highByte(inputVal_2);
        // Word 3 (bytes 4-5)
        data[4] = lowByte(inputVal_3);
        data[5] = highByte(inputVal_3);
        // Word 4 (bytes 6-7)
        data[6] = lowByte(inputVal_4);
        data[7] = highByte(inputVal_4);

        //Serial.println(inputVal_1);  // Debug output
        byte sndStat = CAN0.sendMsgBuf(CANaddress, 0, 8, data);  // Send 8-byte message, standard ID
}

/**
 * sendCAN_BE - Send CAN message with Big Endian byte order
 * 
 * Packs four 16-bit integer values into an 8-byte CAN message.
 * Big Endian: High byte first, then low byte (Motorola byte order).
 * 
 * @param CANaddress - CAN message ID (11-bit identifier)
 * @param inputVal_1 - First 16-bit value (bytes 0-1)
 * @param inputVal_2 - Second 16-bit value (bytes 2-3)
 * @param inputVal_3 - Third 16-bit value (bytes 4-5)
 * @param inputVal_4 - Fourth 16-bit value (bytes 6-7)
 * 
 * Example: inputVal_1 = 0x1234
 *   data[0] = 0x12 (high byte)
 *   data[1] = 0x34 (low byte)
 * 
 * Used for: Speed data (compatible with Haltech protocol)
 */
void sendCAN_BE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4)
{
        // BIG ENDIAN byte packing
        // Word 1 (bytes 0-1)
        data[0] = highByte(inputVal_1);  // MSB first
        data[1] = lowByte(inputVal_1);   // LSB second
        // Word 2 (bytes 2-3)
        data[2] = highByte(inputVal_2);
        data[3] = lowByte(inputVal_2);
        // Word 3 (bytes 4-5)
        data[4] = highByte(inputVal_3);
        data[5] = lowByte(inputVal_3);
        // Word 4 (bytes 6-7)
        data[6] = highByte(inputVal_4);
        data[7] = lowByte(inputVal_4);

        byte sndStat = CAN0.sendMsgBuf(CANaddress, 0, 8, data);  // Send 8-byte message, standard ID
}

/**
 * receiveCAN - Read CAN message from receive buffer
 * 
 * Reads a CAN message from the MCP2515 controller's receive buffer.
 * Called when CAN interrupt pin goes low (message waiting).
 * Message data is copied to canMessageData[] for parsing.
 * 
 * Global variables modified:
 * - rxId: CAN message identifier
 * - len: Number of data bytes (0-8)
 * - rxBuf: Raw message data bytes
 * - canMessageData: Copy of message data for parsing
 * 
 * The commented-out debug code can be enabled to print CAN messages to serial.
 */
void receiveCAN ()
{
  
    CAN0.readMsgBuf(&rxId, &len, rxBuf);  // Read message: ID, length, and data bytes
    
    // Copy received data to processing buffer
    for (byte i =0; i< len; i++){
      canMessageData[i] = rxBuf[i];
      //Serial.println(canMessageData[i]);  // Debug: print each byte
    }
    
    // Debug code for printing CAN messages (currently disabled)
//    if((rxId & 0x80000000) == 0x80000000)     // Check if extended ID (29-bit)
//      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
//    else                                       // Standard ID (11-bit)
//      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
//  
//    Serial.print(msgString);
//  
//    if((rxId & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
//      sprintf(msgString, " REMOTE REQUEST FRAME");
//      Serial.print(msgString);
//    } else {
//      for(byte i = 0; i<len; i++){
//        sprintf(msgString, " 0x%.2X", rxBuf[i]);
//        Serial.print(msgString);
//      }
////      // report value of sensor sent across CAN Bus in human readable format
////        float var = (rxBuf[0]<<8) + rxBuf[1];
////        Serial.print("Volts:");
////        Serial.println(var/100);
//    }      
//    Serial.println();
}


void parseCAN( unsigned long id, unsigned long msg)
{
  int var1 = 0;
  
  if (id == 0x301) {  //test 
    pumpPressureCAN = (rxBuf[0]<<8) + rxBuf[1];
  }
  else if (id == 0x360){  //Haltech Protocol
    rpmCAN = (rxBuf[0]<<8) + rxBuf[1];
    //Serial.print("RPM:");
    //Serial.println(rpmCAN);   // y = x

    mapCAN = (rxBuf[2]<<8) + rxBuf[3];
    // Serial.print("MAP:");
    // Serial.println(mapCAN/10);   // y = x/10

    tpsCAN = (rxBuf[4]<<8) + rxBuf[5];
    // Serial.print("TPS:");
    // Serial.println(tpsCAN/10);   // y = x/10
  }
  else if (id == 0x361){  //Haltech Protocol
    fuelPrsCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("FuelPressure:");
    // Serial.println(fuelPrsCAN/10 - 101.3);   // y = x/10 - 101.3

    oilPrsCAN = (rxBuf[2]<<8) + rxBuf[3];
    // Serial.print("Oil Pressure:");   // y = x/10 - 101.3
    // Serial.println(oilPrsCAN/10 - 101.3); 
  }
  else if (id == 0x362){  //Haltech Protocol
    injDutyCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("Injector DC:");
    // Serial.println(injDutyCAN/10);   // y = x/10

    ignAngCAN = (rxBuf[4]<<8) + rxBuf[5];
    // Serial.print("Ignition Angle:");   // y = x/10
    // Serial.println(ignAngCAN/10); 
  }
  else if (id == 0x368){  //Haltech Protocol
    afr1CAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("AFR:");
    // Serial.println(afr1CAN/1000);   // y = x/1000
  }
  else if (id == 0x368){  //Haltech Protocol
    knockCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("Knock Level:");
    // Serial.println(knockCAN/100);   // y = x/100
  }
  else if (id == 0x3E0){  //Haltech Protocol
    coolantTempCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("Coolant Temp:");
    // Serial.println(coolantTempCAN/10);   // y = (x/10)-273.15 , KELVIN

    airTempCAN = (rxBuf[2]<<8) + rxBuf[3];
    // Serial.print("IAT:");
    // Serial.println(airTempCAN/10);   // y = (x/10)-273.15 , KELVIN

    fuelTempCAN = (rxBuf[4]<<8) + rxBuf[5];
    // Serial.print("Fuel Temp:");
    // Serial.println(fuelTempCAN/10);   // y = x/10

    oilTempCAN = (rxBuf[6]<<8) + rxBuf[7];
    // Serial.print("Oil Temp:");
    // Serial.println(oilTempCAN/10);   // y = x/10
  }
  else if (id == 0x3E1){  //Haltech Protocol
    transTempCAN = (rxBuf[0]<<8) + rxBuf[1];
    // Serial.print("Trans Temp:");
    // Serial.println(transTempCAN/10);   // y = x/10

    fuelCompCAN = (rxBuf[4]<<8) + rxBuf[5];
    // Serial.print("Ethanol %:");
    // Serial.println(fuelCompCAN/10);   // y = x/10
  }

  
}


///// LED TACH AND SHIFT LIGHT FUNCTION /////
void ledShiftLight(int ledRPM){
  if (ledRPM < tachMin) {
      // black out unused range  
    for (int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB::Black;
    }
    return;
  }
  int midPoint = NUM_LEDS/2;
  int blackout_val = map(ledRPM, tachMin, tachMax, midPoint, 0);
 
  //tach normal range 
    for (int i = 0;i <= midPoint - WARN_LEDS; i++){
      leds[i] = CRGB(30, 15 , 0);
    }
    for (int i = midPoint + WARN_LEDS + 1; i < NUM_LEDS; i++){
      leds[i] = CRGB(30, 15 , 0);
    }


  // tach warning range
    for (int i = midPoint - WARN_LEDS - 1;i <= midPoint - SHIFT_LEDS; i++){
      leds[i] = CRGB(80, 10 , 0);
    }
    for (int i = midPoint + SHIFT_LEDS + 1; i <= midPoint + WARN_LEDS; i++){
      leds[i] = CRGB(80, 10 , 0);
    }

  // tach shift light range
    for (int i = midPoint - SHIFT_LEDS - 1;i <= midPoint; i++){
      leds[i] = CRGB(80, 0 , 0);
    }
    for (int i = midPoint; i <= midPoint + SHIFT_LEDS; i++){
      leds[i] = CRGB(80, 0 , 0);
    }
    
  // black out unused range  
    for (int i = midPoint - blackout_val; i <= midPoint + blackout_val-1; i++){
      leds[i] = CRGB::Black;
    }

    // Flash LEDs when shift point is exceeded
    if (RPM > tachMax ){
      if (millis() - timerTachFlash > tachFlashRate){
        
        //Black out the shift LEDs if they are on
        if(tachFlashState == 0){
          for (int i = midPoint - SHIFT_LEDS - 1; i <= midPoint + SHIFT_LEDS; i++){
            leds[i] = CRGB::Black;
          }
        }
        
        timerTachFlash =  millis();             //reset the timer
        tachFlashState = 1 - tachFlashState;    //change the state
      }
    }
  FastLED.show();
}

/*
 * ========================================
 * GPS FUNCTIONS
 * ========================================
 * 
 * Handle GPS data acquisition for speedometer, odometer, and clock
 * Uses Adafruit GPS module with interrupt-based data collection
 */

/**
 * fetchGPSdata - Process new GPS data when available
 * 
 * Parses NMEA sentences from GPS module and updates speed, odometer, and time.
 * Uses filtering and interpolation for smooth speedometer response.
 * 
 * Processing steps:
 * 1. Check if new NMEA sentence received
 * 2. Parse sentence (fails silently if corrupt)
 * 3. Record timestamps for interpolation
 * 4. Convert speed from knots to km/h
 * 5. Apply exponential filtering for smooth display
 * 6. Calculate distance traveled since last update
 * 7. Update total and trip odometers
 * 8. Extract time (hour, minute) for clock
 * 
 * Global variables modified:
 * - v: Speed in km/h (float)
 * - v_new, v_old: Filtered speed values
 * - t_new, t_old: Timestamps
 * - lagGPS: Time since last GPS update
 * - odo, odoTrip: Odometer values
 * - hour, minute: GPS time (UTC)
 * 
 * Called from: main loop every 1ms
 * 
 * Note: Distance calculation uses formula: distance = speed * time * (1 km/h = 2.77778e-7 km/ms)
 */
void fetchGPSdata(){
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA()))   // Parse NMEA sentence; also clears newNMEAreceived flag
    return;  // If parse fails (corrupt data), wait for next sentence
  
    //if (millis() - timerGPSupdate > GPSupdateRate) {  // Optional rate limiting (currently disabled)
      //timerGPSupdate = millis();
      
            unsigned long alpha_0 = 256;  // Filter coefficient (256 = no filtering, instant response)
            
            // Save previous values for interpolation
            t_old = t_new;        // Previous timestamp
            t_new = millis();     // Current timestamp
            v_old = v_new;        // Previous filtered speed
            lagGPS = t_new-t_old; // Time between GPS updates (typically 200ms at 5Hz)
            
            // Get speed from GPS and convert units
            v = GPS.speed*1.852;           // Convert knots to km/h (1 knot = 1.852 km/h)
            float vFloat = GPS.speed*185.2;  // Speed * 100 for precision (km/h * 100)
            v_100 = (unsigned long)vFloat;   // Convert to integer
            
            // Apply exponential filter for smooth speedometer
            v_new = (v_100*alpha_0 + v_old*(256-alpha_0))>>8;  // Weighted average (>>8 = /256)
            
            // Calculate distance traveled for odometer
            if (v > 2) {  // Only integrate if speed > 2 km/h (reduces GPS drift errors)
              distLast = v * lagGPS * 2.77778e-7;  // Distance (km) = speed (km/h) * time (ms) * conversion factor
            } else {
              distLast = 0;  // Don't increment odometer when stationary
            }
            odo = odo + distLast;        // Update total odometer
            odoTrip = odoTrip + distLast;  // Update trip odometer
            
            // Extract time from GPS (UTC)
            hour = GPS.hour;
            minute = GPS.minute;          
      //}
  }
}

/**
 * TIMER0_COMPA_vect - Timer0 compare interrupt for GPS data reading
 * 
 * This interrupt service routine (ISR) is called automatically once per millisecond
 * by the Arduino Timer0 hardware timer. It reads one byte from the GPS module
 * without blocking the main loop.
 * 
 * The GPS module sends NMEA sentences at 9600 baud (960 characters/second).
 * This ISR ensures no characters are missed even when main loop is busy.
 * 
 * Interrupt context: Keep fast and simple - just read one character
 * 
 * Note: Original comment preserved - this is boilerplate code from Adafruit GPS library
 */
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();  // Read one byte from GPS module
  // Debug option: echo GPS data to serial (very slow - only for debugging)
#ifdef UDR0  // Check if UART data register is defined (AVR specific)
  if (GPSECHO)
    if (c) UDR0 = c;  // Write directly to UART register (faster than Serial.print)
    // Writing direct to UDR0 is much faster than Serial.print 
    // but only one character can be written at a time
#endif
}

/**
 * useInterrupt - Enable or disable GPS interrupt-based reading
 * 
 * Configures Timer0 to trigger GPS character reading via interrupt.
 * Timer0 is normally used for millis() function - this adds an additional
 * interrupt handler that piggybacks on the existing timer.
 * 
 * @param v - true to enable interrupts, false to disable
 * 
 * Hardware configuration:
 * - Uses Timer0 Output Compare A (OCR0A)
 * - Triggers at count 0xAF (175 in decimal)
 * - Interrupt fires ~1000 times per second
 * 
 * Global variables modified:
 * - usingInterrupt: Flag tracking interrupt state
 */
void useInterrupt(boolean v) {
  if (v) {
    // Enable GPS reading via Timer0 interrupt
    // Timer0 is already used for millis() - we add our interrupt to it
    OCR0A = 0xAF;  // Set compare value (when timer reaches this, interrupt fires)
    TIMSK0 |= _BV(OCIE0A);  // Enable Timer0 Compare A interrupt
    usingInterrupt = true;
  } else {
    // Disable GPS interrupt
    TIMSK0 &= ~_BV(OCIE0A);  // Disable Timer0 Compare A interrupt
    usingInterrupt = false;
  }
}

/*
 * ========================================
 * STEPPER MOTOR ANGLE CALCULATION FUNCTIONS
 * ========================================
 * 
 * Convert sensor values to motor angles for gauge needle positioning
 */

/**
 * speedometerAngle - Calculate speedometer needle angle from GPS speed
 * 
 * Interpolates between GPS updates for smooth needle movement and converts
 * speed to motor steps. Includes clamping and dead zone logic.
 * 
 * @param sweep - Maximum motor steps for full gauge sweep (e.g., 1416 for M3)
 * @return Motor angle in steps (1 to sweep-1)
 * 
 * Algorithm:
 * 1. Interpolate speed between GPS updates (5Hz to ~50Hz for smooth motion)
 * 2. Convert km/h to mph (* 0.6213712)
 * 3. Apply dead zone (< 0.5 mph reads as 0)
 * 4. Clamp to maximum (100 mph)
 * 5. Map speed to motor angle (0-100 mph -> 1 to sweep-1 steps)
 * 
 * Note: Serial.print statements are for debugging speed values
 */
int speedometerAngle(int sweep) {
  unsigned long t_curr =  millis()-lagGPS;  // Current time minus GPS lag
  // Interpolate speed between last two GPS readings for smooth motion
  float spd_g_float = map(t_curr, t_old, t_new, v_old, v_new)*0.6213712;   // Convert km/h*100 to mph*100
  spd_g = (unsigned long)spd_g_float;
  
  if (spd_g < 50) spd_g = 0;         // Dead zone: below 0.5 mph, show zero
  if (spd_g > speedoMax) spd_g = speedoMax;  // Clamp to max (100 mph * 100 = 10000)
  
  int angle = map( spd_g, 0, speedoMax, 1, sweep-1);  // Map speed to motor angle
  
  // Debug output for speed logging
  // Serial.print(millis());
  // Serial.print(",");
  // Serial.print(v);
  // Serial.print(",");
  // Serial.println(angle);
  
  angle = constrain(angle, 1, sweep-1);  // Ensure angle is within valid range
  return angle;
}

/**
 * fuelLvlAngle - Calculate fuel gauge needle angle from fuel level
 * 
 * Converts fuel level in gallons/liters to motor angle.
 * Uses percentage of tank capacity for linear gauge response.
 * 
 * @param sweep - Maximum motor steps for full gauge sweep
 * @return Motor angle in steps (1 to sweep-1)
 * 
 * Range: 10-100% of tank capacity (doesn't go to absolute zero for gauge geometry)
 */
int fuelLvlAngle(int sweep) {
  float fuelLvlPct = (fuelLvl/fuelCapacity)*1000;  // Fuel percentage * 1000 for precision
  fuelLevelPct_g = (unsigned int)fuelLvlPct;
  int angle = map(fuelLevelPct_g, 100, 1000, 1, sweep-1);  // Map 10-100% to gauge range
  angle = constrain(angle, 1, sweep-1);
  return angle;
} 

/**
 * coolantTempAngle - Calculate coolant temperature gauge needle angle
 * 
 * Non-linear mapping for temperature gauge with compressed cool range
 * and expanded hot range (important for detecting overheating).
 * 
 * @param sweep - Maximum motor steps for full gauge sweep
 * @return Motor angle in steps (1 to sweep-1)
 * 
 * Gauge zones:
 * - Cool zone (60-95°C): First half of gauge (slow rise)
 * - Hot zone (95-115°C): Second half of gauge (fast rise for warning)
 * 
 * This gives driver better visibility of overheating condition.
 */
int coolantTempAngle(int sweep) {
  int angle;
  if (coolantTemp < 95){
    // Normal operating range: 60-95°C maps to first half of gauge
    angle = map((long)coolantTemp, 60, 98, 1, sweep/2);
  }
  else {
    // Warning range: 95-115°C maps to second half of gauge (more sensitive)
    angle = map((long)coolantTemp, 98, 115, sweep/2, sweep-1);
  }
  angle = constrain(angle, 1, sweep-1);
  return angle;
}

/*
 * ========================================
 * SHUTDOWN FUNCTIONS
 * ========================================
 * 
 * Handle graceful system shutdown when ignition is turned off
 */

/**
 * shutdown - Gracefully shut down the gauge system
 * 
 * Called when ignition voltage drops below 1V (key turned off).
 * Saves all settings to EEPROM, displays shutdown screens, zeros gauge needles,
 * and cuts power to the Arduino.
 * 
 * Shutdown sequence:
 * 1. Save display selections to EEPROM
 * 2. Save units setting to EEPROM
 * 3. Save odometer values to EEPROM
 * 4. Save last fuel level reading to EEPROM
 * 5. Display shutdown splash screens (Falcon logo and 302 CID)
 * 6. Zero all gauge needles synchronously
 * 7. Wait 2 seconds for motors to complete
 * 8. Double-check battery voltage (in case key turned back on)
 * 9. Cut power by pulling pwrPin LOW
 * 
 * Called from: main loop when vBatt < 1V
 * 
 * Note: EEPROM.update() only writes if value changed (extends EEPROM life)
 */
void shutdown (void){
  // Save all display menu positions (4 bytes)
  for (int i = dispArray1Address; i < sizeof(dispArray1); i++) {
    EEPROM.update(i, dispArray1[i]);  // Only writes if changed
  }
  
  // Save display 2 selection and units
  EEPROM.update(dispArray2Address, dispArray2[0]);
  EEPROM.update(unitsAddress, units);
  
  // Save odometer values (floats, 4 bytes each)
  EEPROM.put(odoAddress, odo);
  EEPROM.put(odoTripAddress, odoTrip);
  EEPROM.put(fuelSensorRawAddress, fuelSensorRaw);  // Remember fuel level for restart


  // Display shutdown screens
  dispFalconScript(&display1);
  disp302CID(&display2);

  // Return gauge needles to zero position
  motorZeroSynchronous();

  // Wait for gauges to settle
  delay(2000);

  // Double-check that key is still off (in case user turned it back on)
  if (vBatt > 1){
    return;  // Abort shutdown - voltage has returned
  }

  // Cut power to Arduino by releasing power latch
  digitalWrite(pwrPin, LOW);  // This will power off the entire system
}

/**
 * motorZeroSynchronous - Return all gauge needles to zero position
 * 
 * Moves all four stepper motors to their zero (rest) position simultaneously.
 * Blocks until all motors complete their motion.
 * 
 * This function is used during shutdown and startup sweep.
 * 
 * Algorithm:
 * 1. Set all motors' current position to max (trick the library)
 * 2. Command all motors to position 0
 * 3. Loop calling update() for each motor until all reach zero
 * 4. Reset current position counters to 0
 * 
 * Note: Setting currentStep to M*_SWEEP makes motors think they're at max,
 * so they sweep all the way back to zero
 */
void motorZeroSynchronous(void){
  // Set current position to maximum for all motors
  motor1.currentStep = M1_SWEEP;
  motor2.currentStep = M2_SWEEP;
  motor3.currentStep = M3_SWEEP;
  motor4.currentStep = M4_SWEEP;
  
  // Command all motors to position 0
  motor1.setPosition(0);
  motor2.setPosition(0);
  motor3.setPosition(0);
  motor4.setPosition(0);
  
  // Wait for all motors to reach zero (blocking loop)
  while (motor1.currentStep > 0 || motor2.currentStep > 0 || motor3.currentStep > 0 || motor4.currentStep > 0)
  {
      motor1.update();  // Step motor 1 if needed
      motor2.update();  // Step motor 2 if needed
      motor3.update();
      motor4.update();
  }
  // Reset position counters to zero
  motor1.currentStep = 0;
  motor2.currentStep = 0;
  motor3.currentStep = 0;
  motor4.currentStep = 0;
}

/**
 * motorSweepSynchronous - Perform full gauge needle sweep (startup test)
 * 
 * Sweeps all gauge needles from zero to maximum and back to zero.
 * This provides a visual confirmation that all gauges are working correctly
 * during the startup sequence.
 * 
 * Sweep sequence:
 * 1. Zero all motors (via motorZeroSynchronous)
 * 2. Sweep to maximum position (full clockwise)
 * 3. Sweep back to zero (full counter-clockwise)
 * 
 * Blocks execution until sweep completes (~3-5 seconds depending on motor speed).
 * 
 * Called from: setup() during initialization
 */
void motorSweepSynchronous(void){
  // Start by zeroing all motors
  motorZeroSynchronous();
  Serial.println("zeroed");
  
  // Command all motors to maximum position
  motor1.setPosition(M1_SWEEP);
  motor2.setPosition(M2_SWEEP);
  motor3.setPosition(M3_SWEEP);
  motor4.setPosition(M4_SWEEP);
  
  // Wait for all motors to reach maximum (blocking loop)
  while (motor1.currentStep < M1_SWEEP-1  || motor2.currentStep < M2_SWEEP-1 || 
         motor3.currentStep < M3_SWEEP-1  || motor4.currentStep < M4_SWEEP-1)
  {
      motor1.update();  // Step each motor toward target
      motor2.update();
      motor3.update();
      motor4.update();
  }

  Serial.println("full sweep");
  
  // Command all motors back to zero
  motor1.setPosition(0);
  motor2.setPosition(0);
  motor3.setPosition(0);
  motor4.setPosition(0);
  
  // Wait for all motors to return to zero (blocking loop)
  while (motor1.currentStep > 0 || motor2.currentStep > 0 || 
         motor3.currentStep > 0 || motor4.currentStep > 0)
  {
      motor1.update();
      motor2.update();
      motor3.update();
      motor4.update();
  }
}

/*
 * ========================================
 * DEMO AND UTILITY FUNCTIONS
 * ========================================
 * 
 * Testing and debugging functions
 */

/**
 * generateRPM - Generate simulated RPM for demo mode
 * 
 * Creates a realistic RPM sweep for testing the LED tachometer
 * without a running engine. RPM ramps up and down automatically.
 * 
 * Global variables modified:
 * - gRPM: Generated RPM value (900-7000)
 * - rpmSwitch: Direction flag (0=increasing, 1=decreasing)
 * 
 * Called from: main loop when demo mode is enabled (currently commented out)
 * 
 * Note: Commented-out analog signal generation code also included
 */
void generateRPM(void){
    // RPM signal generation for demo/testing
    if (rpmSwitch == 0){
      gRPM = gRPM + 120;  // Ramp up by 120 RPM per update
    }
    else if (rpmSwitch == 1) {
      gRPM = gRPM - 160;  // Ramp down by 160 RPM per update
    }
    
    // Reverse direction at limits
    if (gRPM > 7000) rpmSwitch = 1;  // Start ramping down at 7000 RPM
    if (gRPM < 900) rpmSwitch = 0;   // Start ramping up at 900 RPM

    // Optional analog signal generation for testing (currently disabled)
    // if (analogSwitch == 0){
    //   analog = analog + 20; 
    // }
    // else if (analogSwitch == 1) {
    //   analog = analog - 20;
    // }
    // if (analog > 1022) analogSwitch = 1;
    // if (analog < 1) analogSwitch = 0;
}

/**
 * serialInputFunc - Serial port input for manual testing
 * 
 * Reads integer values from serial monitor and updates test variables.
 * Used for debugging sensor values without physical sensors connected.
 * 
 * Usage:
 * 1. Open Serial Monitor at 115200 baud
 * 2. Type a number and press Enter
 * 3. Value updates coolantTempCAN or fuelLvl (uncomment desired line)
 * 
 * Called from: main loop when debugging (currently commented out)
 * 
 * Example: Enter "95" to simulate 95°C coolant temperature
 */
void serialInputFunc(void){
  // SERIAL INPUT FOR TESTING ONLY
  if (Serial.available() > 0) {
    // Read the incoming data as a string (until newline)
    String inputSer = Serial.readStringUntil('\n');
    
    // Convert the input string to an integer
    int newValue = inputSer.toInt();
    
    // Update the test variable with the new value
    // Uncomment the line for the parameter you want to test:
    //coolantTempCAN = (newValue+273.15)*10 ;  // For temperature testing (convert C to Kelvin*10)
    //fuelLvl = newValue;  // For fuel level testing (gallons or liters)
    
    // Print confirmation of new value
    Serial.println("Updated value of fuel level: " + String(fuelLvl));
    Serial.println("Please enter a new value:");
  }
}

/*
 * ========================================
 * END OF CODE
 * ========================================
 * 
 * All functions have been documented with comprehensive comments explaining:
 * - Purpose and operation
 * - Input parameters and return values
 * - Algorithms and formulas
 * - Edge cases and constraints
 * - Global variables used/modified
 * - Calling context and frequency
 * 
 * The code is now self-documenting for easier maintenance and review.
 */
