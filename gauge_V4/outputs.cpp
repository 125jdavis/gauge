/*
 * ========================================
 * OUTPUT CONTROL FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "outputs.h"
#include "globals.h"

// ===== CONVERSION CONSTANTS =====
const float KM_TO_MILES = 0.621371;  // Conversion factor: kilometers to miles

// ===== ODOMETER MOTOR STATE =====
// Non-blocking stepper motor control for mechanical odometer
static float odoMotorTargetSteps = 0.0;   // Accumulated target position (includes fractional steps)
static unsigned long odoMotorCurrentStep = 0;  // Current motor position in whole steps
static unsigned long lastOdoStepTime = 0;  // Time of last step (microseconds)

// ===== MOTOR S SMOOTHING STATE =====
// Position interpolation for smooth speedometer needle motion
// The SwitecX12 library can move very fast (300 steps/sec max velocity), which means
// it can reach a new target position in just a few milliseconds. Since setPosition()
// is called at ~50Hz (target: every 20ms, but actual: 21-40ms with occasional spikes),
// without interpolation the needle would move quickly to the target then stop and wait,
// causing jerky motion.
//
// Solution: Instead of jumping directly to the final target angle, we interpolate
// the position over multiple update cycles. This ensures the motor arrives at the
// target position "just in time" for the next angle update, creating smooth motion.
// The interpolation adapts to variable update rates by using the actual time between
// target updates rather than assuming a fixed 20ms interval.
static int motorS_previousTarget = 0;     // Previous target angle (steps)
static int motorS_finalTarget = 0;        // Final target angle from speedometerAngleS()
static unsigned long motorS_lastUpdateTime = 0;  // Time of last angle update (millis)
static unsigned long motorS_updateInterval = ANGLE_UPDATE_RATE;  // Actual time between last two updates (millis)

// ===== MOTORS 1-4 SMOOTHING STATE =====
// Position interpolation for smooth gauge needle motion (fuel, coolant temp, etc.)
// Uses the same adaptive linear interpolation approach as motorS smoothing.
// All 4 motors share one timestamp and interval since they are updated together.
static int motor1to4_previousTarget[4] = {0, 0, 0, 0};  // Previous target angles (steps)
static int motor1to4_finalTarget[4]    = {0, 0, 0, 0};  // Final target angles from angle functions
static unsigned long motor1to4_lastUpdateTime = 0;       // Time of last target update (millis)
static unsigned long motor1to4_updateInterval = ANGLE_UPDATE_RATE; // Measured interval between updates (millis)

// ===== ODOMETER MOTOR STATE =====
// 20BYJ-48 stepper motor timing and control
// The 20BYJ-48 is a 5V 4-phase unipolar stepper motor with internal gearing
// - Wave drive sequence: 4 states per electrical cycle (one coil at a time)
// - Wave drive is equivalent to full-step mode for step counting purposes
// - With internal ~64:1 gearing: 2048 steps per output shaft revolution (full-step/wave-drive)
//   (4096 steps/rev is the half-step specification — NOT used here)
// - Maximum speed: ~15 RPM (limited by internal gearing and torque)
// - Target speed: < 3 RPM for odometer application
//
// Step delay calculation for target speed:
// - At 3 RPM: 3 rev/min * 2048 steps/rev = 6,144 steps/min = 102.4 steps/sec
// - Delay per step: 1,000,000 us / 102.4 = 9,766 us (~10ms)
// - Using 5000 us (5ms) gives ~200 steps/sec = 5.86 RPM (well within stall limit)
static const unsigned long ODO_STEP_DELAY_US = 5000;  // 5ms between steps ≈ 5.86 RPM (well within stall limit)

// Wave-drive stepper sequence for 20BYJ-48 (one phase at a time)
// Wave drive: A -> B -> C -> D -> A ...
// Phase:      0    1    2    3
static const uint8_t ODO_STEP_SEQUENCE[4][4] = {
  {HIGH, LOW,  LOW,  LOW},   // Step 0: Phase A (coil 1)
  {LOW,  HIGH, LOW,  LOW},   // Step 1: Phase B (coil 2)
  {LOW,  LOW,  HIGH, LOW},   // Step 2: Phase C (coil 3)
  {LOW,  LOW,  LOW,  HIGH}   // Step 3: Phase D (coil 4)
};
static uint8_t odoMotorStepIndex = 0;  // Current position in step sequence (0-3)

void ledShiftLight(int ledRPM){
  static bool tachFlashState = 0;  // Current state of shift light flashing (0=off, 1=on) - local static
  
  if (ledRPM < TACH_MIN) {
      // black out unused range  
    for (int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB::Black;
    }
  } else {
    // ===== PAIR-BASED TACH RENDERING =====
    // Works correctly for any NUM_LEDS (even or odd).
    // Each LED at index p (from the left) is paired with its mirror at index
    // NUM_LEDS-1-p (from the right).  For odd strip counts there is one
    // unpaired center LED at index halfLeds which is always in the shift zone.
    //
    // Zone assignment uses distFromCenter (0 = innermost pair):
    //   distFromCenter <= SHIFT_LEDS               → shift  (red)
    //   SHIFT_LEDS < distFromCenter <= WARN_LEDS   → warning (orange)
    //   distFromCenter > WARN_LEDS                 → normal  (amber)
    //
    // This produces the same LED layout as the original algorithm for even N
    // and a perfectly symmetric layout for odd N.

    int halfLeds = NUM_LEDS / 2;  // Number of symmetric pairs
    int blackoutPairs = map(ledRPM, TACH_MIN, TACH_MAX, halfLeds, 0);

    // 1. Assign zone colors to every paired LED
    for (int p = 0; p < halfLeds; p++){
      int leftIdx  = p;
      int rightIdx = NUM_LEDS - 1 - p;
      int distFromCenter = halfLeds - 1 - p;  // 0 = innermost, halfLeds-1 = outermost

      CRGB color;
      if      (distFromCenter <= SHIFT_LEDS) color = CRGB( 80,  0, 0);  // shift  (red)
      else if (distFromCenter <= WARN_LEDS)  color = CRGB( 80, 10, 0);  // warning (orange)
      else                                   color = CRGB( 30, 15, 0);  // normal  (amber)

      leds[leftIdx]  = color;
      leds[rightIdx] = color;
    }

    // 2. Center LED for odd strip counts is always in the shift zone
    if (NUM_LEDS % 2 == 1){
      leds[halfLeds] = CRGB(80, 0, 0);
    }

    // 3. Black out innermost pairs to reflect current RPM
    //    Pair p=0 in this loop targets the INNERMOST pair; as blackoutPairs
    //    decreases (RPM rises), outer pairs are progressively revealed first.
    for (int p = 0; p < blackoutPairs; p++){
      leds[halfLeds - 1 - p]       = CRGB::Black;  // innermost left  → outward
      leds[NUM_LEDS - halfLeds + p] = CRGB::Black;  // innermost right → outward
    }

    // 4. Center LED for odd N: black out whenever any pairs are blacked out
    if ((NUM_LEDS % 2 == 1) && blackoutPairs > 0){
      leds[halfLeds] = CRGB::Black;
    }

    // 5. Flash shift zone pairs when shift point is exceeded
    if (RPM > TACH_MAX){
      if (millis() - timerTachFlash > TACH_FLASH_RATE){
        if (tachFlashState == 0){
          // Black out the SHIFT_LEDS+1 innermost pairs (the shift zone)
          for (int p = 0; p <= SHIFT_LEDS; p++){
            leds[halfLeds - 1 - p]       = CRGB::Black;
            leds[NUM_LEDS - halfLeds + p] = CRGB::Black;
          }
          if (NUM_LEDS % 2 == 1){
            leds[halfLeds] = CRGB::Black;  // center LED for odd N
          }
        }

        timerTachFlash = millis();
        tachFlashState = 1 - tachFlashState;
      }
    }
  }

  // ===== FAULT INDICATOR LED =====
  // Override leds[0] with a flashing fault color when a confirmed (debounced) fault is active.
  // This takes priority over the normal RPM display for the first LED.
  // Colors: oil pressure = orange, coolant temp = blue, battery voltage = green, fuel level = purple.
  // Multiple active faults: cycle through each fault color on successive flash-on periods.
  // Flash timing follows the same faultFlashState toggle used by the OLED fault flash.
  {
    // Collect active fault colors in priority order (use debounced globals set by main loop)
    CRGB activeFaultColors[4];
    uint8_t numFaults = 0;
    if (oilFaultActive)     activeFaultColors[numFaults++] = CRGB(255,  60,   0);  // Orange - oil pressure
    if (coolantFaultActive) activeFaultColors[numFaults++] = CRGB(  0,   0, 255);  // Blue   - coolant temp
    if (battFaultActive)    activeFaultColors[numFaults++] = CRGB(  0, 200,   0);  // Green  - battery voltage
    if (fuelFaultActive)    activeFaultColors[numFaults++] = CRGB(180,   0, 200);  // Purple - fuel level

    // Static state: persists across calls, reset when no faults are active
    static uint8_t faultLedColorIdx = 0;
    static bool prevFaultFlashState = false;

    if (numFaults > 0) {
      // Advance color index on each rising edge of faultFlashState (flash-on transition)
      if (faultFlashState && !prevFaultFlashState) {
        faultLedColorIdx++;
      }
      prevFaultFlashState = faultFlashState;

      // Flash: show fault color on flash-on period, off on flash-off period
      leds[0] = faultFlashState ? activeFaultColors[faultLedColorIdx % numFaults] : CRGB::Black;
    } else {
      // No active faults: reset state so next fault cycle starts fresh
      faultLedColorIdx = 0;
      prevFaultFlashState = false;
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
 * speedometerAngleS - Calculate speedometer needle angle for motorS (integer math)
 * 
 * Converts vehicle speed to motor angle using integer math for efficiency.
 * Uses the generic 'spd' variable which is selected based on SPEED_SOURCE.
 * 
 * @param sweep - Maximum motor steps for full gauge sweep
 * @return Motor angle in steps (1 to sweep-1)
 * 
 * Speed format: km/h * 100 (e.g., 5000 = 50 km/h)
 * Output: mph * 100 (e.g., 3107 = 31.07 mph)
 * 
 * Conversion: 1 km/h = 0.621371 mph
 * Using integer math: (spd * 62137) / 100000 ≈ spd * 0.621371
 */
