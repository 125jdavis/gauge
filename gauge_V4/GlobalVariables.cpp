/*
 * ========================================
 * GLOBAL VARIABLES IMPLEMENTATION
 * ========================================
 * 
 * This file contains the actual instantiation of all global variables
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#include "GlobalVariables.h"

// ===== ANALOG SENSOR INPUTS =====
// Battery Voltage Sensor
float vBatt = 12;
int vBattRaw = 12;
int filter_vBatt = 8;
int vBattPin = A0;
float vBattScaler = 0.040923;

// Fuel Level Sensor
int fuelSensorRaw;
int filter_fuel = 1;
int fuelPin = A3;

// Coolant/Oil Temperature Thermistor Sensor
float therm;
float thermSensor;
int filter_therm = 50;
int thermPin = A4;
int thermCAN;

// Barometric Pressure Sensor
unsigned long baro;
byte filter_baro = 4;
int baroPin = A5;

// Reserved Analog Sensors
float sensor_b;
byte filter_b = 12;
int analogPin6 = A6;

float sensor_c;
byte filter_c = 12;
int analogPin7 = A7;

// ===== GPS SPEED AND ODOMETER VARIABLES =====
unsigned long v_old = 0;
unsigned long v_new = 1;
unsigned long t_old = 0;
unsigned long t_new = 1;
unsigned long v_100 = 0;
float v = 0;
bool usingInterrupt = false;
int lagGPS;
int v_g;
float odo;
float odoTrip;
float distLast;
byte hour;
byte minute;

// ===== STEPPER MOTOR POSITION VARIABLES =====
unsigned int spd_g;
unsigned int fuelLevelPct_g;
unsigned int coolantTemp_g;

// ===== ROTARY ENCODER VARIABLES =====
bool stateSW = 1;
bool lastStateSW = 1;
unsigned int lastStateChangeTime = 0;
unsigned int debounceDelay = 50;
bool debounceFlag = 0;
bool button = 0;

// ===== TIMING VARIABLES =====
unsigned int timer0, timerDispUpdate, timerCANsend;
unsigned int timerSensorRead, timerTachUpdate, timerTachFlash;
unsigned int timerCheckGPS, timerGPSupdate, timerAngleUpdate;

// Update rate periods
unsigned int CANsendRate = 50;
unsigned int dispUpdateRate = 75;
unsigned int sensorReadRate = 10;
unsigned int tachUpdateRate = 50;
unsigned int tachFlashRate = 50;
unsigned int GPSupdateRate = 100;
unsigned int checkGPSRate = 1;
unsigned int angleUpdateRate = 20;
unsigned int splashTime = 1500;

// ===== LED TACHOMETER VARIABLES =====
unsigned int tachMax = 6000;
unsigned int tachMin = 3000;
bool tachFlashState = 0;

// ===== CAN BUS ENGINE PARAMETERS =====
int rpmCAN;
int mapCAN;
int tpsCAN;
int fuelPrsCAN;
int oilPrsCAN;
int injDutyCAN;
int ignAngCAN;
int afr1CAN;
int knockCAN;
int coolantTempCAN;
int airTempCAN;
int fuelTempCAN;
int oilTempCAN;
int transTempCAN;
int fuelCompCAN;
int fuelLvlCAN;
int baroCAN;
int spdCAN;
int pumpPressureCAN;

// ===== PROCESSED ENGINE PARAMETERS FOR DISPLAY =====
float oilPrs = 25;
float coolantTemp = 0;
float fuelPrs = 43;
float oilTemp = 0;
float fuelLvl = 0;
float battVolt = 12.6;
float afr = 14.2;
float fuelComp = 0;
int RPM = 0;
int spd = 0;
float spdMph = 0;

// ===== CAN BUS COMMUNICATION BUFFERS =====
byte data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
byte canMessageData[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned long rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];

// ===== LOOKUP TABLES =====
// Thermistor Temperature Lookup Table
const int thermTable_length = 6;
float thermTable_x[thermTable_length] = {0.23, 0.67, 1.43, 3.70, 4.63, 4.95};
float thermTable_l[thermTable_length] = { 150,  105,   75,   25,   -5,  -40};

// Fuel Level Lookup Table
const int fuelLvlTable_length = 9;
float fuelLvlTable_x[fuelLvlTable_length] = {0.87, 1.03, 1.21, 1.40, 1.60, 1.97, 2.21, 2.25, 2.30};
float fuelLvlTable_l[fuelLvlTable_length] = {  16,   14,   12,   10,    8,    6,    4,    2,    0};
float fuelCapacity = 16;

// ===== EEPROM STORAGE ADDRESSES =====
byte dispArray1Address = 0;
byte dispArray2Address = 4;
byte clockOffsetAddress = 5;
byte odoAddress = 6;
byte odoTripAddress = 10;
byte fuelSensorRawAddress = 14;
byte unitsAddress = 18;
int *input;
int output = 0;

// ===== MENU NAVIGATION VARIABLES =====
byte menuLevel = 0;
byte units = 0;
unsigned int nMenuLevel = 15;
byte dispArray1[4] = { 1, 0, 0, 0 };
byte clockOffset = 0;
byte dispArray2[1] = {1};

// ===== DEMO/TEST VARIABLES =====
bool rpmSwitch = 0;
int gRPM;
int analog = 2;
int analogSwitch = 0;
