// Gauge Control Module 
// Jesse Davis
// 11/5/2023


///// LIBRARIES /////
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
//#include <Wire.h> //no I2C devices on this DCU
#include <mcp_can.h>
#include <Rotary.h>
#include <EEPROM.h>
#include <FastLED.h>
#include <Adafruit_GPS.h>
#include <SwitecX25.h>
#include <SwitecX12.h>
#include <TimerOne.h> // not certain this is needed



///// DEFINE /////
//#define OLED_RESET 4  // OLED display reset pin
#define CAN0_CS 53  // CAN Bus Chip Select pin
#define CAN0_INT 18  // CAN Bus Interrupt pin


// GAUGE SETUP //

#define RESET_1 30          // motor driver reset pin

#define M1_SWEEP (90*12)     // range of motion for gauge motor 1 standard X25.168 range 315 degrees at 1/3 degree steps
#define M1_STEP  41         // motor 1 step command
#define M1_DIR   40         // motor 1 direction command

#define M2_SWEEP (135*12)    // range of motion for gauge motor 2
#define M2_STEP  39         // motor 2 step command
#define M2_DIR   38         // motor 2 direction command

#define M3_SWEEP (135*12)    // range of motion for gauge motor 3
#define M3_STEP  44         // motor 3 step command
#define M3_DIR   45         // motor 3 direction command

#define M4_SWEEP (135*12)    // range of motion for gauge motor 4
#define M4_STEP  42         // motor 4 step command
#define M4_DIR   43         // motor 4 direction command

#define ODO_STEPS 32        // number of steps in one ODO revolution

// GPS
#define GPSECHO  false        // do not send raw GPS data to serial monitor 


//Rotary Encoder switch
#define SWITCH 24 

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

// LED Tach
// How many leds in your strip?
#define NUM_LEDS 26     // how many warning leds
#define WARN_LEDS 6     // how many warning LEDS on each side of midpoint (shift LEDS included)
#define SHIFT_LEDS 2    // how many shift light LEDS on each side of midpoint
#define TACH_DATA_PIN 22     // which pin sends data to LED tachometer

///// INITIALIZE /////
MCP_CAN CAN0(CAN0_CS);     // Set CS to pin 53
Adafruit_SSD1306 display1(SCREEN_W, SCREEN_H, &SPI, OLED_DC_1, OLED_RST_1, OLED_CS_1);
Adafruit_SSD1306 display2(SCREEN_W, SCREEN_H, &SPI, OLED_DC_2, OLED_RST_2, OLED_CS_2);
Rotary rotary = Rotary(2, 3);  // rotary encoder ipnput pins (2 and 3 are interrupts)
CRGB leds[NUM_LEDS];

SwitecX12 motor1(M1_SWEEP, M1_STEP, M1_DIR); // initialize motor 1
SwitecX12 motor2(M2_SWEEP, M2_STEP, M2_DIR); // initialize motor 2
SwitecX12 motor3(M3_SWEEP, M3_STEP, M3_DIR); // initialize motor 3
SwitecX12 motor4(M4_SWEEP, M4_STEP, M4_DIR); // initialize motor 4
SwitecX25 odoMotor(ODO_STEPS, 10, 11, 12, 13);          // initialize odometer motor
Adafruit_GPS GPS(&Serial2);   // set serial2 to GPS object

///// GLOBAL VARIABLES /////

// Analog inputs to Dash Control Module
// vBatt, on pin 0
float vBatt;
int filter_vBatt = 6; // out of 16, 16 = no filter
int analogPin0 = A0;

// fuel, on pin 3
float fuelSensor;
int filter_fuel = 6; // out of 16, 16 = no filter
int analogPin3 = A3;

// therm, on pin 4
float therm;
float thermSensor;
int filter_therm = 50; // out of 100, 100 = no filter
int analogPin4 = A4;
int thermCAN;

// sensor a (baro), on pin 5
int baro;
int filter_baro = 6; // out of 16, 16 = no filter
int analogPin5 = A5;

// sensor b, on pin 6
float sensor_b;
int filter_b = 12;
int analogPin6 = A6;

// sensor c, on pin 7
float sensor_c;
int filter_c = 12;
int analogPin7 = A7;

// GPS Speed
unsigned long v_old = 0;
unsigned long v_new = 1;
unsigned long t_old = 0;
unsigned long t_new = 1;
unsigned long v_100 = 0;
float v = 0;
bool usingInterrupt = false;
int lagGPS;
int v_g;

