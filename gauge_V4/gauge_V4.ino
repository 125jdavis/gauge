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

// Include all modular headers
#include "HardwareConfig.h"
#include "GlobalVariables.h"
#include "ImageData.h"
#include "Utils.h"
#include "SensorFunctions.h"
#include "CANBus.h"
#include "GPSFunctions.h"
#include "MotorControl.h"
#include "LEDControl.h"
#include "MenuSystem.h"
#include "DisplayFunctions.h"

/*
 * ========================================
 * SETUP FUNCTION
 * ========================================
 * 
 * Arduino setup() runs once on power-up or reset
 * Initializes all hardware, loads saved settings, and displays splash screen
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
  // Convert raw data to display units and update screens
  // Update rate: every 75ms (~13Hz) for smooth display without flicker
  if (millis() - timerDispUpdate > dispUpdateRate) { 
    // Serial.print("disp: ");  // Debug timing
    int s = micros();
    
    sigSelect();  // Process and convert sensor/CAN data to display formats
    swRead();     // Read rotary encoder button with debouncing
    dispMenu();   // Update display 1 based on menu selection
    disp2();      // Update display 2 based on selection
    timerDispUpdate = millis();  // Reset timer
    
    int time =  micros() - s;
    // Serial.println(time);  // Debug: print execution time
  }

  // ===== MOTOR ANGLE UPDATE =====
  // Calculate new target positions for gauge needles
  // Update rate: every 20ms (50Hz) for smooth needle movement
  if (millis() - timerAngleUpdate > angleUpdateRate) {
    // Serial.print("angle: ");  // Debug timing
    int s = micros();
    
    // Calculate target angles for each gauge based on sensor/CAN data
    spd_g = speedometerAngle(M3_SWEEP);           // Speedometer (motor 3 - wide sweep)
    fuelLevelPct_g = fuelLvlAngle(M1_SWEEP);      // Fuel level (motor 1)
    coolantTemp_g = coolantTempAngle(M4_SWEEP);   // Coolant temp (motor 4)
    
    // Update motor targets (motors will step toward these positions asynchronously)
    motor1.setPosition(fuelLevelPct_g);    // Fuel gauge
    motor3.setPosition(spd_g);             // Speedometer
    motor4.setPosition(coolantTemp_g);     // Temperature gauge
    
    timerAngleUpdate = millis();  // Reset timer
    
    int time =  micros() - s;
    // Serial.println(time);  // Debug: print execution time
  }

  // ===== MOTOR STEPPING =====
  // Execute stepper motor steps - called every loop for smooth motion
  // SwitecX12 library handles microstepping internally
  motor1.update();  // Step fuel gauge motor if needed
  motor2.update();  // Step motor 2 if needed
  motor3.update();  // Step speedometer motor if needed
  motor4.update();  // Step temperature gauge motor if needed

  // ===== SHUTDOWN CHECK =====
  // Monitor battery voltage to detect key-off condition
  // Saves data and shuts down gracefully when voltage drops
  if (vBatt < 11.0) {  // 11V threshold indicates alternator off / key off
    //shutdown();  // Save odometer, zero gauges, cut power
  }
  
  // serialInputFunc();  // Uncomment for serial testing/debugging
}

/*
 * ========================================
 * END OF CODE
 * ========================================
 * 
 * All functions have been modularized into separate files for easier maintenance.
 * The code is organized by functional area for better readability and collaboration.
 */
