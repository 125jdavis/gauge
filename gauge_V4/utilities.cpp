/*
 * ========================================
 * UTILITY FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "utilities.h"
#include "globals.h"
#include "display.h"
#include "outputs.h"
#include <EEPROM.h>

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
  digitalWrite(PWR_PIN, LOW);  // This will power off the entire system
}
void generateRPM(void){
    static bool rpmSwitch = 0;      // Direction flag for demo RPM sweep - local static
    static int gRPM = 900;          // Generated RPM value for demo mode - local static
    static int analog = 2;          // Test analog value - local static
    static int analogSwitch = 0;    // Direction flag for analog test sweep - local static
    
    // RPM signal generation for demo/testing
    if (rpmSwitch == 0){
      gRPM = gRPM + 120;  // Ramp up by 120 RPM per update
    }
    else if (rpmSwitch == 1) {
      gRPM = gRPM - 160;  // Ramp down by 160 RPM per update
    }
    
    // Reverse direction at limits
    if (gRPM > 7000) rpmSwitch = 1;  // Start ramping down at 7000 RPM
    if (gRPM < 900) rpmSwitch = 0;   // Start ramping up at 900 RPM

    // Optional analog signal generation for testing (currently disabled)
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
    // Read the incoming data as a string (until newline)
    String inputSer = Serial.readStringUntil('\n');
    
    // Convert the input string to an integer
    int newValue = inputSer.toInt();
    
    // Update the test variable with the new value
    // Uncomment the line for the parameter you want to test:
    //coolantTempCAN = (newValue+273.15)*10 ;  // For temperature testing (convert C to Kelvin*10)
    //fuelLvl = newValue;  // For fuel level testing (gallons or liters)
    
    // Print confirmation of new value
    Serial.println("Updated value of fuel level: " + String(fuelLvl));
    Serial.println("Please enter a new value:");
  }
}

/**
 * generateSyntheticSpeed - Generate realistic synthetic speed signal for debugging
 * 
 * Creates a realistic speed profile with random accelerations, decelerations,
 * and constant speed periods. Respects maximum acceleration limits.
 * 
 * Speed range: 0-160 km/h
 * Max acceleration: 20 m/s² = 72 km/h/s = 7200 (km/h*100)/s
 * 
 * State machine:
 * - ACCEL: Accelerating toward target speed
 * - DECEL: Decelerating toward target speed (or zero)
 * - HOLD: Holding constant speed
 * - INTERRUPT: Brief interruption (random accel/decel/hold)
 * 
 * Returns: Speed in km/h * 100 format (e.g., 5000 = 50 km/h)
 */
