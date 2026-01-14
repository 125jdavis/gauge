/*
 * ========================================
 * OUTPUT CONTROL FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "outputs.h"
#include "globals.h"

void ledShiftLight(int ledRPM){
  static bool tachFlashState = 0;  // Current state of shift light flashing (0=off, 1=on) - local static
  
  if (ledRPM < TACH_MIN) {
      // black out unused range  
    for (int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB::Black;
    }
    return;
  }
  int midPoint = NUM_LEDS/2;
  int blackout_val = map(ledRPM, TACH_MIN, TACH_MAX, midPoint, 0);
 
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
    if (RPM > TACH_MAX ){
      if (millis() - timerTachFlash > TACH_FLASH_RATE){
        
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
int speedometerAngle(int sweep) {
  unsigned long t_curr =  millis()-lagGPS;  // Current time minus GPS lag
  // Interpolate speed between last two GPS readings for smooth motion
  float spd_g_float = map(t_curr, t_old, t_new, v_old, spdGPS)*0.6213712;   // Convert km/h*100 to mph*100
  spd_g = (unsigned long)spd_g_float;
  
  if (spd_g < 50) spd_g = 0;         // Dead zone: below 0.5 mph, show zero
  if (spd_g > SPEEDO_MAX) spd_g = SPEEDO_MAX;  // Clamp to max (100 mph * 100 = 10000)
  
  int angle = map( spd_g, 0, SPEEDO_MAX, 1, sweep-1);  // Map speed to motor angle
  
  
  angle = constrain(angle, 1, sweep-1);  // Ensure angle is within valid range
  return angle;
}
int speedometerAngleGPS(int sweep) {
  unsigned long t_curr =  millis()-lagGPS;
  float spd_g_float = map(t_curr, t_old, t_new, v_old, spdGPS)*0.6213712;   // interpolate values between GPS data fix, convert from km/h x100 to mph x100
  spd_g = (unsigned long)spd_g_float;
  if (spd_g < 50) spd_g = 0;                                  // if speed is below 0.5 mph set to zero
  if (spd_g > SPEEDO_MAX) spd_g = SPEEDO_MAX;                   // set max pointer rotation
  
  int angle = map( spd_g, 0, SPEEDO_MAX, 1, sweep-1);         // calculate angle of gauge 
  angle = constrain(angle, 1, sweep-1);
  return angle;                                               // return angle of motor
}
int speedometerAngleCAN(int sweep) {
  int angle = map( spdCAN, 0, SPEEDO_MAX, 1, sweep-1);         // calculate angle of gauge 
  angle = constrain(angle, 1, sweep-1);
  return angle;
}
int speedometerAngleHall(int sweep) {
  int angle = map( spdHall, 0, SPEEDO_MAX, 1, sweep-1);         // calculate angle of gauge 
  angle = constrain(angle, 1, sweep-1);
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

/**
 * moveOdometerMotor - Move mechanical odometer motor by calculated distance
 * 
 * Calculates the number of steps required to advance the mechanical odometer
 * based on distance traveled, motor characteristics, and gear ratios.
 * 
 * Per specification: One rotation of the mechanical odometer = 1 mile
 * 
 * @param distanceKm - Distance to advance the odometer in kilometers
 * 
 * Calculation:
 * 1. Convert distance from kilometers to miles (0.621371 miles per km)
 * 2. Calculate odometer revolutions needed (1 revolution = 1 mile)
 * 3. Apply gear ratio: motor_revs = (ODO_GEAR_TEETH / ODO_MOTOR_TEETH) * odo_revs
 * 4. Calculate motor steps: steps = motor_revs * ODO_STEPS
 * 5. Command motor to move the calculated steps
 * 
 * Example with default calibration values (ODO_STEPS=4096, ODO_MOTOR_TEETH=16, ODO_GEAR_TEETH=20):
 * - For 1.60934 km (1 mile):
 *   - Distance in miles = 1.0
 *   - Odometer revs = 1.0
 *   - Gear ratio = 20/16 = 1.25
 *   - Motor revs = 1.0 * 1.25 = 1.25
 *   - Steps = 1.25 * 4096 = 5120 steps
 */
void moveOdometerMotor(float distanceKm) {
    // Convert distance from kilometers to miles (1 mile = 1.60934 km)
    float distanceMiles = distanceKm * 0.621371;
    
    // Calculate odometer revolutions (1 revolution = 1 mile per specification)
    float odoRevs = distanceMiles;
    
    // Apply gear ratio to get motor revolutions
    // Gear ratio: how many motor revs needed per odometer revolution
    float gearRatio = (float)ODO_GEAR_TEETH / (float)ODO_MOTOR_TEETH;
    float motorRevs = odoRevs * gearRatio;
    
    // Calculate steps required (steps = motor revolutions * steps per revolution)
    int steps = (int)(motorRevs * ODO_STEPS);
    
    // Move the motor if there are steps to command
    if (steps > 0) {
        odoMotor.step(steps);
    }
}
