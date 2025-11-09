/*
 * ========================================
 * MOTOR CONTROL FUNCTIONS IMPLEMENTATION
 * ========================================
 * 
 * This file contains the implementation of stepper motor control functions
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#include "MotorControl.h"
#include "HardwareConfig.h"
#include "GlobalVariables.h"
#include <EEPROM.h>

int speedometerAngle(int sweep) {
  unsigned long t_curr =  millis()-lagGPS;  // Current time minus GPS lag
  // Interpolate speed between last two GPS readings for smooth motion
  float spd_g_float = map(t_curr, t_old, t_new, v_old, v_new)*0.6213712;   // Convert km/h*100 to mph*100
  spd_g = (unsigned long)spd_g_float;
  
  if (spd_g < 50) spd_g = 0;         // Dead zone: below 0.5 mph, show zero
  if (spd_g > speedoMax) spd_g = speedoMax;  // Clamp to max (100 mph * 100 = 10000)
  
  int angle = map( spd_g, 0, speedoMax, 1, sweep-1);  // Map speed to motor angle
  
  // Debug output for speed logging
  Serial.print(millis());
  Serial.print(",");
  Serial.print(v);
  Serial.print(",");
  Serial.println(angle);
  
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