// Rotary Encoder Variables
bool stateSW = 1;
bool lastStateSW = 1;
unsigned long lastStateChangeTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;       // the debounce time; increase if the output flickers
bool debounceFlag = 0;
bool button = 0;

// timers and refresh rates
long unsigned timer0, timerDispUpdate, timerCANsend, timerSensorRead, timerTachUpdate, timerTachFlash, timerGPSupdate;

//long unsigned dispMenuRate = 20;
long unsigned CANsendRate = 100;
long unsigned dispUpdateRate = 50;
long unsigned sensorReadRate = 100;
long unsigned tachUpdateRate = 50;
long unsigned tachFlashRate = 50;
long unsigned GPSupdateRate = 100;
int splashTime = 1500; //how long should the splash screens show?

// LED Tach variables
int tachMax = 4000;
int tachMin = 6000;
bool tachFlashState = 0;

//Engine Parameters from CAN Bus
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

//Engine Parameters for Display
float oilPrs = 25;
float coolantTemp = 180;
float fuelPrs = 43;
float oilTemp = 200;
float fuelLvl = 75;
float battVolt = 12.6;
float afr = 14.2;
int RPM = 2000;
int Speed = 25;

// CAN Bus variables
byte data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
byte canMessageData [8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned long rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128]; 

// Thermistor lookup table
const int thermTable_length = 6;
float thermTable_x[thermTable_length] = {0.23, 0.67, 1.43, 3.70, 4.63, 4.95};
float thermTable_l[thermTable_length] = { 150,  105,   75,   25,   -5,  -40};

// Fuel Level lookup table
const int fuelLvlTable_length = 9;
float fuelLvlTable_x[fuelLvlTable_length] = {2.30, 2.25, 2.21, 1.97, 1.60, 1.40, 1.21, 1.03, 0.87};
float fuelLvlTable_l[fuelLvlTable_length] = {   0,    2,    4,    6,    8,   10,   12,   14,   16};

// EEPROM Variables
byte clockOffsetAddress;  //EEPROM Address
int *input;               //this is a memory address
int output = 0;

// Menu Navigation Variables
byte menuLevel = 0;
char units = 'Imperial';
unsigned int nMenuLevel = 14; //This should be the number of menu items on the given level
byte dispArray1[4] = { 1, 0, 0, 0 };  //should be written to EEPROM 0-3
unsigned int clockOffset;
byte dispArray2[1] = {0};


///// IMAGES /////
// 'falcon_script', 128x32px
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

// 'Battery Icon', 38x32px
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

// DEMO VARIABLES
bool rpmSwitch = 0;

// Signal selection  //
void sigSelect (void) {
    Speed = v_new; //multiply by unit conversion
    RPM = rpmCAN/10;
    coolantTemp = coolantTempCAN/10;
    oilPrs = oilPrsCAN/10;
    fuelPrs = fuelPrsCAN/10;
    afr = afr1CAN/10;
    fuelLvlCAN = (int)fuelLvl;

}

///// SETUP LOOP //////////////////////////////////////////////////////////////////
void setup() {

  Serial.begin(115200); // open the serial port at 115200 bps:

    // Write dispArray1 values from initialization into EEPROM. this is not necessary code
  for (int i = 0; i < sizeof(dispArray1); i++) {
    EEPROM.write(i, dispArray1[i]);
  }

  // Initialize LED Tach
  FastLED.addLeds<WS2812, TACH_DATA_PIN, GRB>(leds, NUM_LEDS);
  
  // Initialize displays
  display1.begin(SSD1306_SWITCHCAPVCC);  // initialize with SPI
  display2.begin(SSD1306_SWITCHCAPVCC);  // initialize with SPI
  dispFalconScript(&display1);
  disp302CID(&display2);

  // Set up rotary switch interrupts
  attachInterrupt(0, rotate, CHANGE);
  attachInterrupt(1, rotate, CHANGE);
  
  //read display array from EEPROM and print to Serial Monitor. 
  //will need to change this code to actually control display screens instead of just serial print
  for (int i = 0; i < sizeof(dispArray1); i++) {
    byte temp = EEPROM.read(i);
    Serial.println(temp);
  }

  //fetch last known clock offset from EEPROM
  clockOffset = EEPROM.read(clockOffsetAddress);  
  
  // Initialize MCP2515 running at 8MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) Serial.println("MCP2515 Initialized Successfully!");
  else Serial.println("Error Initializing MCP2515...");

  // Set up CAN interrupt pin
  pinMode(CAN0_INT, INPUT); 
  CAN0.setMode(MCP_NORMAL);   // Change to normal mode to allow messages to be transmitted

  //do nothing until splash screen timer runs out
  while (millis() < splashTime){
  }


}