int speedometerAngleS(int sweep) {
  // Convert km/h*100 to mph*100 using integer math
  // spd is in km/h * 100, multiply by 62137 then divide by 100000
  // This gives mph * 100
  // Bounds check to prevent overflow (spd max is typically ~65535, safe for this calculation)
  // Use local copy to avoid modifying the global spd variable
  int local_spd = spd;
  if (local_spd > 30000) {  // 300 km/h * 100, well above typical max speed
    local_spd = 30000;
  }
  long spd_mph_long = ((long)local_spd * 62137L) / 100000L;
  int spd_mph = (int)spd_mph_long;
  
  // Dead zone: below 0.5 mph, show zero
  if (spd_mph < 50) spd_mph = 0;
  
  // Clamp to max (100 mph * 100 = 10000)
  if (spd_mph > SPEEDO_MAX) spd_mph = SPEEDO_MAX;
  
  // Map speed to motor angle using integer math
  // Standard map formula: output = (input * (out_max - out_min)) / (in_max - in_min) + out_min
  // For speedometer: angle = (spd_mph * (sweep - 1 - 1)) / SPEEDO_MAX + 1
  // The (sweep - 2) accounts for valid range being 1 to (sweep-1), giving (sweep-2) steps
  int angle = ((long)spd_mph * (long)(sweep - 2)) / (long)SPEEDO_MAX + 1;
  
  // Ensure angle is within valid range
  angle = constrain(angle, 1, sweep - 1);
  return angle;
}

