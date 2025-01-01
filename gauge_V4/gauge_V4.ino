// Gauge Control Module 
// Jesse Davis
// 12/30/2024
// STATUS: new hardware acquired december 2024. not tested, mechanical ODO should work now, powered by 12v. 
// Larger voltage regulator to deal with load from LED tachometer. currently attempting to modularize code.
// Would also like to make everything configureable through settings menu and rotary encoder.
// settings will be saved to EEPROM and read out as variables during startup.

///// LIBRARIES /////
#include <Adafruit_SSD1306.h> //https://github.com/adafruit/Adafruit_SSD1306
#include <Adafruit_GFX.h>     //https://github.com/adafruit/Adafruit-GFX-Library
#include <SPI.h>          // included in arduino IDE
#include <mcp_can.h>      //https://downloads.arduino.cc/libraries/github.com/coryjfowler/mcp_can-1.5.1.zip
#include <Rotary.h>       //https://github.com/brianlow/Rotary/blob/master/Rotary.cpp
#include <EEPROM.h>       // included in arduino IDE
#include <FastLED.h>      //https://github.com/FastLED/FastLED
#include <Adafruit_GPS.h> //https://github.com/adafruit/Adafruit_GPS
#include <SwitecX25.h>    //https://github.com/clearwater/SwitecX25
#include <SwitecX12.h>    //https://github.com/clearwater/SwitecX25
#include <TimerOne.h>     // not certain this is needed
#include <Stepper.h>      // included in arduino IDE

#include "sensors.h"
#include "display.h"
#include "can_bus.h"
#include "images.h"
#include "utils.h"



///// DEFINE /////
//#define OLED_RESET 4  // OLED display reset pin
#define CAN0_CS 53  // CAN Bus Chip Select pin
#define CAN0_INT 18  // CAN Bus Interrupt pin


// GAUGE SETUP //
#define pwrPin 49           // pin to keep power alive after key is off 
#define speedoMax (100*100)     // maximum mph x100

#define MOTOR_RST 36          // motor driver reset pin

#define M1_SWEEP (58*12)     // range of motion for gauge motor 1 standard X25.168 range 315 degrees at 1/3 degree steps
#define M1_STEP  37         // motor 1 step command
#define M1_DIR   38         // motor 1 direction command

#define M2_SWEEP (58*12)    // range of motion for gauge motor 2
#define M2_STEP  34         // motor 2 step command
#define M2_DIR   35         // motor 2 direction command

#define M3_SWEEP (118*12)    // range of motion for gauge motor 3
#define M3_STEP  33         // motor 3 step command
#define M3_DIR   32         // motor 3 direction command

#define M4_SWEEP (58*12)    // range of motion for gauge motor 4
#define M4_STEP  40         // motor 4 step command
#define M4_DIR   41         // motor 4 direction command

//#define ODO_STEPS 32        // number of steps in one ODO revolution

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
#define odoSteps 32
#define odoPin1 10
#define odoPin2 11
#define odoPin3 12
#define odoPin4 13          // initialize odometer motor
Stepper odoMotor(odoSteps, odoPin1, odoPin2, odoPin3, odoPin4); 
Adafruit_GPS GPS(&Serial2);   // set serial2 to GPS object

///// GLOBAL VARIABLES /////

// Analog inputs to Dash Control Module
// vBatt, on pin 0
float vBatt = 12;
int vBattRaw = 12;
int filter_vBatt = 8; // out of 64, 64 = no filter
int vBattPin = A0;
float vBattScaler = 0.040923; // voltage divider factor (in this case 4/100: r1 = 10k, r2 = 3.3k, 100 is the multiplier from ADC)

// fuel, on pin 3
int fuelSensorRaw;
int filter_fuel = 1; // out of 64, 64 = no filter
int fuelPin = A3;

// therm, on pin 4
float therm;
float thermSensor;
int filter_therm = 50; // out of 100, 100 = no filter
int thermPin = A4;
int thermCAN;

// sensor a (baro), on pin 5
unsigned long baro;
byte filter_baro = 4; // out of 16, 16 = no filter
int baroPin = A5;

// sensor b, on pin 6
float sensor_b;
byte filter_b = 12;
int analogPin6 = A6;

// sensor c, on pin 7
float sensor_c;
byte filter_c = 12;
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
float odoTrip;
float distLast;
byte hour;
byte minute;

// Stepper Motor Variables
unsigned int spd_g;
unsigned int fuelLevelPct_g;
unsigned int coolantTemp_g;