///// MAIN LOOP //////////////////////////////////////////////////////////////////
void loop() {
    
  //read analog voltage and get filtered value
  if (millis() - timerSensorRead > sensorReadRate) {
    fuelSensor = readSensor(analogPin3,fuelSensor,filter_fuel);
    fuelLvl = curveLookup(fuelSensor, thermTable_x, thermTable_l, thermTable_length);
    thermSensor = readThermSensor(analogPin4, thermSensor, filter_therm);
    therm = curveLookup(thermSensor, thermTable_x, thermTable_l, thermTable_length);
    thermCAN = therm*10;
    baro = read30PSIAsensor(analogPin5,baro,filter_baro);  
  //sensor_b = readSensor(analogPin6,sensor_b,filter_b); 
  //sensor_c = readSensor(analogPin7,sensor_c,filter_c); 
    timerSensorRead = millis();
  }


  //Send CAN messages at specified rate
  if (millis() - timerCANsend > CANsendRate) {  
    //sendCAN_200(sensor_a);
    sendCAN_203(thermCAN, fuelLvlCAN, baro, 1000);
    timerCANsend = millis();
  }


  //Read CAN messages as they come in
  if(!digitalRead(CAN0_INT)){     // If CAN0_INT pin is low, read receive buffer
    receiveCAN ();
    parseCAN( rxId, rxBuf);
  }

  
  //Tach update timer
  if (millis() - timerTachUpdate > tachUpdateRate) {  
    
    // // RPM signal generation for demo
    // if (rpmSwitch == 0){
    //   RPM = RPM + 120; 
    // }
    // else if (rpmSwitch == 1) {
    //   RPM = RPM - 160;
    // }
    // if (RPM > 7000) rpmSwitch = 1;
    // if (RPM < 2000) rpmSwitch = 0;
    
    
    //ledShiftLight(RPM);
    //timerTachUpdate = millis();        // reset timer1 
  }

  sigSelect();
  //OLED Displays
  swRead();
  if(millis() - timerDispUpdate > dispUpdateRate){
    dispMenu();
    disp2();
    timerDispUpdate = millis();
  }
}





///// ALL FUNCTIONS /////////////////////////////////////////////////////////////////

////// SENSOR READING FUNCTIONS ///// 
// Generic Sensor reader - reads, re-maps, and filters analog input values
int readSensor(int inputPin, int oldVal, int filt)  // read voltage, map to 0-5v, and filter
{
    int raw = analogRead (inputPin);
    int newVal = map( raw, 0, 1023, 0, 500);  
    int filtVal = ((newVal*filt) + (oldVal*(16-filt)))>>4;
    return filtVal; 
}

// Reads 30 PSI Absolute Sensor
int read30PSIAsensor(int inputPin, int oldVal, int filt)  // read voltage, map to 0-30 PSIA, and filter
{
    int raw = analogRead (inputPin);
    int newVal = map( raw, 102, 921, 0, 2068);  
    int filtVal = ((newVal*filt) + (oldVal*(16-filt)))>>4;
    return filtVal; 
}

// Reads GM CLT/IAT Thermistor
float readThermSensor(int inputPin, float oldVal, int filt)  // read voltage, map to -40-150 deg C, and filter
{
    int raw = analogRead (inputPin);
    float newVal = map( raw, 0, 1023, 0, 500)*0.01; 
    float filtVal = ((newVal*filt) + (oldVal*(100-filt)))*0.01;
    return filtVal; 
}

// Generic Curve Lookup
float curveLookup(float input, float brkpts[], float curve[], int curveLength){
  int index = 1;

  //find input's position within the breakpoints
  for (int i = 0; i <= curveLength-1; i++){
    if (input < brkpts[0]){
      float output = curve[0];
      return output;
    } 
    else if (input <= brkpts[i+1]){
      index = i+1;
      break;
    } 
    else if (input > brkpts[curveLength-1]){
      float output = curve[curveLength-1];
      return output;
    }
  } 

  // interpolation
  float x1 = brkpts[index];
  float x0 = brkpts[index-1];
  float y1 = curve[index];
  float y0 = curve[index-1];
  
  float output = (((y1-y0)/(x1-x0))*(input-x0))+y0;
  return output;
}