/**
 * updateMotorSTarget - Update the final target angle for motorS (called at ~50Hz)
 * 
 * This function updates the target angle that motorS should reach.
 * It's called nominally at ANGLE_UPDATE_RATE (50Hz, every 20ms) from the main loop,
 * but actual timing varies (typically 21-40ms, with occasional spikes up to ~100ms).
 * The actual motor position is interpolated by updateMotorSSmoothing().
 * 
 * @param sweep - Maximum motor steps for full gauge sweep
 */
void updateMotorSTarget(int sweep) {
  // Get the new target angle from speed calculation
  int newTarget = speedometerAngleS(sweep);
  
  unsigned long currentTime = millis();
  
  // Calculate actual interval since last update for adaptive interpolation
  // This handles variable update rates (21-40ms typical, with occasional spikes)
  if (motorS_lastUpdateTime > 0 && currentTime >= motorS_lastUpdateTime) {
    motorS_updateInterval = currentTime - motorS_lastUpdateTime;
    // Sanity check: limit to reasonable range (5ms to 500ms)
    // Prevents issues with extreme spikes or millis() overflow edge cases
    if (motorS_updateInterval < 5) motorS_updateInterval = 5;
    if (motorS_updateInterval > 500) motorS_updateInterval = 500;
  } else {
    // First call or overflow: use nominal rate
    motorS_updateInterval = ANGLE_UPDATE_RATE;
  }
  
  // Update the final target and timing
  motorS_previousTarget = motorS_finalTarget;
  motorS_finalTarget = newTarget;
  motorS_lastUpdateTime = currentTime;
}