// Rotary Encoder Variables
bool stateSW = 1;
bool lastStateSW = 1;
unsigned int lastStateChangeTime = 0;  // the last time the output pin was toggled
unsigned int debounceDelay = 50;       // the debounce time; increase if the output flickers
bool debounceFlag = 0;
bool button = 0;

// timers and refresh rates
unsigned int timer0, timerDispUpdate, timerCANsend;
unsigned int timerSensorRead, timerTachUpdate, timerTachFlash;
unsigned int timerCheckGPS, timerGPSupdate, timerAngleUpdate;

//long unsigned dispMenuRate = 20;
unsigned int CANsendRate = 50;
unsigned int dispUpdateRate = 75;
unsigned int sensorReadRate = 10;
unsigned int tachUpdateRate = 50;
unsigned int tachFlashRate = 50;
unsigned int GPSupdateRate = 100; // might not be needed
unsigned int checkGPSRate = 1;
unsigned int angleUpdateRate = 20;
unsigned int splashTime = 1500; //how long should the splash screens show?

// LED Tach variables
unsigned int tachMax = 6000;
unsigned int tachMin = 3000;
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
int baroCAN;
int spdCAN;
int pumpPressureCAN;

//Engine Parameters for Display
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
float fuelLvlTable_x[fuelLvlTable_length] = {0.87, 1.03, 1.21, 1.40, 1.60, 1.97, 2.21, 2.25, 2.30};
float fuelLvlTable_l[fuelLvlTable_length] = {  16,   14,   12,   10,    8,    6,    4,    2,    0};
float fuelCapacity = 16;

// EEPROM Variables
byte dispArray1Address = 0;   // starting EEPEOM address for display 1, length is 4 bytes
byte dispArray2Address = 4;   // EEPROM Address for display 2, length is 1 byte
byte clockOffsetAddress = 5;  // EEPROM Address for clock offset, length is 1 byte
byte odoAddress = 6;          // EEPROM address for odometer, length is 4 bytes
byte odoTripAddress = 10;     // EEPROM address for odometer, length is 4 bytes
byte fuelSensorRawAddress = 14;
byte unitsAddress = 18;
int *input;               //this is a memory address
int output = 0;

// Menu Navigation Variables
byte menuLevel = 0;
byte units = 0;  // 0 = metric, 1 = 'Merican
unsigned int nMenuLevel = 15; //This should be the number of menu items on the given level
byte dispArray1[4] = { 1, 0, 0, 0 };  //should be written to EEPROM 0-3
byte clockOffset = 0;
byte dispArray2[1] = {1};


// DEMO VARIABLES
bool rpmSwitch = 0;
int gRPM;
int analog = 2;
int analogSwitch = 0;