///// DISPLAY AND NAVIGATION FUNCTIONS /////
// Reads Encoder Switch and debounces
void swRead() {       
  stateSW = digitalRead(SWITCH);            // read the digital input pin
  int stateChange = stateSW - lastStateSW;  // calculate state change value

  if ((millis() - lastStateChangeTime) > debounceDelay) {
    debounceFlag = 0;  // flag will block state change if debounce time has not elapsed
  }

  if (stateChange < 0 && debounceFlag == 0) {  //if state change negative, button has been presed
    lastStateChangeTime = millis();            // reset the debouncing timer
    debounceFlag = 1;                          // set flag to block button bounce
  } else if (stateChange > 0 && debounceFlag == 0) {
    lastStateChangeTime = millis();    // reset the debouncing timer
    debounceFlag = 1;                  // set flag to block button bounce
    button = 1;
  } else if (stateChange = 0) {  // if state change = 0, nothing has happened
  }
  lastStateSW = stateSW;  // saves current switch state for next time
}

// General Encoder Incrementer for menu navigation 
void rotate() {
  //void rotate() {
  unsigned char result = rotary.process();
  if (result == DIR_CW) {
    if (dispArray1[menuLevel] == nMenuLevel) dispArray1[menuLevel] = 0; else dispArray1[menuLevel]++;  // increment up one within range given for current menu level
  } else if (result == DIR_CCW) {
    if (dispArray1[menuLevel] == 0l) dispArray1[menuLevel] = nMenuLevel; else dispArray1[menuLevel]--;  // increment down one within range given for current menu level
  }
}