/**
 * updateMotorSSmoothing - Interpolate and set motorS position for smooth motion
 * 
 * This function should be called frequently (much more than 50Hz, ideally from
 * high frequency main loop) to smoothly interpolate the speedometer needle 
 * position between target updates.
 * 
 * Design rationale:
 * - motorS.update() is called at 10kHz (very fast) via hardware interrupt
 * - Target angle updates happen at ~50Hz (variable: 21-40ms typical, occasional spikes up to ~100ms)
 * - SwitecX12 library can reach target in ~5ms at max velocity (300 steps/sec)
 * - Without smoothing: motor moves fast → stops → waits → moves fast (jerky!)
 * - With smoothing: motor moves continuously at controlled pace (smooth!)
 * 
 * Implementation:
 * - Calculate time elapsed since last target update
 * - Interpolate between previous and final target based on elapsed time
 * - Use actual measured update interval (not fixed 20ms) for robustness to variable rates
 * - Set intermediate position to create smooth motion
 * - Motor naturally tracks this slowly-moving target
 * 
 * This approach prioritizes motion smoothness over absolute positional accuracy.
 */
void updateMotorSSmoothing(void) {
  unsigned long currentTime = millis();
  
  // Handle first call or millis() overflow (occurs every ~50 days)
  // When overflow occurs, currentTime will be less than motorS_lastUpdateTime
  if (motorS_lastUpdateTime == 0 || currentTime < motorS_lastUpdateTime) {
    // Initialize/reset both targets to current position to maintain continuity
    // This prevents needle jump on startup or millis() overflow
    motorS_previousTarget = motorS.currentStep;
    motorS_finalTarget = motorS.currentStep;
    motorS_updateInterval = ANGLE_UPDATE_RATE;
    motorS.setPosition(motorS_finalTarget);
    motorS_lastUpdateTime = currentTime;
    return;
  }
  
  unsigned long elapsed = currentTime - motorS_lastUpdateTime;
  
  // Calculate interpolated position based on time elapsed since last target update
  // Linear interpolation: pos = previous + (final - previous) * (elapsed / interval)
  // This creates smooth motion that arrives at the target just as the next update occurs
  int positionDelta = motorS_finalTarget - motorS_previousTarget;
  
  // Use the actual measured update interval for interpolation (handles variable rates)
  // Clamp elapsed to updateInterval to prevent overshoot when we reach the target
  if (elapsed > motorS_updateInterval) {
    elapsed = motorS_updateInterval;
  }
  
  // Use integer math to avoid floating point
  // interpolatedPos = previous + (delta * elapsed) / updateInterval
  // Note: positionDelta can be negative (moving backward), which is fine
  // long (32-bit signed) can handle values up to ±2 billion, so no overflow risk
  // Max calculation: positionDelta (-8000 to +8000) * elapsed (0-500) = ±4,000,000
  long interpolation = ((long)positionDelta * (long)elapsed) / (long)motorS_updateInterval;
  int interpolatedPosition = motorS_previousTarget + (int)interpolation;
  
  // Constrain to valid range (1 to MS_SWEEP-1)
  interpolatedPosition = constrain(interpolatedPosition, 1, MS_SWEEP - 1);
  
  // Set the interpolated position - motor will smoothly track this moving target
  motorS.setPosition(interpolatedPosition);
}

