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
// Display libraries
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// Communication libraries
#include <SPI.h>
#include <mcp_can.h>

// Input libraries
#include <Rotary.h>

// Storage library
#include <EEPROM.h>

// LED library
#include <FastLED.h>

// GPS library
#include <Adafruit_GPS.h>

// Stepper motor libraries
#include <SwitecX25.h>
#include <SwitecX12.h>
#include <Stepper.h>

///// PROJECT MODULES /////
#include "config_hardware.h"
#include "config_calibration.h"
#include "globals.h"
#include "gps.h"
#include "can.h"
#include "sensors.h"
#include "display.h"
#include "outputs.h"
#include "menu.h"
#include "utilities.h"
#include "image_data.h"

/*
 * ========================================
 * SETUP FUNCTION
 * ========================================
 */
void setup() {

  // Initialize serial communication for debugging
  Serial.begin(115200);

  // Keep power enabled after ignition switch turns off
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);

  // ===== GPS INITIALIZATION =====
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);
  useInterrupt(true);
 
  // ===== HALL SENSOR INITIALIZATION =====
  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hallSpeedISR, FALLING);

  // ===== ENGINE RPM SENSOR INITIALIZATION =====
  pinMode(IGNITION_PULSE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IGNITION_PULSE_PIN), ignitionPulseISR, FALLING);

  // ===== DISPLAY INITIALIZATION =====
  display1.begin(SSD1306_SWITCHCAPVCC);
  display2.begin(SSD1306_SWITCHCAPVCC);
  dispFalconScript(&display1);
  disp302CID(&display2);
  
  // ===== STEPPER MOTOR INITIALIZATION =====
  pinMode(MOTOR_RST, OUTPUT);
  digitalWrite(MOTOR_RST, HIGH);
  motorSweepSynchronous();
  
  // ===== ODOMETER MOTOR SETUP =====
  pinMode(ODO_PIN1, OUTPUT);
  pinMode(ODO_PIN2, OUTPUT);
  pinMode(ODO_PIN3, OUTPUT);
  pinMode(ODO_PIN4, OUTPUT);

  // ===== LED TACHOMETER INITIALIZATION =====
  if (NUM_LEDS > MAX_LEDS) {
    NUM_LEDS = MAX_LEDS;
    Serial.println("Warning: NUM_LEDS exceeds MAX_LEDS, clamped to maximum");
  }
  if (WARN_LEDS + SHIFT_LEDS >= NUM_LEDS / 2) {
    Serial.println("Warning: WARN_LEDS + SHIFT_LEDS too large for NUM_LEDS, LED zones may overlap");
  }
  FastLED.addLeds<WS2812, TACH_DATA_PIN, GRB>(leds, NUM_LEDS);

  // ===== ROTARY ENCODER SETUP =====
  pinMode(SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), rotate, CHANGE);
  attachInterrupt(digitalPinToInterrupt(3), rotate, CHANGE);
  
  // ===== LOAD SAVED SETTINGS FROM EEPROM =====
  for (int i = 0; i < sizeof(dispArray1); i++) {
    dispArray1[i] = EEPROM.read(dispArray1Address + i);
  }
  dispArray2[0] = EEPROM.read(dispArray2Address);
  clockOffset = EEPROM.read(clockOffsetAddress); 
  EEPROM.get(odoAddress, odo);
  EEPROM.get(odoTripAddress, odoTrip);
  EEPROM.get(fuelSensorRawAddress, fuelSensorRaw);
  EEPROM.get(unitsAddress, units);

  Serial.print("clockOffset: ");
  Serial.println(clockOffset);
  
  // ===== CAN BUS INITIALIZATION =====
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) 
    Serial.println("MCP2515 Initialized Successfully!");
  else 
    Serial.println("Error Initializing MCP2515...");
  
  pinMode(CAN0_INT, INPUT);
  CAN0.setMode(MCP_NORMAL);

  // ===== SPLASH SCREEN DELAY =====
  while (millis() < SPLASH_TIME){
    // Wait for splash screen timer to expire
  }

}