int generateSyntheticSpeed(void) {
    // State machine states
    enum SyntheticSpeedState {
        ACCEL,      // Accelerating toward target
        DECEL,      // Decelerating toward target or zero
        HOLD,       // Holding constant speed
        INTERRUPT   // Brief interruption period
    };
    
    // Static variables persist between function calls
    static SyntheticSpeedState state = HOLD;
    static int currentSpeed = 0;              // Current speed in km/h * 100
    static int targetSpeed = 0;               // Target speed in km/h * 100
    static int accelRate = 0;                 // Current acceleration rate in (km/h*100)/s
    static unsigned long stateStartTime = 0;  // When current state started (ms)
    static unsigned long stateDuration = 0;   // How long to stay in current state (ms)
    static unsigned long lastUpdateTime = 0;  // Last update timestamp for delta calculation
    
    // Constants
    const int MAX_SPEED = 16000;              // 160 km/h * 100
    const int MAX_ACCEL = 7200;               // 20 m/s² = 72 km/h/s = 7200 (km/h*100)/s
    const int MIN_ACCEL = 1000;               // Minimum acceleration: 10 km/h/s = 1000 (km/h*100)/s
    
    // Initialize on first call
    if (lastUpdateTime == 0) {
        lastUpdateTime = millis();
        stateStartTime = millis();
        state = HOLD;
        currentSpeed = 0;
        targetSpeed = 0;
        stateDuration = random(2000, 5000);  // Hold at zero for 2-5 seconds
        return 0;
    }
    
    // Calculate time delta
    unsigned long currentTime = millis();
    unsigned long deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;
    
    // Skip if called too quickly (< 10ms)
    if (deltaTime < 10) {
        return currentSpeed;
    }
    
    // Update speed based on current acceleration rate
    // deltaSpeed = accelRate * (deltaTime / 1000.0)
    // Using integer math: deltaSpeed = (accelRate * deltaTime) / 1000
    long deltaSpeed = ((long)accelRate * (long)deltaTime) / 1000L;
    currentSpeed += (int)deltaSpeed;
    
    // Clamp speed to valid range
    if (currentSpeed < 0) currentSpeed = 0;
    if (currentSpeed > MAX_SPEED) currentSpeed = MAX_SPEED;
    
    // Check if it's time to transition states
    unsigned long timeInState = currentTime - stateStartTime;
    
    if (timeInState >= stateDuration) {
        // State duration expired - transition to new state
        
        // Randomly decide if we should have an interruption (20% chance)
        bool shouldInterrupt = (random(100) < 20) && (state != INTERRUPT);
        
        if (shouldInterrupt && currentSpeed > 500) {
            // Enter interrupt state (brief random behavior)
            state = INTERRUPT;
            stateStartTime = currentTime;
            stateDuration = random(1000, 3000);  // 1-3 second interruption
            
            // Random interrupt behavior
            int interruptType = random(3);
            if (interruptType == 0) {
                // Brief acceleration
                accelRate = random(MIN_ACCEL, MAX_ACCEL);
            } else if (interruptType == 1) {
                // Brief deceleration
                accelRate = -random(MIN_ACCEL, MAX_ACCEL);
            } else {
                // Brief hold
                accelRate = 0;
            }
        } else {
            // Normal state transition
            switch (state) {
                case HOLD:
                case INTERRUPT:
                    // Transition from hold/interrupt to accel or decel
                    if (currentSpeed < 500) {
                        // At or near zero - start accelerating
                        state = ACCEL;
                        targetSpeed = random(3000, MAX_SPEED);  // Random target: 30-160 km/h
                        accelRate = random(MIN_ACCEL, MAX_ACCEL);
                        stateDuration = random(3000, 8000);  // 3-8 seconds
                    } else {
                        // Decide whether to accelerate or decelerate
                        if (random(2) == 0) {
                            // Start decelerating (possibly to zero)
                            state = DECEL;
                            if (random(3) == 0) {
                                targetSpeed = 0;  // Decel to stop
                            } else {
                                targetSpeed = random(0, currentSpeed);  // Decel to lower speed
                            }
                            accelRate = -random(MIN_ACCEL, MAX_ACCEL);
                            stateDuration = random(3000, 8000);  // 3-8 seconds
                        } else {
                            // Start accelerating
                            state = ACCEL;
                            targetSpeed = random(currentSpeed, MAX_SPEED);  // Accel to higher speed
                            accelRate = random(MIN_ACCEL, MAX_ACCEL);
                            stateDuration = random(3000, 8000);  // 3-8 seconds
                        }
                    }
                    stateStartTime = currentTime;
                    break;
                    
                case ACCEL:
                    // After accelerating, hold or continue
                    if (random(2) == 0) {
                        // Hold at current speed
                        state = HOLD;
                        accelRate = 0;
                        stateDuration = random(2000, 5000);  // 2-5 seconds
                    } else {
                        // Decelerate
                        state = DECEL;
                        if (random(3) == 0) {
                            targetSpeed = 0;  // Decel to stop
                        } else {
                            targetSpeed = random(0, currentSpeed);  // Decel to lower speed
                        }
                        accelRate = -random(MIN_ACCEL, MAX_ACCEL);
                        stateDuration = random(3000, 8000);  // 3-8 seconds
                    }
                    stateStartTime = currentTime;
                    break;
                    
                case DECEL:
                    // After decelerating, hold or accelerate
                    if (currentSpeed < 500) {
                        // Near zero - hold briefly then accelerate
                        state = HOLD;
                        currentSpeed = 0;
                        accelRate = 0;
                        stateDuration = random(2000, 5000);  // 2-5 seconds at zero
                    } else if (random(2) == 0) {
                        // Hold at current speed
                        state = HOLD;
                        accelRate = 0;
                        stateDuration = random(2000, 5000);  // 2-5 seconds
                    } else {
                        // Accelerate again
                        state = ACCEL;
                        targetSpeed = random(currentSpeed, MAX_SPEED);
                        accelRate = random(MIN_ACCEL, MAX_ACCEL);
                        stateDuration = random(3000, 8000);  // 3-8 seconds
                    }
                    stateStartTime = currentTime;
                    break;
            }
        }
    }
    
    // Check if we've reached or exceeded target speed (adjust acceleration)
    if (state == ACCEL && currentSpeed >= targetSpeed) {
        // Reached target while accelerating - transition to hold
        currentSpeed = targetSpeed;
        state = HOLD;
        accelRate = 0;
        stateStartTime = currentTime;
        stateDuration = random(2000, 5000);  // Hold for 2-5 seconds
    } else if (state == DECEL && currentSpeed <= targetSpeed) {
        // Reached target while decelerating - transition to hold
        currentSpeed = targetSpeed;
        if (currentSpeed < 500) {
            currentSpeed = 0;  // Snap to zero if very close
        }
        state = HOLD;
        accelRate = 0;
        stateStartTime = currentTime;
        stateDuration = random(2000, 5000);  // Hold for 2-5 seconds
    }
    
    // Final clamp
    if (currentSpeed < 0) currentSpeed = 0;
    if (currentSpeed > MAX_SPEED) currentSpeed = MAX_SPEED;
    
    return currentSpeed;
}