/**
 * updateMotors1to4Target - Record new target angles for motors 1-4 and measure update interval
 *
 * Called at ANGLE_UPDATE_RATE from the main loop.  Saves the previous final targets,
 * sets the new final targets, and measures the actual elapsed interval since the last
 * call.  The measured interval is used by updateMotors1to4Smoothing() to spread motor
 * movement evenly, eliminating the jerky "move fast → stop → wait" behaviour.
 *
 * @param t1 New target angle for motor 1 (steps, 1 to M1_SWEEP-1)
 * @param t2 New target angle for motor 2
 * @param t3 New target angle for motor 3
 * @param t4 New target angle for motor 4
 */
void updateMotors1to4Target(int t1, int t2, int t3, int t4) {
  unsigned long currentTime = millis();

  // Measure actual interval since last update for adaptive interpolation
  if (motor1to4_lastUpdateTime > 0 && currentTime >= motor1to4_lastUpdateTime) {
    motor1to4_updateInterval = currentTime - motor1to4_lastUpdateTime;
    if (motor1to4_updateInterval < 5)   motor1to4_updateInterval = 5;
    if (motor1to4_updateInterval > 500) motor1to4_updateInterval = 500;
  } else {
    motor1to4_updateInterval = ANGLE_UPDATE_RATE;
  }

  // Shift final → previous, then store new finals
  motor1to4_previousTarget[0] = motor1to4_finalTarget[0];
  motor1to4_previousTarget[1] = motor1to4_finalTarget[1];
  motor1to4_previousTarget[2] = motor1to4_finalTarget[2];
  motor1to4_previousTarget[3] = motor1to4_finalTarget[3];
  motor1to4_finalTarget[0] = t1;
  motor1to4_finalTarget[1] = t2;
  motor1to4_finalTarget[2] = t3;
  motor1to4_finalTarget[3] = t4;
  motor1to4_lastUpdateTime = currentTime;
}

/**
 * updateMotors1to4Smoothing - Interpolate and set positions for motors 1-4
 *
 * Called every main loop iteration (>1 kHz) to smoothly interpolate each motor's
 * position between the previous and final targets over the measured update interval.
 * This is the same adaptive linear interpolation used by updateMotorSSmoothing().
 *
 * All arithmetic is integer-only to minimise overhead.
 */
void updateMotors1to4Smoothing(void) {
  unsigned long currentTime = millis();

  // Handle first call or millis() overflow
  if (motor1to4_lastUpdateTime == 0 || currentTime < motor1to4_lastUpdateTime) {
    motor1to4_previousTarget[0] = motor1.currentStep;
    motor1to4_previousTarget[1] = motor2.currentStep;
    motor1to4_previousTarget[2] = motor3.currentStep;
    motor1to4_previousTarget[3] = motor4.currentStep;
    motor1to4_finalTarget[0] = motor1.currentStep;
    motor1to4_finalTarget[1] = motor2.currentStep;
    motor1to4_finalTarget[2] = motor3.currentStep;
    motor1to4_finalTarget[3] = motor4.currentStep;
    motor1to4_updateInterval = ANGLE_UPDATE_RATE;
    motor1.setPosition(motor1to4_finalTarget[0]);
    motor2.setPosition(motor1to4_finalTarget[1]);
    motor3.setPosition(motor1to4_finalTarget[2]);
    motor4.setPosition(motor1to4_finalTarget[3]);
    motor1to4_lastUpdateTime = currentTime;
    return;
  }

  unsigned long elapsed = currentTime - motor1to4_lastUpdateTime;
  if (elapsed > motor1to4_updateInterval) {
    elapsed = motor1to4_updateInterval;
  }

  // Motor 1
  {
    int delta = motor1to4_finalTarget[0] - motor1to4_previousTarget[0];
    long interp = ((long)delta * (long)elapsed) / (long)motor1to4_updateInterval;
    motor1.setPosition(constrain(motor1to4_previousTarget[0] + (int)interp, 1, M1_SWEEP - 1));
  }
  // Motor 2
  {
    int delta = motor1to4_finalTarget[1] - motor1to4_previousTarget[1];
    long interp = ((long)delta * (long)elapsed) / (long)motor1to4_updateInterval;
    motor2.setPosition(constrain(motor1to4_previousTarget[1] + (int)interp, 1, M2_SWEEP - 1));
  }
  // Motor 3
  {
    int delta = motor1to4_finalTarget[2] - motor1to4_previousTarget[2];
    long interp = ((long)delta * (long)elapsed) / (long)motor1to4_updateInterval;
    motor3.setPosition(constrain(motor1to4_previousTarget[2] + (int)interp, 1, M3_SWEEP - 1));
  }
  // Motor 4
  {
    int delta = motor1to4_finalTarget[3] - motor1to4_previousTarget[3];
    long interp = ((long)delta * (long)elapsed) / (long)motor1to4_updateInterval;
    motor4.setPosition(constrain(motor1to4_previousTarget[3] + (int)interp, 1, M4_SWEEP - 1));
  }
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
  motorS.currentStep = MS_SWEEP;
  
  // Command all motors to position 0
  motor1.setPosition(0);
  motor2.setPosition(0);
  motor3.setPosition(0);
  motor4.setPosition(0);
  motorS.setPosition(0);
  
  // Wait for all motors to reach zero (blocking loop)
  while (motor1.currentStep > 0 || motor2.currentStep > 0 || motor3.currentStep > 0 || motor4.currentStep > 0 || motorS.currentStep > 0)
  {
      motor1.update();  // Step motor 1 if needed
      motor2.update();  // Step motor 2 if needed
      motor3.update();
      motor4.update();
      motorS.update();
  }
  // Reset position counters to zero
  motor1.currentStep = 0;
  motor2.currentStep = 0;
  motor3.currentStep = 0;
  motor4.currentStep = 0;
  motorS.currentStep = 0;
}

