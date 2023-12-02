// Gauge Control Module 
// Jesse Davis
// 11/27/2023


///// LIBRARIES /////
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
#include <TimerOne.h> // not certain this is needed



///// DEFINE /////
//#define OLED_RESET 4  // OLED display reset pin
#define CAN0_CS 53  // CAN Bus Chip Select pin
#define CAN0_INT 18  // CAN Bus Interrupt pin


// GAUGE SETUP //
#define pwrPin 49           // pin to keep power alive after key is off 
#define speedoMax (100)     // maximum mph x10

#define RESET_1 30          // motor driver reset pin

#define M1_SWEEP (110*12)     // range of motion for gauge motor 1 standard X25.168 range 315 degrees at 1/3 degree steps
#define M1_STEP  41         // motor 1 step command
#define M1_DIR   40         // motor 1 direction command

#define M2_SWEEP (60*12)    // range of motion for gauge motor 2
#define M2_STEP  39         // motor 2 step command
#define M2_DIR   38         // motor 2 direction command

#define M3_SWEEP (60*12)    // range of motion for gauge motor 3
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

SwitecX12 motor1(M1_SWEEP, M1_STEP, M1_DIR); // initialize motor 1 as Speedometer
SwitecX12 motor2(M2_SWEEP, M2_STEP, M2_DIR); // initialize motor 2 Coolant temp
SwitecX12 motor3(M3_SWEEP, M3_STEP, M3_DIR); // initialize motor 3 fuel level
SwitecX12 motor4(M4_SWEEP, M4_STEP, M4_DIR); // initialize motor 4
SwitecX25 odoMotor(ODO_STEPS, 10, 11, 12, 13);          // initialize odometer motor
Adafruit_GPS GPS(&Serial2);   // set serial2 to GPS object

///// GLOBAL VARIABLES /////

// Analog inputs to Dash Control Module
// vBatt, on pin 0
float vBatt;
int vBattRaw;
int filter_vBatt = 6; // out of 16, 16 = no filter
int vBattPin = A0;
float vBattScaler = 0.04; // voltage divider factor (in this case 4/100: r1 = 10k, r2 = 3.3k, 100 is the multiplier from ADC)

// fuel, on pin 3
float fuelSensor;
int filter_fuel = 1; // out of 16, 16 = no filter
int analogPin3 = A3;
float fuelCap = 16;

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
float odo;
byte hour;
byte minute;

// Rotary Encoder Variables
bool stateSW = 1;
bool lastStateSW = 1;
unsigned long lastStateChangeTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;       // the debounce time; increase if the output flickers
bool debounceFlag = 0;
bool button = 0;

// timers and refresh rates
long unsigned timer0, timerDispUpdate, timerCANsend, timerSensorRead, timerTachUpdate, timerTachFlash, timerCheckGPS, timerGPSupdate;

//long unsigned dispMenuRate = 20;
long unsigned CANsendRate = 50;
long unsigned dispUpdateRate = 75;
long unsigned sensorReadRate = 100;
long unsigned tachUpdateRate = 50;
long unsigned tachFlashRate = 50;
long unsigned GPSupdateRate = 100; // might not be needed
long unsigned checkGPSRate = 1;
int splashTime = 1500; //how long should the splash screens show?

// LED Tach variables
int tachMax = 6000;
int tachMin = 3000;
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
float oilTemp = 0;
float fuelLvl = 50;
float battVolt = 12.6;
float afr = 14.2;
float fuelComp = 0;
int RPM = 0;
int spd = 25;
float spdMph = 0;

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
byte dispArray1Address = 0;   // starting EEPEOM address for display 1, length is 4 
byte dispArray2Address = 4;   // EEPROM Address for display 2, length is 1
byte clockOffsetAddress = 5;  // EEPROM Address for 
int *input;               //this is a memory address
int output = 0;

// Menu Navigation Variables
byte menuLevel = 0;
byte units = 0;  // 0 = metric, 1 = 'Merican
unsigned int nMenuLevel = 15; //This should be the number of menu items on the given level
byte dispArray1[4] = { 1, 0, 0, 0 };  //should be written to EEPROM 0-3
unsigned int clockOffset;
byte dispArray2[1] = {1};


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

// 'Oil Temp Icon', 40x32px
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
int gRPM;
int analog = 2;
int analogSwitch = 0;

