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
#include <Adafruit_SSD1306.h>
#include <SwitecX25.h>    //https://github.com/clearwater/SwitecX25
#include <SwitecX12.h>    //https://github.com/clearwater/SwitecX25
#include <TimerOne.h>     // not certain this is needed
#include <Stepper.h>      // included in arduino IDE

#include "sensors.h"
#include "gauges.h"
#include "display.h"
#include "can_bus.h"
#include "images.h"
#include "utils.h"

///// DEFINE /////
//#define OLED_RESET 4  // OLED display reset pin



///// INITIALIZE /////
MCP_CAN CAN0(CAN0_CS);     // Set CS to pin 53

Rotary rotary = Rotary(2, 3);  // rotary encoder ipnput pins (2 and 3 are interrupts)
CRGB leds[NUM_LEDS];

SwitecX12 motor1(M1_SWEEP, M1_STEP, M1_DIR); // initialize motor 1 as Speedometer
SwitecX12 motor2(M2_SWEEP, M2_STEP, M2_DIR); // initialize motor 2 Coolant temp
SwitecX12 motor3(M3_SWEEP, M3_STEP, M3_DIR); // initialize motor 3 fuel level
SwitecX12 motor4(M4_SWEEP, M4_STEP, M4_DIR); // initialize motor 4

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


///// GPS FUNCTIONS /////



////// STEPPER MOTORS /////


/////  SHUTDOWN  /////

// save settings, display shutdown screens, and zero out the gauges


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