/**
 * sweepDelay - Calculate per-motor update delay for a timed sweep
 *
 * Returns the number of microseconds between update() calls so that a motor
 * with `sweep` steps completes its full range in MOTOR_SWEEP_TIME_MS ms.
 * All motors share the same MOTOR_SWEEP_TIME_MS but get individually-scaled
 * delays so they all finish at exactly the same moment.
 *
 * @param sweep - Total steps in the motor's range
 * @return Delay in microseconds (minimum 10 µs)
 */
static unsigned long sweepDelay(uint16_t sweep) {
  const unsigned long MIN_DELAY_US = 10;
  if (sweep == 0 || MOTOR_SWEEP_TIME_MS == 0) return MIN_DELAY_US;
  unsigned long d = (unsigned long)MOTOR_SWEEP_TIME_MS * 1000UL / sweep;
  return (d < MIN_DELAY_US) ? MIN_DELAY_US : d;
}

/**
 * motorZeroTimed - Return all motors to zero with synchronized timed stepping
 *
 * Identical to motorZeroSynchronous but uses per-motor delays so that every
 * motor completes its full return sweep in MOTOR_SWEEP_TIME_MS milliseconds
 * simultaneously.
 *
 * The Timer3 ISR is disabled for the duration so that the 10 kHz interrupt
 * cannot override the per-motor pacing.  The ISR is re-enabled on exit.
 * Safe to call during shutdown (where the ISR is active).
 */
void motorZeroTimed(void) {
  // Disable Timer3 ISR — without this, the 10 kHz interrupt drives all motors at
  // maximum library speed, completely overriding the per-motor delay calculation.
  TIMSK3 &= ~(1 << OCIE3A);

  // Tell the library each motor is at its maximum so it steps all the way back to zero.
  motor1.currentStep = M1_SWEEP;
  motor2.currentStep = M2_SWEEP;
  motor3.currentStep = M3_SWEEP;
  motor4.currentStep = M4_SWEEP;
  motorS.currentStep = MS_SWEEP;

  motor1.setPosition(0);
  motor2.setPosition(0);
  motor3.setPosition(0);
  motor4.setPosition(0);
  motorS.setPosition(0);

  // Per-motor delays: each motor steps at a rate that fills exactly MOTOR_SWEEP_TIME_MS
  unsigned long d[5] = {
    sweepDelay(M1_SWEEP),
    sweepDelay(M2_SWEEP),
    sweepDelay(M3_SWEEP),
    sweepDelay(M4_SWEEP),
    sweepDelay(MS_SWEEP)
  };

  unsigned long t0 = micros();
  unsigned long last[5] = {t0, t0, t0, t0, t0};

  while (motor1.currentStep > 0 || motor2.currentStep > 0 ||
         motor3.currentStep > 0 || motor4.currentStep > 0 || motorS.currentStep > 0) {
    unsigned long now = micros();
    if (now - last[0] >= d[0]) { motor1.update(); last[0] = now; }
    if (now - last[1] >= d[1]) { motor2.update(); last[1] = now; }
    if (now - last[2] >= d[2]) { motor3.update(); last[2] = now; }
    if (now - last[3] >= d[3]) { motor4.update(); last[3] = now; }
    if (now - last[4] >= d[4]) { motorS.update(); last[4] = now; }
    yield();
  }

  motor1.currentStep = 0;
  motor2.currentStep = 0;
  motor3.currentStep = 0;
  motor4.currentStep = 0;
  motorS.currentStep = 0;

  // Re-enable Timer3 ISR
  TIMSK3 |= (1 << OCIE3A);
}