// Controls display and menu navigation
void dispMenu() {
  switch (dispArray1[0]) {  // Level 0
    case 1:                //dispArray1 {1 0 0 0} Oil Pressure
      if (menuLevel == 0 && button == 1) {  //if button is pressed, go down one level
        goToLevel0();
      }
      Serial.println("Oil Pressure");
      dispOilPrsGfx(&display1);
      break;
    
    case 2:                //dispArray1 {2 0 0 0} Coolant Temp
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("Coolant Temp");
      dispCoolantTempGfx(&display1);
      break;
    
    case 3:                //dispArray1 {3 0 0 0} Oil Temp
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();      }
      Serial.println("Oil Temp");
      dispOilTemp(&display1);
      break;
    
    case 4:                //dispArray1 {4 0 0 0} Fuel Level
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("Fuel Level");
      dispFuelLvlGfx(&display1);
      break;
    
    case 5:                //dispArray1 {5 0 0 0} Odometer
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0(); 
      }
      Serial.println("Odometer");
      dispRectangle(&display1);
      break;    
    
    case 6:                //dispArray1 {6 0 0 0} Speed
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("Speed");
      dispSpd(&display1);
      break;  
    
    case 7:                //dispArray1 {7 0 0 0} RPM
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("RPM");
      dispRPM(&display1);
      break;  
    
    case 8:                //dispArray1 {8 0 0 0} Ignition Timing
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("ignition timing");
      dispBattVoltGfx(&display1);
      break;
    
    case 9:                //dispArray1 {9 0 0 0} AFR
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("AFR");
      dispAFR(&display1);
      break;  
    case 10:               //dispArray1 {10 0 0 0} Fuel Pressure
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("Fuel Pressure");
      dispFuelPrs(&display1);
      break;  

    case 11:               //dispArray1 {11 0 0 0} Fuel Composition
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("ethanol %");
      dispFuelComp(&display1);
      break;  
    
    case 12:               //dispArray1 {12 0 0 0} Clock
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("Clock");
      break;  
    
    case 13:               //dispArray1 {13 0 0 0} Battery Voltage
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("batt voltage");
      dispBattVoltGfx(&display1);
      break;  
    
    case 14:               //dispArray1 {14 0 0 0} Falcon Script
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      Serial.println("falcon Script");
      dispFalconScript(&display1);
      break;  

    case 0:                //dispArray1 {0 0 0 0} Settings // ALWAYS LAST SCREEN, ALWAYS CASE 0 //

      if (menuLevel == 0 && button == 1) {  //if button is pressed, change menu level
        button = 0;
        menuLevel = 1; // menu level 1 (0 indexed)
        nMenuLevel = 3 ; // number of screens on this level - 1 (0 indexed)
      } 
      else if (menuLevel == 0) {  //if no button is pressed, display settings
        Serial.println("settings");
        dispSettings(&display1);
      } 
      else {  // proceed to Level 0 screen 3 deeper levels

        switch (dispArray1[1]) {
          case 0: //Select screen for display2                  dispArray1 {0 0 0 0}
            if (menuLevel == 1 && button == 1) {
              button = 0;
              menuLevel = 2;  // menu level 2 (0 indexed)
              nMenuLevel = 8; // number of screens on this level - 1 (0 indexed)
            } 
            else if (menuLevel == 1) {
              Serial.println("Display 2");
              dispDisp2Select(&display1);
            } 
            else {
              switch (dispArray1[2]) {
                case 0:         // Oil Pressure on Display 2
                  Serial.println("Disp2: Oil Pressure");
                  dispArray2[0] = 0;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 1:         // Coolant Temp on Display 2
                  Serial.println("Disp2: Coolant Temp");
                  dispArray2[0] = 1;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 2:         // Battery Voltage on Display 2
                  Serial.println("Disp2: Battery Voltage");
                  dispArray2[0] = 2;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 3:         // Fuel Level on Display 2
                  Serial.println("Disp2: Fuel Level");
                  dispArray2[0] = 3;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 4:         // RPM on Display 2
                  Serial.println("Disp2: RPM");
                  dispArray2[0] = 4;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 5:         // Speed on Display 2
                  Serial.println("Disp2: Speed");
                  dispArray2[0] = 5;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 6:         // Speed on Display 2
                  Serial.println("Disp2: 302CID");
                  dispArray2[0] = 6;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 7:         // Speed on Display 2
                  Serial.println("Disp2: 302V");
                  dispArray2[0] = 7;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 8:         // Speed on Display 2
                  Serial.println("Disp2: Falcon Script");
                  dispArray2[0] = 8;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                
              }
            }
            break;
 
          case 1: //Select screen for display2                  dispArray1 {0 1 0 0}
            if (menuLevel == 1 && button == 1) {
              button = 0;
              menuLevel = 2;
              nMenuLevel = 1;
            } else if (menuLevel == 1) {
              Serial.println("Units");
              dispUnits(&display1);
            } else {
              switch (dispArray1[2]) {
                case 0:         // 'Merican Units
                  Serial.println("'Merican'");
                  units = 'Merican';
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 1:         // Metric Units
                  Serial.println("Metric");
                  units = 'Metric';
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
              }
            }
            break;

          case 2: //Clock Offset for modifying from GMT   dispArray1 {0 2 0 0}
            if (menuLevel == 1 && button == 1) {
              button = 0;
              menuLevel = 2;

            } else if (menuLevel == 1) {
              Serial.println("ClockOffset");
              dispClockOffset(&display1);
            } else {
              if (button == 1) {
                button = 0;
                dispArray1[0] = 0;
                dispArray1[1] = 0;
                dispArray1[2] = 0;
                menuLevel = 0;
                nMenuLevel = 4;
                detachInterrupt(0);
                detachInterrupt(1);
                attachInterrupt(0, rotate, CHANGE);
                attachInterrupt(1, rotate, CHANGE);
                EEPROM.write(clockOffset, clockOffsetAddress);
              } else {
                
                //Need code here to make the rotary encoder change the offset value
                detachInterrupt(0);
                detachInterrupt(1);
                attachInterrupt(0, incrementOffset, CHANGE);
                attachInterrupt(1, incrementOffset, CHANGE);


                Serial.print("clock Offset: ");
                Serial.println(clockOffset);
              }
            }
            break;
          
          case 3: // Exit from settings,                  dispArray1 {0 3 0 0}
            Serial.println("Exit");
            if (button == 1) {
                    goToLevel0();
            }
            break;
        }
        break;
      }
  }
}

// Navigation subfunction
void goToLevel0(void){
  button = 0;
  dispArray1[0] = 0;
  dispArray1[1] = 0;
  dispArray1[2] = 0;
  menuLevel = 0;
  nMenuLevel = 14;
}

// Display #2 Control function
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

    case 6: // 302CID
      disp302CID(&display2);
      break;

    case 7: // 302V
      disp302V(&display2);
      break;

    case 8: // Falcon Script
      dispFalconScript(&display2);
      break;
  }
}

// Increments Clock Offset
void incrementOffset() {
  //void rotate() {
  unsigned char result = rotary.process();
  if (result == DIR_CW) {
    if (clockOffset == 23) clockOffset = 0; else clockOffset++; // increment up one within range of 0-23
  } else if (result == DIR_CCW) {
    if(clockOffset == 0) clockOffset = 23; else clockOffset--;// increment down one within range of 0-23
  }
}

// void dispUpdate()   // Send voltage value to OLED display MAYBE NOT NEEDED
// {