/*
 * ========================================
 * MAIN LOOP FUNCTION
 * ========================================
 */
void loop() {
    
  // ===== ANALOG SENSOR READING =====
  if (millis() - timerSensorRead > SENSOR_READ_RATE) {
    int s = micros();

    vBattRaw = readSensor(VBATT_PIN, vBattRaw, FILTER_VBATT);
    vBatt = (float)vBattRaw * VBATT_SCALER;
    
    fuelSensorRaw = readSensor(FUEL_PIN, fuelSensorRaw, FILTER_FUEL);
    float fuelSensor = (float)fuelSensorRaw*0.01;
    fuelLvl = curveLookup(fuelSensor, fuelLvlTable_x, fuelLvlTable_l, fuelLvlTable_length);
    
    thermSensor = readThermSensor(THERM_PIN, thermSensor, FILTER_THERM);
    therm = curveLookup(thermSensor, thermTable_x, thermTable_l, thermTable_length);
    thermCAN = (int)(therm*10);
    
    sensor_av1 = read30PSIAsensor(PIN_AV1, sensor_av1, FILTER_AV1);
    sensor_av1 = constrain(sensor_av1, 600, 1050);
    baroCAN = sensor_av1;
    
    timerSensorRead = millis();

    int time =  micros() - s;
  }

  // ===== HALL SENSOR READING ======
  if (millis() - timerHallUpdate > HALL_UPDATE_RATE) {
    hallSpeedUpdate();
    timerHallUpdate = millis();
  }

  // ===== ENGINE RPM SENSOR READING =====
  if (millis() - timerEngineRPMUpdate > ENGINE_RPM_UPDATE_RATE) {
    engineRPMUpdate();
    timerEngineRPMUpdate = millis();
  }

  // ===== CAN BUS TRANSMISSION =====
  if (millis() - timerCANsend > CAN_SEND_RATE) {  
    int s = micros();

    sendCAN_BE(0x200, 0, spdCAN, 0, 0);
    sendCAN_LE(0x201, thermCAN, fuelLvlCAN, baroCAN, 555);
    
    timerCANsend = millis();

    int time =  micros() - s;
  }

  // ===== CAN BUS RECEPTION =====
  if(!digitalRead(CAN0_INT)) {
    receiveCAN();
    parseCAN(rxId, 0);
  }

  // ===== GPS DATA PROCESSING =====
  if (millis() - timerCheckGPS > CHECK_GPS_RATE) {
    fetchGPSdata();
    timerCheckGPS = millis();
  }

  // ===== LED TACHOMETER UPDATE =====
  if (millis() - timerTachUpdate > TACH_UPDATE_RATE) {
    ledShiftLight(RPM);
    timerTachUpdate = millis();
  }

  // ===== DISPLAY UPDATE =====
  if (millis() - timerDispUpdate > DISP_UPDATE_RATE) {
    swRead();
    sigSelect();
    dispMenu();
    disp2();
    timerDispUpdate = millis();
  }

  // ===== MOTOR ANGLE UPDATE =====
  if (millis() - timerAngleUpdate > ANGLE_UPDATE_RATE) {
    motor1.setPosition(fuelLvlAngle(M1_SWEEP));
    motor2.setPosition(coolantTempAngle(M2_SWEEP));
    motor3.setPosition(speedometerAngleHall(M3_SWEEP));
    motor4.setPosition(fuelLvlAngle(M4_SWEEP));
    timerAngleUpdate = millis();
  }

  // ===== MOTOR STEP EXECUTION =====
  motor1.update();
  motor2.update();
  motor3.update();
  motor4.update();
  updateOdometerMotor();  // Non-blocking odometer motor update

  // ===== SHUTDOWN DETECTION =====
  // Check if ignition voltage has dropped (key turned off)
  // Shutdown when battery voltage < 1V AND system has been running for at least 3 seconds
  if (vBatt < 1 && millis() > SPLASH_TIME + 3000) {
    shutdown();  // Save settings, zero gauges, display shutdown screen, cut power
  }

}