/**
 * motorSweepSynchronous - Perform timed startup sweep test of all motors
 * 
 * Sweeps all motors from zero to maximum and back to zero with configurable timing.
 * Used during startup to verify motor operation and provide visual feedback.
 * 
 * Timing:
 * - Each direction (0→max, max→0) takes MOTOR_SWEEP_TIME_MS milliseconds
 * - Default: 1000ms per direction (2000ms total for full sweep test)
 * - Configurable via MOTOR_SWEEP_TIME_MS in config_calibration.cpp
 * 
 * Implementation:
 * - Each motor gets an independent per-motor delay calibrated to its sweep range:
 *   delay = MOTOR_SWEEP_TIME_MS * 1000 / M?_SWEEP  (microseconds between update() calls)
 * - All motors therefore complete their sweep in the same MOTOR_SWEEP_TIME_MS window
 * - Uses micros() for precise per-motor timing control
 * 
 * Note: Called before initMotorUpdateTimer() so the Timer3 ISR is not yet active;
 *       no ISR management is needed here.
 */
void motorSweepSynchronous(void){
  // Start by zeroing all motors (fast, establish known position)
  motorZeroSynchronous();
  Serial.println("zeroed");

  // Per-motor delays: each motor steps at a rate that fills exactly MOTOR_SWEEP_TIME_MS
  unsigned long d[5] = {
    sweepDelay(M1_SWEEP),
    sweepDelay(M2_SWEEP),
    sweepDelay(M3_SWEEP),
    sweepDelay(M4_SWEEP),
    sweepDelay(MS_SWEEP)
  };

  // ── Sweep up: command all motors to maximum position ──────────────────────
  motor1.setPosition(M1_SWEEP);
  motor2.setPosition(M2_SWEEP);
  motor3.setPosition(M3_SWEEP);
  motor4.setPosition(M4_SWEEP);
  motorS.setPosition(MS_SWEEP);

  unsigned long t0 = micros();
  unsigned long last[5] = {t0, t0, t0, t0, t0};

  while (motor1.currentStep < M1_SWEEP - 1 || motor2.currentStep < M2_SWEEP - 1 ||
         motor3.currentStep < M3_SWEEP - 1 || motor4.currentStep < M4_SWEEP - 1 ||
         motorS.currentStep < MS_SWEEP - 1) {
    unsigned long now = micros();
    if (now - last[0] >= d[0]) { motor1.update(); last[0] = now; }
    if (now - last[1] >= d[1]) { motor2.update(); last[1] = now; }
    if (now - last[2] >= d[2]) { motor3.update(); last[2] = now; }
    if (now - last[3] >= d[3]) { motor4.update(); last[3] = now; }
    if (now - last[4] >= d[4]) { motorS.update(); last[4] = now; }
    yield();
  }

  Serial.println("full sweep");

  // ── Sweep down: command all motors back to zero ───────────────────────────
  motor1.setPosition(0);
  motor2.setPosition(0);
  motor3.setPosition(0);
  motor4.setPosition(0);
  motorS.setPosition(0);

  t0 = micros();
  unsigned long lastReturn[5] = {t0, t0, t0, t0, t0};

  while (motor1.currentStep > 0 || motor2.currentStep > 0 ||
         motor3.currentStep > 0 || motor4.currentStep > 0 ||
         motorS.currentStep > 0) {
    unsigned long now = micros();
    if (now - lastReturn[0] >= d[0]) { motor1.update(); lastReturn[0] = now; }
    if (now - lastReturn[1] >= d[1]) { motor2.update(); lastReturn[1] = now; }
    if (now - lastReturn[2] >= d[2]) { motor3.update(); lastReturn[2] = now; }
    if (now - lastReturn[3] >= d[3]) { motor4.update(); lastReturn[3] = now; }
    if (now - lastReturn[4] >= d[4]) { motorS.update(); lastReturn[4] = now; }
    yield();
  }
}

