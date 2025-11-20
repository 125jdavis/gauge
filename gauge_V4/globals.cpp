/*
 * ========================================
 * GLOBAL VARIABLE DEFINITIONS
 * ========================================
 */

#include "globals.h"

// ===== HARDWARE OBJECT INSTANCES =====
MCP_CAN CAN0(CAN0_CS);
Adafruit_SSD1306 display1(SCREEN_W, SCREEN_H, &SPI, OLED_DC_1, OLED_RST_1, OLED_CS_1);
Adafruit_SSD1306 display2(SCREEN_W, SCREEN_H, &SPI, OLED_DC_2, OLED_RST_2, OLED_CS_2);
Rotary rotary = Rotary(2, 3);
CRGB leds[MAX_LEDS];
SwitecX12 motor1(M1_SWEEP, M1_STEP, M1_DIR);
SwitecX12 motor2(M2_SWEEP, M2_STEP, M2_DIR);
SwitecX12 motor3(M3_SWEEP, M3_STEP, M3_DIR);
SwitecX12 motor4(M4_SWEEP, M4_STEP, M4_DIR);
Stepper odoMotor(ODO_STEPS, ODO_PIN1, ODO_PIN2, ODO_PIN3, ODO_PIN4);
Adafruit_GPS GPS(&Serial2);

// ===== ANALOG SENSOR READINGS =====
float vBatt = 12;              // Current battery voltage in volts (filtered)
int vBattRaw = 12;             // Raw battery reading (0-500, representing 0-5V after mapping)
int fuelSensorRaw;             // Raw fuel sensor ADC reading (0-500)
float therm;                   // Current temperature in Celsius (after lookup table conversion)
float thermSensor;             // Voltage reading from thermistor (0-5V)
int thermCAN;                  // Temperature formatted for CAN transmission (temp * 10)
float sensor_av1;              // Barometric pressure in kPa * 10
float sensor_av2;              // Reserved sensor B value
float sensor_av3;              // Reserved sensor C value

// ===== HALL EFFECT SPEED SENSOR VARIABLES =====
volatile unsigned long hallLastTime = 0;     // Last pulse time (micros)
volatile float hallSpeedRaw = 0;             // Most recent calculated speed (MPH)
float hallSpeedEMA = 0;                      // Filtered speed (MPH)

// ===== ENGINE RPM SENSOR VARIABLES =====
volatile unsigned long ignitionLastTime = 0; // Last ignition pulse time (micros)
volatile float engineRPMRaw = 0;             // Most recent calculated RPM (unfiltered)
float engineRPMEMA = 0;                      // Filtered RPM with exponential moving average

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
// Manage update rates for different subsystems
unsigned int timer0, timerDispUpdate, timerCANsend;
unsigned int timerSensorRead, timerTachUpdate, timerTachFlash;
unsigned int timerCheckGPS, timerGPSupdate, timerAngleUpdate;
unsigned int timerHallUpdate;
unsigned int timerEngineRPMUpdate;
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
// Note: data buffer moved to local in sendCAN_LE() and sendCAN_BE() functions
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
byte dispArray2[1] = {1};        // Menu selection for display 2 (single level)