// Signal selection  //
void sigSelect (void) {
    spd = v_new; //km/h * 100
    //spdMph = spd *0.6213712;
    spdCAN = (int)(v*16);
    RPM = rpmCAN;
    coolantTemp = (coolantTempCAN/10)-273.15; // convert kelvin to C;
    oilPrs = (oilPrsCAN/10)-101.3;   //kPa, convert to gauge pressure
    fuelPrs = (fuelPrsCAN/10)-101.3;  //kPa, convert to gauge pressure
    oilTemp = therm;
    afr = (float)afr1CAN/1000;
    fuelComp = fuelCompCAN/10;
    fuelLvlCAN = (int)((fuelLvl/fuelCapacity)*100);

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
 
  // Initialize displays
  display1.begin(SSD1306_SWITCHCAPVCC);  // initialize with SPI
  display2.begin(SSD1306_SWITCHCAPVCC);  // initialize with SPI
  dispFalconScript(&display1);
  disp302CID(&display2);
  
  // Initialize Stepper Motors
  pinMode(MOTOR_RST, OUTPUT);
  digitalWrite(MOTOR_RST, HIGH);
  
  // Sweep gauges through range of motion
  motorSweepSynchronous();
  
  // Setup the Odometer
  pinMode(odoPin1, OUTPUT);
  pinMode(odoPin2, OUTPUT);
  pinMode(odoPin3, OUTPUT);
  pinMode(odoPin4, OUTPUT);

  // Initialize LED Tach
  FastLED.addLeds<WS2812, TACH_DATA_PIN, GRB>(leds, NUM_LEDS);

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

  //fetch odometer values from EEPROM
  EEPROM.get(odoAddress, odo);
  EEPROM.get(odoTripAddress, odoTrip);
  EEPROM.get(fuelSensorRawAddress, fuelSensorRaw); 
  EEPROM.get(unitsAddress, units); 

  Serial.print("clockOffset: ");
  Serial.println(clockOffset);
  
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
    // Serial.print("sensorRead: ");
    int s = micros();

    vBattRaw = readSensor(vBattPin, vBattRaw, filter_vBatt);
    vBatt = (float)vBattRaw*vBattScaler;
    
    fuelSensorRaw = readSensor(fuelPin,fuelSensorRaw,filter_fuel);
    float fuelSensor = (float)fuelSensorRaw*0.01;
    fuelLvl = curveLookup(fuelSensor, fuelLvlTable_x, fuelLvlTable_l, fuelLvlTable_length);
    
    thermSensor = readThermSensor(thermPin, thermSensor, filter_therm);
    therm = curveLookup(thermSensor, thermTable_x, thermTable_l, thermTable_length);
    thermCAN = (int)(therm*10);
    
    baro = read30PSIAsensor(baroPin,baro,filter_baro); //baro x 10 
    baro = constrain(baro, 600, 1050);  // limit baro pressure to elevations between -300m to 4000m
    baroCAN = baro; 
  //sensor_b = readSensor(analogPin6,sensor_b,filter_b); 
  //sensor_c = readSensor(analogPin7,sensor_c,filter_c); 
    timerSensorRead = millis();

    int time =  micros() - s;
    // Serial.println(time);

  }


  //Send CAN messages at specified rate
  if (millis() - timerCANsend > CANsendRate) {  
        // Serial.print("CANsend: ");
    int s = micros();

    sendCAN_BE(0x200, 0, spdCAN, 0, 0);
    sendCAN_LE(0x201, thermCAN, fuelLvlCAN, baroCAN, 555);
    //sendCAN_LE(0x201, 255, 50, 988, 555);
    //sendCAN_BE(0x301, 333, 444, 1010, 2020);
    timerCANsend = millis();

    int time =  micros() - s;
    // Serial.println(time);

  }


  //Read CAN messages as they come in
  if(!digitalRead(CAN0_INT)){     // If CAN0_INT pin is low, read receive buffer
    // Serial.print("CAN recieve: ");
    int s = micros();
    receiveCAN ();
    parseCAN( rxId, rxBuf);

    int time =  micros() - s;
    // Serial.println(time);
  }

  //Check for new GPS and process if new data is present
  if (millis() - timerCheckGPS > checkGPSRate) {
    // Serial.print("GPS recieve: ");
    int s = micros(); 
    
    fetchGPSdata();
    int time =  micros() - s;
    // Serial.println(time);
  }
  
  //Tach update timer
  if (millis() - timerTachUpdate > tachUpdateRate) {     
    // Serial.print("tach: ");
    int s = micros();
    
    // demoRPM = generateRPM();    
    ledShiftLight(RPM);
    timerTachUpdate = millis();        // reset timer1 
    int time =  micros() - s;
    // Serial.println(time);
  }

  sigSelect();
  //OLED Displays
  swRead();
  if(millis() - timerDispUpdate > dispUpdateRate){
    // Serial.print("display: ");
    int s = micros();
    
    dispMenu();
    disp2();
    //Serial.println(button);
    timerDispUpdate = millis();

    int time =  micros() - s;
    // Serial.println(time);
  }

  if(millis() - timerAngleUpdate > angleUpdateRate){
    // Serial.print("motors: ");
    int s = micros();
    
    int angle1 = fuelLvlAngle(M1_SWEEP);
    int angle2 = fuelLvlAngle(M2_SWEEP);
    int angle3 = speedometerAngle(M3_SWEEP);
    int angle4 = coolantTempAngle(M4_SWEEP);
    motor1.setPosition(angle1);
    motor2.setPosition(angle2);
    motor3.setPosition(angle3);
    motor4.setPosition(angle4);
  }
  
  motor1.update();
  motor2.update();
  motor3.update();
  motor4.update();

  // Check for key off, if switched voltage supply is below 1v, turn off control module 
  if (vBatt < 1 && millis() > splashTime + 3000) {
    shutdown();
    
  }

  //serialInputFunc();  // Debugging only

}





///// ALL FUNCTIONS /////////////////////////////////////////////////////////////////

////// SENSOR READING FUNCTIONS ///// 

///// DISPLAY AND NAVIGATION FUNCTIONS /////


///// CAN BUS FUNCTIONS /////




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

///// GPS FUNCTIONS /////



////// STEPPER MOTORS /////

