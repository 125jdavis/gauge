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
 * TIMER-BASED MOTOR UPDATE ISR
 * ========================================
 * 
 * This ISR is called at MOTOR_UPDATE_FREQ_HZ (default 10 kHz) by Timer3
 * to provide deterministic, smooth motor stepping independent of main loop timing.
 * 
 * Design rationale:
 * - Main loop has variable execution time due to display updates, CAN, GPS parsing
 * - update() calls from main loop result in irregular step timing → jerky motion
 * - Timer-driven updates provide consistent intervals → smooth motion
 * - 10 kHz frequency ensures steps at max motor speed don't accumulate delays
 * 
 * ISR performance:
 * - Executes in ~10-20 µs with 5 motors (measured on Mega 2560)
 * - At 10 kHz: ~10-20% CPU overhead (10-20 µs per 100 µs period)
 * - Kept minimal: only calls update() on each motor, no complex logic
 * 
 * Motors updated:
 * - motor1, motor2, motor3, motor4 (SwitecX12 gauge motors)
 * - motorS (SwitecX12 speedometer motor)
 * - updateOdometerMotor() (mechanical odometer, custom non-blocking implementation)
 */
ISR(TIMER3_COMPA_vect) {
  // Update all gauge motors (SwitecX12)
  // These motors have internal acceleration/deceleration logic
  // and track their own timing via micros()
  motor1.update();
  motor2.update();
  motor3.update();
  motor4.update();
  motorS.update();
  
  // Update mechanical odometer motor (custom non-blocking implementation)
  updateOdometerMotor();
}

/**
 * initMotorUpdateTimer - Initialize Timer3 for motor update ISR
 * 
 * Configures Timer3 to generate periodic interrupts at MOTOR_UPDATE_FREQ_HZ
 * for deterministic motor stepping.
 * 
 * Timer3 configuration:
 * - Mode: CTC (Clear Timer on Compare Match) - resets counter at OCR3A
 * - Prescaler: 8 (provides good resolution for target frequency range)
 * - Compare value: Calculated from F_CPU and target frequency
 * 
 * Calculation:
 * - Timer frequency = F_CPU / prescaler = 16 MHz / 8 = 2 MHz
 * - Timer ticks per interrupt = timer_freq / target_freq
 * - For 10 kHz target: 2,000,000 / 10,000 = 200 ticks
 * - OCR3A = 200 - 1 = 199 (compare triggers at 199, giving 200 ticks per cycle)
 * 
 * CPU overhead:
 * - ISR executes ~10-20 µs
 * - At 10 kHz: 100-200 µs per millisecond = 10-20% worst case
 * - Typical is lower due to early exits in update()
 */