//     display.setTextColor(WHITE); 
//     display.clearDisplay();             //clear buffer
//     display.setTextSize(2);             // text size
//     display.setCursor(1,1);
//     display.print("Sens A ");
//     display.print(baro/10, 0);
//     display.println("%");        
//     display.setCursor(1,18);
//     display.print("Sens B ");
//     display.print(sensor_b/10, 0);
//     display.println("%");  
//     // display.setTextSize(3); 
//     // display.setCursor(74,6);
//     // display.print(sensor_a/10, 0);
//     // display.println("%");         
//     display.display();


// }


///// SCREEN DRAWING FUNCTIONS /////
void dispSettings (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(20,8);
    display->println("Settings");                 
    display->display();
}

void dispDisp2Select (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(15,8);
    display->println("Display 2");                 
    display->display();
}

void dispUnits (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(32,8);
    display->println("Units");                 
    display->display();
}

void dispClockOffset (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(0,8);
    display->println("Clock Offset");                 
    display->display();
}

void dispRPM (Adafruit_SSD1306 *display){
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(3);             // text size
    display->setCursor(0,8);
    display->print("RPM: ");
    display->println(RPM, 0);                 
    display->display();
}

void dispSpd (Adafruit_SSD1306 *display){
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(3);             // text size
    display->setCursor(0,8);
    display->print("Speed: ");
    display->println(v_new, 0);              
    display->display();
}

void dispOilPrs (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(1,1);
    display->println("Oil");        
    display->setCursor(1,18);
    display->println("Press");
    display->setTextSize(3); 
    display->setCursor(72,6);
    display->println(oilPrs, 0);         
    display->display();
}

void dispCoolantTemp (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(1,1);
    display->println("Eng");        
    display->setCursor(1,18);
    display->println("Temp");
    display->setTextSize(3); 
    display->setCursor(66,6);
    display->println(coolantTemp, 0);         
    display->display();
}

void dispOilTemp (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(1,1);
    display->println("Oil");        
    display->setCursor(1,18);
    display->println("Temp");
    display->setTextSize(3); 
    display->setCursor(66,6);
    display->println(oilTemp, 0);         
    display->display();
}

void dispFuelPrs (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(1,1);
    display->println("Fuel");        
    display->setCursor(1,18);
    display->println("Prs");
    display->setTextSize(3); 
    display->setCursor(72,6);
    display->println(fuelPrs, 0);         
    display->display();
}

void dispFuelComp (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(1,1);
    display->println("Eth");        
    display->setTextSize(3); 
    display->setCursor(48,6);
    display->println(fuelCompCAN/10, 0);         
    display->display();
}

void dispFuelLvl (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(1,1);
    display->println("Fuel");        
    display->setCursor(1,18);
    display->println("Level");
    display->setTextSize(3); 
    display->setCursor(72,6);
    display->print(fuelLvl, 0);
    display->println("%");         
    display->display();
}

void dispAFR (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setCursor(18,6);
    display->setTextSize(3); 
    display->print(afr, 0);
    display->println("AFR");         
    display->display();
}

void dispRectangle (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(1,1);
    display->println("TEST SCREEN");        
    display->drawRect(0,0,128,32,SSD1306_WHITE);       
    display->display();
}

void dispFalconScript(Adafruit_SSD1306 *display) {
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_falcon_script, SCREEN_W, SCREEN_H, 1);
    display->display();
}

void disp302CID(Adafruit_SSD1306 *display) {
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_302_CID, SCREEN_W, SCREEN_H, 1);
    display->display();
}

void disp302V(Adafruit_SSD1306 *display) {
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_302V, SCREEN_W, SCREEN_H, 1);
    display->display();
}

void dispOilPrsGfx (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_oilPrs, 40, 32, 1);
    display->setTextSize(3); 
    display->setCursor(72,6);
    display->println(oilPrs, 0);         
    display->display();
}

void dispCoolantTempGfx (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_coolantTemp, 38, 32, 1);
    display->setTextSize(3); 
    display->setCursor(66,6);
    display->println(coolantTemp, 0);         
    display->display();
}

void dispBattVoltGfx (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_battVolt, 35, 32, 1);
    display->setTextSize(3); 
    display->setCursor(50,6);
    display->println(battVolt, 1);         
    display->display();
}

void dispFuelLvlGfx (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_fuelLvl, 32, 32, 1);
    display->setTextSize(3); 
    display->setCursor(56,6);
    display->print(fuelLvl, 0);
    display->println("%");         
    display->display();
}