//  Speedometer Needle Angle Function  //
int speedometerAngle(int sweep) {
  unsigned long t_curr =  millis()-lagGPS;
  float spd_g_float = map(t_curr, t_old, t_new, v_old, v_new)*0.6213712;   // interpolate values between GPS data fix, convert from km/h x100 to mph x100
  spd_g = (unsigned long)spd_g_float;
  if (spd_g < 50) spd_g = 0;                                  // if speed is below 0.5 mph set to zero
  if (spd_g > speedoMax) spd_g = speedoMax;                   // set max pointer rotation
  
  int angle = map( spd_g, 0, speedoMax, 1, sweep-1);         // calculate angle of gauge 
  Serial.print(millis());
  Serial.print(",");
  Serial.print(v);
  Serial.print(",");
  Serial.println(angle);
  angle = constrain(angle, 1, sweep-1);
  return angle;                                               // return angle of motor
}

int fuelLvlAngle(int sweep) {
  float fuelLvlPct = (fuelLvl/fuelCapacity)*1000;
  fuelLevelPct_g = (unsigned int)fuelLvlPct;
  int angle = map(fuelLevelPct_g, 100, 1000, 1, sweep-1);
  angle = constrain(angle, 1, sweep-1);
  return angle;
} 

int coolantTempAngle(int sweep) {
  int angle;
  if (coolantTemp < 95){
    angle = map((long)coolantTemp, 60, 98, 1, sweep/2);
  }
  else {
    angle = map((long)coolantTemp, 98, 115, sweep/2, sweep-1);
  }
  angle = constrain(angle, 1, sweep-1);
  return angle;
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
  EEPROM.update(unitsAddress, units);
  EEPROM.put(odoAddress, odo);
  EEPROM.put(odoTripAddress, odoTrip);
  EEPROM.put(fuelSensorRawAddress, fuelSensorRaw);


  // Display 
  dispFalconScript(&display1);
  disp302CID(&display2);

  // Zero the motors
  motorZeroSynchronous();

  // delay
  delay(2000);

  // check again for key off
  if (vBatt > 1){
    return;
  }

  // cut power to Dash control unit
    digitalWrite(pwrPin, LOW);
}

void motorZeroSynchronous(void){
  motor1.currentStep = M1_SWEEP;
  motor2.currentStep = M2_SWEEP;
  motor3.currentStep = M3_SWEEP;
  motor4.currentStep = M4_SWEEP;
  motor1.setPosition(0);
  motor2.setPosition(0);
  motor3.setPosition(0);
  motor4.setPosition(0);
    while (motor1.currentStep > 0 || motor2.currentStep > 0 || motor3.currentStep > 0 || motor4.currentStep > 0)
  {
      motor1.update();
      motor2.update();
      motor3.update();
      motor4.update();
  }
  motor1.currentStep = 0;
  motor2.currentStep = 0;
  motor3.currentStep = 0;
  motor4.currentStep = 0;
}


void motorSweepSynchronous(void){
  motorZeroSynchronous();
  Serial.println("zeroed");
  motor1.setPosition(M1_SWEEP);
  motor2.setPosition(M2_SWEEP);
  motor3.setPosition(M3_SWEEP);
  motor4.setPosition(M4_SWEEP);
    while (motor1.currentStep < M1_SWEEP-1  || motor2.currentStep < M2_SWEEP-1 || motor3.currentStep < M3_SWEEP-1 || motor4.currentStep < M4_SWEEP-1)
  {
      motor1.update();
      motor2.update();
      motor3.update();
      motor4.update();
  }

  Serial.println("full sweep");
  motor1.setPosition(0);
  motor2.setPosition(0);
  motor3.setPosition(0);
  motor4.setPosition(0);
    while (motor1.currentStep > 0 || motor2.currentStep > 0 || motor3.currentStep > 0 || motor4.currentStep > 0)
  {
      motor1.update();
      motor2.update();
      motor3.update();
      motor4.update();
  }
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

void serialInputFunc(void){
  // SERIAL INPUT FOR TESTING ONLY
  if (Serial.available() > 0) {
    // Read the incoming data as a string
    String inputSer = Serial.readStringUntil('\n');
    
    // Convert the input string to an integer
    int newValue = inputSer.toInt();
    
    // Update the variable with the new value
    //coolantTempCAN = (newValue+273.15)*10 ;
    //fuelLvl = newValue;
    
    
    // Print the new value of the variable
    Serial.println("Updated value of fuel level: " + String(fuelLvl));
    Serial.println("Please enter a new value:");
  }
}