void initMotorUpdateTimer() {
  // Disable interrupts while configuring timer
  cli();
  
  // Timer3 configuration for CTC mode
  TCCR3A = 0;  // Clear control register A
  TCCR3B = 0;  // Clear control register B
  TCNT3 = 0;   // Initialize counter to 0
  
  // Calculate compare match value
  // Formula: OCR3A = (F_CPU / (prescaler * target_frequency)) - 1
  // With prescaler = 8:
  //   Timer frequency = 16 MHz / 8 = 2 MHz
  //   For 10 kHz: OCR3A = (2,000,000 / 10,000) - 1 = 199
  const uint16_t prescaler = 8;
  const uint32_t timer_freq = F_CPU / prescaler;  // 2 MHz with prescaler=8
  const uint16_t compare_value = (timer_freq / MOTOR_UPDATE_FREQ_HZ) - 1;
  
  OCR3A = compare_value;  // Set compare match register
  
  // Configure Timer3 for CTC mode with prescaler = 8
  // CTC mode: WGM32:0 = 0b0100 (CTC mode, TOP = OCR3A)
  // CS32:0 = 0b010 (prescaler = 8)
  TCCR3B |= (1 << WGM32) | (1 << CS31);
  
  // Enable Timer3 Compare A interrupt
  TIMSK3 |= (1 << OCIE3A);
  
  // Re-enable interrupts
  sei();
  
  // Debug output
  Serial.print(F("Motor update timer initialized: "));
  Serial.print(MOTOR_UPDATE_FREQ_HZ);
  Serial.print(F(" Hz (OCR3A="));
  Serial.print(compare_value);
  Serial.println(F(")"));
}

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
  
  // Configure hardware filters to reduce MCU load
  configureCANFilters();
  Serial.print("CAN filters configured for protocol: ");
  Serial.println(CAN_PROTOCOL);
  
  pinMode(CAN0_INT, INPUT);
  CAN0.setMode(MCP_NORMAL);

  // ===== MOTOR UPDATE TIMER INITIALIZATION =====
  // Initialize Timer3 for deterministic motor stepping at MOTOR_UPDATE_FREQ_HZ
  // This must be done after motor initialization but before main loop starts
  initMotorUpdateTimer();

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

    swRead();
    
    timerSensorRead = millis();
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

  // ===== CAN BUS RECEPTION =====
  if(!digitalRead(CAN0_INT)) {
    receiveCAN();
    parseCAN(rxId, 0);
  }

  // ===== OBDII POLLING =====
  // Poll ECU for parameters when using OBDII protocol
  if (CAN_PROTOCOL == CAN_PROTOCOL_OBDII) {
    pollOBDII();
  }

  // ===== GPS DATA PROCESSING =====
  if (millis() - timerCheckGPS > CHECK_GPS_RATE) {
    fetchGPSdata();
    timerCheckGPS = millis();
  }

  // ===== SIGNAL SELECTION UPDATE =====
  // Process sensor readings and synthetic signal generators
  // Runs at 100Hz for responsive synthetic signals
  if (millis() - timerSigSelectUpdate > SIG_SELECT_UPDATE_RATE) {
    sigSelect();
    timerSigSelectUpdate = millis();
  }

  // ===== MOTOR ANGLE UPDATE =====
  // Set target positions for motors based on sensor readings
  // The actual stepping is handled by Timer3 ISR at MOTOR_UPDATE_FREQ_HZ
  // This decouples position updates (slow, ~50 Hz) from stepping (fast, 10 kHz)
  // CRITICAL: This block is positioned BEFORE display updates to ensure consistent timing.
  // Display updates are blocking operations (~10-20ms) that would otherwise delay
  // angle updates, causing visible jitter/ticks in motor motion.
  if (millis() - timerAngleUpdate > ANGLE_UPDATE_RATE) {
    motor1.setPosition(fuelLvlAngle(M1_SWEEP));
    motor2.setPosition(coolantTempAngle(M2_SWEEP));
    motor3.setPosition(fuelLvlAngle(M3_SWEEP));  // Motor 3 now same config as motor1
    motor4.setPosition(fuelLvlAngle(M4_SWEEP));
    // Motor S uses smoothing: update target at 50Hz, interpolation happens below
    updateMotorSTarget(MS_SWEEP);
    timerAngleUpdate = millis();
  }
  
  // ===== MOTOR S POSITION SMOOTHING =====
  // Continuously interpolate motorS position between target updates for smooth motion
  // Called every loop iteration (typically >1kHz) to provide frequent position updates
  // that the Timer3 ISR can act upon. This creates smooth needle motion instead of
  // the jerky "move-stop-wait" behavior that occurs without interpolation.
  updateMotorSSmoothing();

  // ===== MOTOR STEP EXECUTION =====
  // Motor updates (stepping) are now handled by Timer3 ISR for deterministic timing
  // The ISR calls:
  //   - motor1/2/3/4/S.update() for gauge motors
  //   - updateOdometerMotor() for mechanical odometer
  // 
  // This provides smooth motion independent of main loop timing variations
  // caused by display updates, GPS parsing, CAN processing, etc.
  //
  // Note: Do not call update() here - would conflict with ISR and cause race conditions

  // ===== LED TACHOMETER UPDATE =====
  if (millis() - timerTachUpdate > TACH_UPDATE_RATE) {
    ledShiftLight(RPM);
    timerTachUpdate = millis();
  }

  // ===== DISPLAY UPDATE =====
  // Positioned AFTER motor angle updates to prevent blocking OLED operations
  // from delaying time-critical motor position updates
  if (millis() - timerDispUpdate > DISP_UPDATE_RATE) {
    dispMenu();
    disp2();
    timerDispUpdate = millis();
  }

  // ===== CAN BUS TRANSMISSION =====
  if (millis() - timerCANsend > CAN_SEND_RATE) {  
    //sendCAN_BE(0x200, 0, spdCAN, 0, 0);
    timerCANsend = millis();
  }


  // ===== SHUTDOWN DETECTION =====
  // Check if ignition voltage has dropped (key turned off)
  // Shutdown when battery voltage < 1V AND system has been running for at least 3 seconds
  if (vBatt < 1 && millis() > SPLASH_TIME + 3000) {
    shutdown();  // Save settings, zero gauges, display shutdown screen, cut power
  }


}