///// CAN BUS FUNCTIONS /////

void sendCAN_200(int inputVal) //Send input value to CAN BUS at address 0x200
{
  // send data:  ID = 0x100, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send

       //BIG ENDIAN 
        data[1] = highByte(inputVal);
        data[0] = lowByte(inputVal);
//        byte sndStat = CAN0.sendMsgBuf(0x100, 0, 8, data);
        byte sndStat = CAN0.sendMsgBuf(0x200, 0, 8, data);
//        if(sndStat == CAN_OK){
//          Serial.println("Message Sent Successfully!");
//        } 
//        else {
//          Serial.println("Error Sending Message...");
//        }
}

void sendCAN_201(int inputVal) //Send input value to CAN BUS
{
        // BIG  ENDIAN
        //data[2] = highByte(inputVal);
        //data[3] = lowByte(inputVal);
        // LITTLE  ENDIAN
        data[1] = highByte(inputVal);
        data[0] = lowByte(inputVal);

        byte sndStat = CAN0.sendMsgBuf(0x201, 0, 8, data);
}

void sendCAN_202(int inputVal) //Send input value to CAN BUS
{
        // BIG  ENDIAN
        //data[2] = highByte(inputVal);
        //data[3] = lowByte(inputVal);
        // LITTLE  ENDIAN
        data[1] = highByte(inputVal);
        data[0] = lowByte(inputVal);
        byte sndStat = CAN0.sendMsgBuf(0x202, 0, 8, data);
}

void sendCAN_203(int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4) //Send input value to CAN BUS
{
        // LITTLE  ENDIAN 
        data[0] = lowByte(inputVal_1);
        data[1] = highByte(inputVal_1);
        // LITTLE  ENDIAN 
        data[2] = lowByte(inputVal_2);
        data[3] = highByte(inputVal_2);
        // LITTLE  ENDIAN 
        data[4] = lowByte(inputVal_3);
        data[5] = highByte(inputVal_3);
        // LITTLE  ENDIAN 
        data[6] = lowByte(inputVal_4);
        data[7] = highByte(inputVal_4);

        //Serial.println(inputVal_1);
        byte sndStat = CAN0.sendMsgBuf(0x203, 0, 8, data);
}

void receiveCAN ()  //Recive message from CAN BUS
{
  
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    for (byte i =0; i< len; i++){
      canMessageData[i] = rxBuf[i];
      //Serial.println(canMessageData[i]);
    }
//    if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
//      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
//    else
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
  
  if (id == 0x200) {  //test 
    Serial.print("Address 0x200 recognized ");
    var1 = (rxBuf[2]<<8) + rxBuf[3];
    Serial.print("Speed:");
    float dispVal = var1/16;
    Serial.println(dispVal);
  }
  else if (id == 0x360){  //Haltech Protocol
    rpmCAN = (rxBuf[0]<<8) + rxBuf[1];
    Serial.print("RPM:");
    Serial.println(rpmCAN);   // y = x

    mapCAN = (rxBuf[2]<<8) + rxBuf[3];
    Serial.print("MAP:");
    Serial.println(mapCAN/10);   // y = x/10

    tpsCAN = (rxBuf[4]<<8) + rxBuf[5];
    Serial.print("TPS:");
    Serial.println(tpsCAN/10);   // y = x/10
  }
  else if (id == 0x361){  //Haltech Protocol
    fuelPrsCAN = (rxBuf[0]<<8) + rxBuf[1];
    Serial.print("FuelPressure:");
    Serial.println(fuelPrsCAN/10 - 101.3);   // y = x/10 - 101.3

    oilPrsCAN = (rxBuf[2]<<8) + rxBuf[3];
    Serial.print("Oil Pressure:");   // y = x/10 - 101.3
    Serial.println(oilPrsCAN/10 - 101.3); 
  }
  else if (id == 0x362){  //Haltech Protocol
    injDutyCAN = (rxBuf[0]<<8) + rxBuf[1];
    Serial.print("Injector DC:");
    Serial.println(injDutyCAN/10);   // y = x/10

    ignAngCAN = (rxBuf[4]<<8) + rxBuf[5];
    Serial.print("Ignition Angle:");   // y = x/10
    Serial.println(ignAngCAN/10); 
  }
  else if (id == 0x368){  //Haltech Protocol
    afr1CAN = (rxBuf[0]<<8) + rxBuf[1];
    Serial.print("AFR:");
    Serial.println(afr1CAN/1000);   // y = x/1000
  }
  else if (id == 0x368){  //Haltech Protocol
    knockCAN = (rxBuf[0]<<8) + rxBuf[1];
    Serial.print("Knock Level:");
    Serial.println(knockCAN/100);   // y = x/100
  }
  else if (id == 0x3E0){  //Haltech Protocol
    coolantTempCAN = (rxBuf[0]<<8) + rxBuf[1];
    Serial.print("Coolant Temp:");
    Serial.println(coolantTempCAN/10);   // y = x/10

    airTempCAN = (rxBuf[2]<<8) + rxBuf[3];
    Serial.print("IAT:");
    Serial.println(airTempCAN/10);   // y = x/10

    fuelTempCAN = (rxBuf[4]<<8) + rxBuf[5];
    Serial.print("Fuel Temp:");
    Serial.println(fuelTempCAN/10);   // y = x/10

    oilTempCAN = (rxBuf[6]<<8) + rxBuf[7];
    Serial.print("Oil Temp:");
    Serial.println(oilTempCAN/10);   // y = x/10
  }
  else if (id == 0x3E1){  //Haltech Protocol
    transTempCAN = (rxBuf[0]<<8) + rxBuf[1];
    Serial.print("Trans Temp:");
    Serial.println(transTempCAN/10);   // y = x/10

    fuelCompCAN = (rxBuf[4]<<8) + rxBuf[5];
    Serial.print("Ethanol %:");
    Serial.println(fuelCompCAN/10);   // y = x/10
  }

  
}