// Signal selection  //
void sigSelect (void) {
    spd = v_new; //km/h * 100
    //spdMph = spd *0.6213712;
    RPM = rpmCAN;
    coolantTemp = (coolantTempCAN/10)-273.15; // convert kelvin to C;
    oilPrs = (oilPrsCAN/10)-101.3;   //kPa, convert to gauge pressure
    fuelPrs = (fuelPrsCAN/10)-101.3;  //kPa, convert to gauge pressure
    afr = afr1CAN/1000;
    fuelComp = fuelCompCAN/10;
    fuelLvlCAN = (int)fuelLvl;

}

///// SETUP LOOP //////////////////////////////////////////////////////////////////
void setup() {

  Serial.begin(115200); // open the serial port at 115200 bps:

  // Keep Power on until shut off by arduino
  pinMode(pwrPin, OUTPUT);
  digitalWrite(pwrPin, HIGH);

  // GPS setup
  GPS.begin(9600);                                // initialize GPS at 9600 baud
  //GPS.sendCommand(PMTK_SET_BAUD_57600);         // increase baud rate to 57600
  //GPS.begin(57600);                             // initialize GPS at 57600
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);  // set GPS module to fetch NMEA RMC only data
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);      // set GPS module to update at 5Hz
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);      // set GPS module position fix to 5Hz

  useInterrupt(true);                             // allow use of interrupts for GPS data fetching


  // Initialize Stepper Motors
  motor1.setPosition(0);
  motor2.setPosition(0);
  motor3.setPosition(0);
    while (motor1.currentStep > 0 || motor2.currentStep > 0 || motor3.currentStep > 0)
  {
      motor1.update();
      motor2.update();
      motor3.update();
  }
  motor1.currentStep = 0;
  motor2.currentStep = 0;
  motor3.currentStep = 0;

  // sweep motors through full range of motion
  motor1.targetStep = M1_SWEEP;
  motor2.targetStep = M2_SWEEP;
  motor3.targetStep = M3_SWEEP;
  while (motor1.currentStep < M1_SWEEP || motor2.currentStep < M2_SWEEP || motor3.currentStep < M3_SWEEP)  
  {
      motor1.update();
      motor2.update();
      motor3.update();
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
  // Display 1
  for (int i = dispArray1Address; i < sizeof(dispArray1); i++) {
    dispArray1[i] = EEPROM.read(i);
  }
  //Display 2
  dispArray2[0] = EEPROM.read(dispArray2Address);

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
    vBattRaw = readSensor(vBattPin, vBattRaw, filter_vBatt);
    vBatt = (float)vBattRaw*vBattScaler;
    // Serial.println(vBattRaw);
    // Serial.println(vBatt);
    fuelSensor = readSensor(analogPin3,fuelSensor,filter_fuel);
    fuelLvl = curveLookup(fuelSensor, fuelLvlTable_x, fuelLvlTable_l, fuelLvlTable_length);
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

  //Check for new GPS and process if new data is present
  if (millis() - timerCheckGPS > checkGPSRate) {
    fetchGPSdata();
  }
  
  //Tach update timer
  if (millis() - timerTachUpdate > tachUpdateRate) {     
    // demoRPM = generateRPM();    
    ledShiftLight(RPM);
    timerTachUpdate = millis();        // reset timer1 
  }

  sigSelect();
  //OLED Displays
  swRead();
  if(millis() - timerDispUpdate > dispUpdateRate){
    dispMenu();
    disp2();
    timerDispUpdate = millis();
  }

  // Check for key off, if switched voltage supply is below 1v, turn off control module
  if (vBatt < 1){
    shutdown();
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
      //Serial.println("Oil Pressure");
      dispOilPrsGfx(&display1);
      break;
    
    case 2:                //dispArray1 {2 0 0 0} Coolant Temp
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("Coolant Temp");
      dispCoolantTempGfx(&display1);
      break;
    
    case 3:                //dispArray1 {3 0 0 0} Oil Temp
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();      }
      //Serial.println("Oil Temp");
      dispOilTempGfx(&display1);
      break;
    
    case 4:                //dispArray1 {4 0 0 0} Fuel Level
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("Fuel Level");
      dispFuelLvlGfx(&display1);
      break;
    
    case 5:               //dispArray1 {5 0 0 0} Battery Voltage
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("batt voltage");
      dispBattVoltGfx(&display1);
      break;

    case 6:               //dispArray1 {6 0 0 0} Clock
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("Clock");
      dispClock(&display1);
      break;  

    case 7:                //dispArray1 {7 0 0 0} Odometer
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0(); 
      }
      //Serial.println("Odometer");
      dispTripOdo(&display1);
      break;    
    
    case 8:                //dispArray1 {8 0 0 0} Speed
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("Speed");
      dispSpd(&display1);
      break;  
    
    case 9:                //dispArray1 {9 0 0 0} RPM
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("RPM");
      dispRPM(&display1);
      break;  
    
    case 10:                //dispArray1 {10 0 0 0} Ignition Timing
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("ignition timing");
      dispIgnAng(&display1);
      break;
    
    case 11:                //dispArray1 {11 0 0 0} AFR
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("AFR");
      dispAFR(&display1);
      break;  
    
    case 12:               //dispArray1 {12 0 0 0} Fuel Pressure
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("Fuel Pressure");
      dispFuelPrs(&display1);
      break;  

    case 13:               //dispArray1 {13 0 0 0} Fuel Composition
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("ethanol %");
      dispFuelComp(&display1);
      break;  

    case 14:               //dispArray1 {14 0 0 0} INJ DUTY
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("Inj Duty");
      dispInjDuty(&display1);
      break;
      
    case 15:               //dispArray1 {15 0 0 0} Falcon Script
      if (menuLevel == 0 && button == 1) {  //if button is pressed, do nothing
        goToLevel0();
      }
      //Serial.println("falcon Script");
      dispFalconScript(&display1);
      break;  

    case 0:                //dispArray1 {0 0 0 0} Settings // ALWAYS LAST SCREEN, ALWAYS CASE 0 //

      if (menuLevel == 0 && button == 1) {  //if button is pressed, change menu level
        button = 0;
        menuLevel = 1; // menu level 1 (0 indexed)
        nMenuLevel = 3 ; // number of screens on this level - 1 (0 indexed)
      } 
      else if (menuLevel == 0) {  //if no button is pressed, display settings
        //Serial.println("settings");
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
              //Serial.println("Display 2");
              dispDisp2Select(&display1);
            } 
            else {
              switch (dispArray1[2]) {
                case 0:         // Oil Pressure on Display 2
                  //Serial.println("Disp2: Oil Pressure");
                  dispArray2[0] = 0;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 1:         // Coolant Temp on Display 2
                  //Serial.println("Disp2: Coolant Temp");
                  dispArray2[0] = 1;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 2:         // Battery Voltage on Display 2
                  //Serial.println("Disp2: Battery Voltage");
                  dispArray2[0] = 2;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 3:         // Fuel Level on Display 2
                  //Serial.println("Disp2: Fuel Level");
                  dispArray2[0] = 3;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 4:         // RPM on Display 2
                  //Serial.println("Disp2: RPM");
                  dispArray2[0] = 4;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 5:         // Speed on Display 2
                  //Serial.println("Disp2: Speed");
                  dispArray2[0] = 5;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;

                case 6:         // Clock on Display 2
                  //Serial.println("Clock");
                  dispArray2[0] = 6;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;

                case 7:         // 302CID on Display 2
                  //Serial.println("Disp2: 302CID");
                  dispArray2[0] = 7;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 8:         // 302V on Display 2
                  //Serial.println("Disp2: 302V");
                  dispArray2[0] = 8;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 9:         // Falcon Script on Display 2
                  //Serial.println("Disp2: Falcon Script");
                  dispArray2[0] = 9;
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
              //Serial.println("Units");
              dispUnits(&display1);
            } else {
              switch (dispArray1[2]) {
                case 0:         // Metric Units
                  display1.setTextColor(WHITE); 
                  display1.clearDisplay();             //clear buffer
                  display1.setTextSize(2);             // text size
                  display1.setCursor(31,8);
                  display1.println("Metric");                 
                  display1.display();
                  units = 0;
                  if (button == 1) {
                    // save this setting to the EEPROM
                    goToLevel0();
                  }
                  break;
                case 1:         // Merican Units
                  display1.setTextColor(WHITE); 
                  display1.clearDisplay();             //clear buffer
                  display1.setTextSize(2);             // text size
                  display1.setCursor(20,8);
                  display1.println("'Merican");                 
                  display1.display();
                  units = 1;
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
              // Serial.println("ClockOffset");
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
                // Serial.print("clock Offset: ");
                // Serial.println(clockOffset);
                dispClock(&display1);

              }
            }
            break;
          
          case 3: // Exit from settings,                  dispArray1 {0 3 0 0}
            display1.setTextColor(WHITE); 
            display1.clearDisplay();             //clear buffer
            display1.setTextSize(2);             // text size
            display1.setCursor(35,8);
            display1.println("EXIT");                 
            display1.display();
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

    case 6: // Clock
      dispClock(&display2);
      break;
    
    case 7: // 302CID
      disp302CID(&display2);
      break;

    case 8: // 302V
      disp302V(&display2);
      break;

    case 9: // Falcon Script
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


///// SCREEN DRAWING FUNCTIONS /////
void dispSettings (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(20,8);
    display->println("SETTINGS");
    display->drawRect(0,0,128,32,SSD1306_WHITE);                  
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
    display->println("UNITS");                 
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
    byte nDig = digits(RPM);
    byte center = 47;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(3);
    display->setCursor(center-((nDig*18)/2),6);
    display->println(RPM); 
    display->setTextSize(2);             // text size
    display->setCursor(88,10);
    display->println("RPM");                
    display->display();
}

void dispSpd (Adafruit_SSD1306 *display){
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer

    if (units == 0){    // Metric Units
      float spdDisp = spd*0.01; // spd is km/h*100
      byte nDig = digits(spdDisp);
      byte center = 37;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(spdDisp, 0);
      display->setCursor(center+((nDig*18)/2)+4,10);
      display->setTextSize(2); 
      display->println("km/h");
               
    } 
    else {              // 'Merican units
      float spdDisp = spd * 0.006213711922; //convert km/h*100 to mph
      byte nDig = digits (spdDisp);
      byte center = 47;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(spdDisp, 0);  
      display->setCursor(center+((nDig*18)/2)+4,10);
      display->setTextSize(2);
      display->println("MPH");          
    }
          
    display->display();
}

void dispOilTemp (Adafruit_SSD1306 *display) {
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

void dispFuelPrs (Adafruit_SSD1306 *display) {
    float fuelPrsDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();
    display->setTextSize(2); 
    display->setCursor(0,3);
    display->println("FUEL");
    display->setTextSize(1); 
    display->setCursor(0,21);
    display->println("PRESSURE");

    if (units == 0){    // Metric Units
      fuelPrsDisp = fuelPrs/100; // convert kpa to bar
      if (fuelPrsDisp < 0) {fuelPrsDisp = 0;}
      byte nDig = 3; //nDig always == 3 for metric oil pressure
      byte center = 79;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelPrsDisp, 1);
      display->setCursor(center+((nDig*18)/2)+3,18);
      display->setTextSize(1); 
      display->println("bar");
               
    } 
    else {              // 'Merican units
      fuelPrsDisp = fuelPrs * 0.1450377; //convert kpa to PSI  
      if (fuelPrsDisp < 0) {fuelPrsDisp = 0;}
      byte nDig = digits (fuelPrsDisp);
      byte center = 71;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(fuelPrsDisp, 0);  
      display->setCursor(center+((nDig*18)/2)+2,10);
      display->setTextSize(2);
      display->println("PSI");          
    }
    
    display->display();
}

void dispFuelComp (Adafruit_SSD1306 *display) {
    byte nDig = digits (fuelComp);
    byte center = 79;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setTextSize(2);             // text size
    display->setCursor(2,0);
    display->println("Flex");
    display->setCursor(2,15);
    display->println("Fuel");            
    display->setTextSize(3); 
    display->setCursor(center-((nDig*18)/2),6);
    display->print(fuelComp, 0); 
    display->println("%");        
    display->display();
}

void dispAFR (Adafruit_SSD1306 *display) {
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->setCursor(8,6);
    display->setTextSize(3); 
    display->print(afr, 1);
    display->setCursor(88,10);
    display->setTextSize(2);
    display->println("AFR");         
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
    float oilPrsDisp;
    display->setTextColor(WHITE); 
    display->clearDisplay();             //clear buffer
    display->drawBitmap(0, 0, img_oilPrs, 40, 32, 1);
    if (oilPrs < 0) {oilPrs = 0;}
    
    if (units == 0){    // Metric Units
      oilPrsDisp = oilPrs/100; // convert kpa to bar
      if (oilPrsDisp < 0) {oilPrsDisp = 0;}
      byte nDig = 3; //nDig always == 3 for metric oil pressure
      byte center = 79;
      display->setTextSize(3); // char width = 18
      display->setCursor(center-((nDig*18)/2),6);
      display->print(oilPrsDisp, 1);
      display->setCursor(center+((nDig*18)/2)+3,18);
      display->setTextSize(1); 
      display->println("bar");
               
    } 
    else {              // 'Merican units
      oilPrsDisp = oilPrs * 0.1450377; //convert kpa to PSI  
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
      odoDisp = odo; 
      display->setCursor(100,6);
      display->setTextSize(2);
      display->println("mi");         
    } 
    else {              // 'Merican units
      odoDisp = odo * 0.6213712; //convert km to miles  
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

void dispClock (Adafruit_SSD1306 *display){
    byte hourDisp;
    display->clearDisplay();             //clear buffer
    if (clockOffset + hour > 23) {        // ensure hours don't exceed 23
      hourDisp = clockOffset + hour - 24;
    }
    else {
      hourDisp = clockOffset + hour;
    }

    if (hourDisp < 10) {display->setCursor(32,6);} // Center the numbers
    else {display->setCursor(22,6);}
    
    display->setTextColor(WHITE);
    display->setTextSize(3);             // text size
    display->print(hourDisp); 
    display->print(':');
    if (minute < 10) { display->print('0'); } //keep time format for minutes
    display->println(minute);
    display->display();
}

// This function helps when centering text. Number of display digits are returned
byte digits(float val){
  byte nDigits;
  if (val >= 0){ 
    if (val < 10)         {nDigits = 1;}
    else if (val < 100)   {nDigits = 2;}
    else if (val < 1000)  {nDigits = 3;}
    else if (val < 10000) {nDigits = 4;}
  }
  else {
    if (val > -10)        {nDigits = 2;}
    else if (val > -100)  {nDigits = 3;}
    else if (val > -1000) {nDigits = 4;}
  }
  return nDigits;
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
    // Serial.print("Speed:");
    // float dispVal = var1/16;
    // Serial.println(dispVal);
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
  
    //if (millis() - timerGPSupdate > GPSupdateRate) { 
      //timerGPSupdate = millis();        // reset timer2
            unsigned long alpha_0 = 192; // filter coefficeint to set speedometer response rate
              t_old = t_new;                     // save previous time value
              t_new = millis();                  // record time of GPS update
              v_old = v_new;                     // save previous value of velocity                       
              lagGPS = t_new-t_old;                 // time between updated
            v = GPS.speed*3.6;              // fetch velocity from GPS object, convert to km/h             
            float vFloat = GPS.speed*360;       // x100 to preserve hundredth km/h accuracy
            v_100 = (unsigned long)vFloat;           // convert to unsigned long       
            v_new = (v_100*alpha_0 + v_old*(256-alpha_0))>>8; //filtered velocity value
            hour = GPS.hour;
            minute = GPS.minute;            
      //}
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

//  Speedometer Needle Angle Function  //
int speedometer (int spd_g) {

  spd_g = map(millis()-lagGPS, t_old, t_new, v_old, v_new);               // interpolate values between GPS data fix
 
  
  if (spd_g < 50) spd_g = 0;                                  // if speed is below 0.5 mph set to zero
  if (spd_g > speedoMax) spd_g = speedoMax;                   // set max pointer rotation
  
  int angle = map( spd_g, 0, speedoMax, 0, M1_SWEEP);         // calculate angle of gauge 
  return angle;                                               // return angle of motor
  
}


/////  SHUTDOWN  /////

// save settings, display shutdown screens, and zero out the gauges
void shutdown (void){
  // Write dispArray1 values from into EEPROM address 0-3
  for (int i = dispArray1Address; i < sizeof(dispArray1); i++) {
    EEPROM.update(i, dispArray1[i]);
  }
  
  // Write dispArray2 values from into EEPROM for disp array 2
  EEPROM.update(dispArray2Address, dispArray2[0]);

  // Display 
  dispFalconScript(&display1);
  disp302CID(&display2);

  // Zero the motors
  motor1.setPosition(0);
  motor2.setPosition(0);
  motor3.setPosition(0);
    while (motor1.currentStep > 0 || motor2.currentStep > 0 || motor3.currentStep > 0)
  {
      motor1.update();
      motor2.update();
      motor3.update();
  }

  // delay
  delay(2000);

  // check again for key off
  if (vBatt > 1){
    return;
  }

  // cut power to Dash control unit
    digitalWrite(pwrPin, LOW);
}


void generateRPM(void){
    // // RPM signal generation for demo
    if (rpmSwitch == 0){
      gRPM = gRPM + 120; 
    }
    else if (rpmSwitch == 1) {
      gRPM = gRPM - 160;
    }
    if (gRPM > 7000) rpmSwitch = 1;
    if (gRPM < 900) rpmSwitch = 0;

    // // Fake  signal generation for demo
    // if (analogSwitch == 0){
    //   analog = analog + 20; 
    // }
    // else if (analogSwitch == 1) {
    //   analog = analog - 20;
    // }
    // if (analog > 1022) analogSwitch = 1;
    // if (analog < 1) analogSwitch = 0;
}