/**
 * moveOdometerMotor - Queue distance for mechanical odometer motor
 * 
 * Calculates the number of steps required to advance the mechanical odometer
 * based on distance traveled and adds them to the target position. The motor
 * will be moved non-blocking via updateOdometerMotor() calls from main loop.
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
 * 5. Add to target position (accumulates fractional steps for precision)
 * 
 * Example with default calibration values (ODO_STEPS=2048, ODO_MOTOR_TEETH=16, ODO_GEAR_TEETH=20):
 * - For 1.60934 km (1 mile):
 *   - Distance in miles = 1.0
 *   - Odometer revs = 1.0
 *   - Gear ratio = 20/16 = 1.25
 *   - Motor revs = 1.0 * 1.25 = 1.25
 *   - Steps = 1.25 * 2048 = 2560 steps
 */
void moveOdometerMotor(float distanceKm) {
    // Convert distance from kilometers to miles
    float distanceMiles = distanceKm * KM_TO_MILES;
    
    // Calculate odometer revolutions (1 revolution = 1 mile per specification)
    // and apply gear ratio to get motor revolutions
    // Gear ratio: how many motor revs needed per odometer revolution
    float gearRatio = (float)ODO_GEAR_TEETH / (float)ODO_MOTOR_TEETH;
    float motorRevs = distanceMiles * gearRatio;
    
    // Calculate steps required (steps = motor revolutions * steps per revolution)
    // Keep as float to accumulate fractional steps for better precision
    float steps = motorRevs * ODO_STEPS;
    
    // Add to target position (non-blocking - actual movement happens in updateOdometerMotor)
    if (steps > 0) {
        odoMotorTargetSteps += steps;
    }
}

/**
 * updateOdometerMotor - Non-blocking motor update for mechanical odometer
 * 
 * Moves the odometer motor one step at a time toward the target position.
 * This function should be called frequently from the ISR to ensure
 * smooth, non-blocking motor operation.
 * 
 * The function enforces a delay between steps to keep motor speed below 3 RPM
 * and prevent the motor from losing steps or stalling.
 * 
 * Implementation:
 * - Uses direct pin control for 4-phase stepper motor (20BYJ-48)
 * - Wave drive mode (one phase at a time) for lower power and heat
 * - 5ms delay between steps = ~2.93 RPM (safely below 3 RPM limit)
 * - Non-blocking: only advances if enough time has passed
 */
void updateOdometerMotor(void) {
    // Check if there are steps to move
    // Round target to ensure fractional parts >= 0.5 trigger the next step
    unsigned long targetStep = (unsigned long)(odoMotorTargetSteps + 0.5);
    
    if (odoMotorCurrentStep < targetStep) {
        // Check if enough time has passed since last step
        unsigned long currentTime = micros();
        
        // Initialize timer on first run to avoid immediate step
        if (lastOdoStepTime == 0) {
            lastOdoStepTime = currentTime;
            return;
        }
        
        if (currentTime - lastOdoStepTime >= ODO_STEP_DELAY_US) {
            // Advance to next step in sequence
            odoMotorStepIndex = (odoMotorStepIndex + 3) % 4; //(odoMotorStepIndex + x) FWD: x=1 REV: x=3 
            
            // Apply step sequence to motor pins
            digitalWrite(ODO_PIN1, ODO_STEP_SEQUENCE[odoMotorStepIndex][0]);
            digitalWrite(ODO_PIN2, ODO_STEP_SEQUENCE[odoMotorStepIndex][1]);
            digitalWrite(ODO_PIN3, ODO_STEP_SEQUENCE[odoMotorStepIndex][2]);
            digitalWrite(ODO_PIN4, ODO_STEP_SEQUENCE[odoMotorStepIndex][3]);
            
            odoMotorCurrentStep++;
            lastOdoStepTime = currentTime;
        }
    }
}