///// LED TACH AND SHIFT LIGHT FUNCTION /////
void ledShiftLight(int ledRPM){
  if (ledRPM< tachMin) {
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
      leds[i] = CRGB(100, 38 , 0);
    }
    for (int i = midPoint + WARN_LEDS + 1; i < NUM_LEDS; i++){
      leds[i] = CRGB(100, 38 , 0);
    }


  // tach warning range
    for (int i = midPoint - WARN_LEDS - 1;i <= midPoint - SHIFT_LEDS; i++){
      leds[i] = CRGB(255, 30 , 0);
    }
    for (int i = midPoint + SHIFT_LEDS + 1; i <= midPoint + WARN_LEDS; i++){
      leds[i] = CRGB(255, 30 , 0);
    }

  // tach shift light range
    for (int i = midPoint - SHIFT_LEDS - 1;i <= midPoint; i++){
      leds[i] = CRGB(255, 0 , 0);
    }
    for (int i = midPoint; i <= midPoint + SHIFT_LEDS; i++){
      leds[i] = CRGB(255, 0 , 0);
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

///// GPS FUNCTIONS /////

// FECTCH GPS DATA //
void fetchGPSdata(){
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
    return;  // we can fail to parse a sentence in which case we should just wait for another  }
  
    if (millis() - timerGPSupdate > GPSupdateRate) { 
      timerGPSupdate = millis();        // reset timer2
            unsigned long alpha_0 = 192; // filter coefficeint to set speedometer response rate
              t_old = t_new;                     // save previous time value
              t_new = millis();                  // record time of GPS update
              v_old = v_new;                     // save previous value of velocity                       
              lagGPS = t_new-t_old;                 // time between updated
            v = GPS.speed*1.150779;              // fetch velocity from GPS object, convert to MPH             
            float vFloat = GPS.speed*115.0779;       // x100 to preserve hundredth MPH accuracy
            v_100 = (unsigned long)vFloat;           // convert to unsigned long       
            v_new = (v_100*alpha_0 + v_old*(256-alpha_0))>>8; //filtered velocity value
                        
      }
  }
}

//  ADAFRUIT GPS INTERRUPT FUNCTION  //
//  I don't really understand this code, but it works, so don't freaking mess with it
//  Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
#ifdef UDR0
  if (GPSECHO)
    if (c) UDR0 = c;  
    // writing direct to UDR0 is much much faster than Serial.print 
    // but only one character can be written at a time. 
#endif
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}

////// STEPPER MOTORS /////

//  GAUGE 1 DATA UPDATE FUNCTION  //
int speedometer () {

  v_g = map(millis()-lagGPS, t_old, t_new, v_old, v_new);               // interpolate values between GPS data fix
 
  
  if (v_g < 70 || v_g > 14000) {                              // bring speeds below 1.5mph and above 140 mph to zero
    v_g = 0;
  }

  int angle = map( v_g, 0, 6000, 0, M1_SWEEP);                  // calculate angle of gauge 
  return angle;                                               // return angle of motor
  
}
