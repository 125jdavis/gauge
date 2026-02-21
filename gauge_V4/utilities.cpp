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
/**
 * generateRPM - Generate simulated RPM for demo mode
 * 
 * Creates a realistic RPM sweep for testing the LED tachometer
 * without a running engine. Time-based implementation for smooth operation.
 * 
 * RPM range: 900-7000 RPM
 * Max rate of change: Configurable via MAX_RPM_RATE
 * 
 * Returns: RPM value (integer)
 */
int generateRPM(void){
    // Static variables persist between function calls
    static bool rpmSwitch = 0;              // Direction flag: 0=up, 1=down
    static int gRPM = 900;                  // Current RPM value
    static unsigned long lastUpdateTime = 0; // Last update timestamp for delta calculation
    
    // Constants - calibratable parameters
    const int MIN_RPM = 900;                // Minimum RPM
    const int MAX_RPM = 7000;               // Maximum RPM
    const int MAX_RPM_RATE = 1800;          // Maximum rate of change: 3000 RPM/second
    const int RPM_UP_RATE = 2400;           // Ramp up rate: 2400 RPM/second (adjustable)
    const int RPM_DOWN_RATE = 3500;         // Ramp down rate: 3200 RPM/second (adjustable)
    
    // Initialize on first call
    if (lastUpdateTime == 0) {
        lastUpdateTime = millis();
        gRPM = MIN_RPM;
        rpmSwitch = 0;
        return gRPM;
    }
    
    // Calculate time delta
    unsigned long currentTime = millis();
    unsigned long deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;
    
    // Skip if called too quickly (< 5ms) or if time wrapped
    if (deltaTime < 5 || deltaTime > 1000) {
        return gRPM;
    }
    
    // Calculate RPM change based on time delta
    // delta_RPM = (rate_in_RPM_per_sec * deltaTime_in_ms) / 1000
    int deltaRPM;
    if (rpmSwitch == 0) {
        // Ramping up
        deltaRPM = (RPM_UP_RATE * deltaTime) / 1000;
        gRPM += deltaRPM;
        
        // Check if we've hit the upper limit
        if (gRPM >= MAX_RPM) {
            gRPM = MAX_RPM;
            rpmSwitch = 1;  // Start ramping down
        }
    }
    else {
        // Ramping down
        deltaRPM = (RPM_DOWN_RATE * deltaTime) / 1000;
        gRPM -= deltaRPM;
        
        // Check if we've hit the lower limit
        if (gRPM <= MIN_RPM) {
            gRPM = MIN_RPM;
            rpmSwitch = 0;  // Start ramping up
        }
    }
    
    return gRPM;
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
                        stateDuration = random(500, 2000);  // .5-2 seconds at zero
                    } else if (random(2) == 0) {
                        // Hold at current speed
                        state = HOLD;
                        accelRate = 0;
                        stateDuration = random(500, 2000);  // .5-2 seconds
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
        stateDuration = random(0, 2000);  // Hold for 0-2 seconds
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

/**
 * generateSyntheticCoolantTemp - Generate synthetic coolant temperature signal
 * 
 * Temperature range: -10°C to 230°C
 * Max rate: 20°C/second
 * Spends 75% of time between 60°C and 210°C
 * 
 * Returns: Temperature in °C (float)
 */
float generateSyntheticCoolantTemp(void) {
    static float currentTemp = 20.0;           // Current temperature in °C
    static float targetTemp = 90.0;            // Target temperature in °C
    static float rate = 0.0;                   // Current rate in °C/s
    static unsigned long lastUpdateTime = 0;   // Last update timestamp
    static unsigned long stateStartTime = 0;   // When current state started
    static unsigned long stateDuration = 0;    // How long to maintain current target
    
    // Constants
    const float MIN_TEMP = -10.0;
    const float MAX_TEMP = 140.0;
    const float PREFERRED_MIN = 60.0;   // Spend 75% of time above this
    const float PREFERRED_MAX = 110.0;  // Spend 75% of time below this
    const float MAX_RATE = 18.0;        // 20°C/second max
    const float MIN_RATE = 2.0;         // 2°C/second min
    
    // Initialize on first call
    if (lastUpdateTime == 0) {
        lastUpdateTime = millis();
        stateStartTime = millis();
        currentTemp = 20.0;
        targetTemp = random(60, 120);  // Start with target in normal range
        rate = random(20, 100) / 10.0; // 2.0 to 10.0 °C/s
        stateDuration = random(5000, 15000);  // 5-15 seconds
        return currentTemp;
    }
    
    // Calculate time delta
    unsigned long currentTime = millis();
    unsigned long deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;
    
    // Skip if called too quickly
    if (deltaTime < 10) {
        return currentTemp;
    }
    
    // Update temperature based on current rate
    float deltaTemp = rate * (deltaTime / 1000.0);
    currentTemp += deltaTemp;
    
    // Clamp to valid range
    if (currentTemp < MIN_TEMP) currentTemp = MIN_TEMP;
    if (currentTemp > MAX_TEMP) currentTemp = MAX_TEMP;
    
    // Check if it's time to change target
    unsigned long timeInState = currentTime - stateStartTime;
    
    if (fabs(currentTemp - targetTemp) < 1.0 || timeInState >= stateDuration) {
        // Reached target or time expired - set new target
        stateStartTime = currentTime;
        stateDuration = random(5000, 15000);  // 5-15 seconds
        
        // 75% chance to target preferred range, 25% chance for extremes
        if (random(100) < 75) {
            // Stay in preferred operating range
            targetTemp = random((int)PREFERRED_MIN, (int)PREFERRED_MAX);
        } else {
            // Occasionally go to extremes
            if (random(2) == 0) {
                targetTemp = random((int)MIN_TEMP, (int)PREFERRED_MIN);  // Cold
            } else {
                targetTemp = random((int)PREFERRED_MAX, (int)MAX_TEMP);  // Hot
            }
        }
        
        // Set rate (positive if heating, negative if cooling)
        float rateAbs = random((int)(MIN_RATE * 10), (int)(MAX_RATE * 10)) / 10.0;
        rate = (targetTemp > currentTemp) ? rateAbs : -rateAbs;
    }
    
    return currentTemp;
}

/**
 * generateSyntheticOilPressure - Generate synthetic oil pressure signal
 * 
 * Pressure range: 0-600 kPa
 * Max rate: 300 kPa/second
 * 
 * Returns: Pressure in kPa (float)
 */
float generateSyntheticOilPressure(void) {
    static float currentPressure = 100.0;      // Current pressure in kPa
    static float targetPressure = 350.0;       // Target pressure in kPa
    static float rate = 0.0;                   // Current rate in kPa/s
    static unsigned long lastUpdateTime = 0;   // Last update timestamp
    static unsigned long stateStartTime = 0;   // When current state started
    static unsigned long stateDuration = 0;    // How long to maintain current target
    
    // Constants
    const float MIN_PRESSURE = 0.0;
    const float MAX_PRESSURE = 600.0;
    const float MAX_RATE = 300.0;  // 300 kPa/second max
    const float MIN_RATE = 50.0;   // 50 kPa/second min
    
    // Initialize on first call
    if (lastUpdateTime == 0) {
        lastUpdateTime = millis();
        stateStartTime = millis();
        currentPressure = 100.0;
        targetPressure = random(200, 400);
        rate = random(500, 3000) / 10.0;  // 50 to 300 kPa/s
        stateDuration = random(3000, 10000);  // 3-10 seconds
        return currentPressure;
    }
    
    // Calculate time delta
    unsigned long currentTime = millis();
    unsigned long deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;
    
    if (deltaTime < 10) {
        return currentPressure;
    }
    
    // Update pressure
    float deltaPressure = rate * (deltaTime / 1000.0);
    currentPressure += deltaPressure;
    
    // Clamp to valid range
    if (currentPressure < MIN_PRESSURE) currentPressure = MIN_PRESSURE;
    if (currentPressure > MAX_PRESSURE) currentPressure = MAX_PRESSURE;
    
    // Check if it's time to change target
    unsigned long timeInState = currentTime - stateStartTime;
    
    if (fabs(currentPressure - targetPressure) < 5.0 || timeInState >= stateDuration) {
        stateStartTime = currentTime;
        stateDuration = random(3000, 10000);  // 3-10 seconds
        
        // New random target
        targetPressure = random(0, (int)MAX_PRESSURE);
        
        // Set rate (positive or negative)
        float rateAbs = random((int)(MIN_RATE * 10), (int)(MAX_RATE * 10)) / 10.0;
        rate = (targetPressure > currentPressure) ? rateAbs : -rateAbs;
    }
    
    return currentPressure;
}

/**
 * generateSyntheticFuelPressure - Generate synthetic fuel pressure signal
 * 
 * Pressure range: 0-600 kPa
 * Max rate: 600 kPa/second
 * 
 * Returns: Pressure in kPa (float)
 */
float generateSyntheticFuelPressure(void) {
    static float currentPressure = 300.0;      // Current pressure in kPa
    static float targetPressure = 400.0;       // Target pressure in kPa
    static float rate = 0.0;                   // Current rate in kPa/s
    static unsigned long lastUpdateTime = 0;   // Last update timestamp
    static unsigned long stateStartTime = 0;   // When current state started
    static unsigned long stateDuration = 0;    // How long to maintain current target
    
    // Constants
    const float MIN_PRESSURE = 0.0;
    const float MAX_PRESSURE = 600.0;
    const float MAX_RATE = 600.0;  // 600 kPa/second max
    const float MIN_RATE = 100.0;  // 100 kPa/second min
    
    // Initialize on first call
    if (lastUpdateTime == 0) {
        lastUpdateTime = millis();
        stateStartTime = millis();
        currentPressure = 300.0;
        targetPressure = random(250, 450);
        rate = random(1000, 6000) / 10.0;  // 100 to 600 kPa/s
        stateDuration = random(2000, 8000);  // 2-8 seconds
        return currentPressure;
    }
    
    // Calculate time delta
    unsigned long currentTime = millis();
    unsigned long deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;
    
    if (deltaTime < 10) {
        return currentPressure;
    }
    
    // Update pressure
    float deltaPressure = rate * (deltaTime / 1000.0);
    currentPressure += deltaPressure;
    
    // Clamp to valid range
    if (currentPressure < MIN_PRESSURE) currentPressure = MIN_PRESSURE;
    if (currentPressure > MAX_PRESSURE) currentPressure = MAX_PRESSURE;
    
    // Check if it's time to change target
    unsigned long timeInState = currentTime - stateStartTime;
    
    if (fabs(currentPressure - targetPressure) < 10.0 || timeInState >= stateDuration) {
        stateStartTime = currentTime;
        stateDuration = random(2000, 8000);  // 2-8 seconds
        
        // New random target
        targetPressure = random(0, (int)MAX_PRESSURE);
        
        // Set rate (positive or negative)
        float rateAbs = random((int)(MIN_RATE * 10), (int)(MAX_RATE * 10)) / 10.0;
        rate = (targetPressure > currentPressure) ? rateAbs : -rateAbs;
    }
    
    return currentPressure;
}

/**
 * generateSyntheticFuelLevel - Generate synthetic fuel level signal
 * 
 * Level range: 0-100%
 * Max rate: 10%/second
 * 
 * Returns: Fuel level in percent (float)
 */
float generateSyntheticFuelLevel(void) {
    static float currentLevel = 75.0;          // Current level in %
    static float targetLevel = 50.0;           // Target level in %
    static float rate = 0.0;                   // Current rate in %/s
    static unsigned long lastUpdateTime = 0;   // Last update timestamp
    static unsigned long stateStartTime = 0;   // When current state started
    static unsigned long stateDuration = 0;    // How long to maintain current target
    
    // Constants
    const float MIN_LEVEL = 0.0;
    const float MAX_LEVEL = 100.0;
    const float MAX_RATE = 10.0;   // 10%/second max
    const float MIN_RATE = 1.0;    // 1%/second min
    
    // Initialize on first call
    if (lastUpdateTime == 0) {
        lastUpdateTime = millis();
        stateStartTime = millis();
        currentLevel = 75.0;
        targetLevel = random(20, 80);
        rate = random(10, 100) / 10.0;  // 1 to 10 %/s
        stateDuration = random(10000, 30000);  // 10-30 seconds
        return currentLevel;
    }
    
    // Calculate time delta
    unsigned long currentTime = millis();
    unsigned long deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;
    
    if (deltaTime < 10) {
        return currentLevel;
    }
    
    // Update level
    float deltaLevel = rate * (deltaTime / 1000.0);
    currentLevel += deltaLevel;
    
    // Clamp to valid range
    if (currentLevel < MIN_LEVEL) currentLevel = MIN_LEVEL;
    if (currentLevel > MAX_LEVEL) currentLevel = MAX_LEVEL;
    
    // Check if it's time to change target
    unsigned long timeInState = currentTime - stateStartTime;
    
    if (fabs(currentLevel - targetLevel) < 1.0 || timeInState >= stateDuration) {
        stateStartTime = currentTime;
        stateDuration = random(10000, 30000);  // 10-30 seconds
        
        // New random target
        targetLevel = random(0, 100);
        
        // Set rate (positive or negative)
        float rateAbs = random((int)(MIN_RATE * 10), (int)(MAX_RATE * 10)) / 10.0;
        rate = (targetLevel > currentLevel) ? rateAbs : -rateAbs;
    }
    
    return currentLevel;
}

/**
 * generateSyntheticManifoldPressure - Generate synthetic manifold pressure signal
 * 
 * Pressure range: 0-250 kPa
 * Max rate: 600 kPa/second
 * 
 * Returns: Pressure in kPa (float)
 */
float generateSyntheticManifoldPressure(void) {
    static float currentPressure = 100.0;      // Current pressure in kPa
    static float targetPressure = 150.0;       // Target pressure in kPa
    static float rate = 0.0;                   // Current rate in kPa/s
    static unsigned long lastUpdateTime = 0;   // Last update timestamp
    static unsigned long stateStartTime = 0;   // When current state started
    static unsigned long stateDuration = 0;    // How long to maintain current target
    
    // Constants
    const float MIN_PRESSURE = 0.0;
    const float MAX_PRESSURE = 250.0;
    const float MAX_RATE = 200.0;  // 200 kPa/second max (reduced from 600 by factor of 3)
    const float MIN_RATE = 10;   // 33.3 kPa/second min (reduced from 100 by factor of 3)
    
    // Initialize on first call
    if (lastUpdateTime == 0) {
        lastUpdateTime = millis();
        stateStartTime = millis();
        currentPressure = 100.0;
        targetPressure = random(80, 280);
        rate = random(100, 2000) / 10.0;  // 100 to 600 kPa/s
        stateDuration = random(1000, 5000);  // 1-5 seconds
        return currentPressure;
    }
    
    // Calculate time delta
    unsigned long currentTime = millis();
    unsigned long deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;
    
    if (deltaTime < 10) {
        return currentPressure;
    }
    
    // Update pressure
    float deltaPressure = rate * (deltaTime / 1000.0);
    currentPressure += deltaPressure;
    
    // Clamp to valid range
    if (currentPressure < MIN_PRESSURE) currentPressure = MIN_PRESSURE;
    if (currentPressure > MAX_PRESSURE) currentPressure = MAX_PRESSURE;
    
    // Check if it's time to change target
    unsigned long timeInState = currentTime - stateStartTime;
    
    if (fabs(currentPressure - targetPressure) < 10.0 || timeInState >= stateDuration) {
        stateStartTime = currentTime;
        stateDuration = random(2000, 5000);  // 1-5 seconds
        
        // New random target
        targetPressure = random(0, (int)MAX_PRESSURE);
        
        // Set rate (positive or negative)
        float rateAbs = random((int)(MIN_RATE * 10), (int)(MAX_RATE * 10)) / 10.0;
        rate = (targetPressure > currentPressure) ? rateAbs : -rateAbs;
    }
    
    return currentPressure;
}

/**
 * generateOdometerTestSpeed - Generate a deterministic 1-mile odometer accuracy test profile
 * 
 * Starts 5 seconds after device boot. Speed profile:
 * - Phase 1 (5s-10s):  Linear ramp from 0 to 96.56 km/h (60 mph) over 5 seconds
 * - Phase 2 (10s-65s): Hold constant at 96.56 km/h for 55 seconds
 * - Phase 3 (65s-70s): Linear ramp from 96.56 km/h to 0 over 5 seconds
 * Total distance = 1 mile, allowing accuracy comparison of the mechanical
 * odometer (motor) and trip odometer (OLED display) against the expected value.
 * 
 * Returns 0 before the test starts and after it completes.
 * 
 * Returns: Speed in km/h * 100 format (e.g., 9656 = 96.56 km/h)
 */
int generateOdometerTestSpeed(void) {
    const unsigned long TEST_START_MS = 5000UL;   // 5 seconds after boot
    const unsigned long RAMP_UP_MS    = 5000UL;   // 5-second ramp up (0 → 96.56 km/h)
    const unsigned long HOLD_MS       = 55000UL;  // 55-second hold at 96.56 km/h
    const unsigned long RAMP_DOWN_MS  = 5000UL;   // 5-second ramp down (96.56 km/h → 0)
    const int           MAX_SPEED     = 9656;     // 96.56 km/h * 100

    unsigned long currentTime = millis();

    // Before test starts, return 0
    if (currentTime < TEST_START_MS) {
        return 0;
    }

    unsigned long elapsed = currentTime - TEST_START_MS;

    // Phase 1: Ramp up
    if (elapsed < RAMP_UP_MS) {
        return (int)((long)MAX_SPEED * (long)elapsed / (long)RAMP_UP_MS);
    }

    // Phase 2: Hold constant speed
    if (elapsed < RAMP_UP_MS + HOLD_MS) {
        return MAX_SPEED;
    }

    // Phase 3: Ramp down
    unsigned long rampDownStart = RAMP_UP_MS + HOLD_MS;
    if (elapsed < rampDownStart + RAMP_DOWN_MS) {
        unsigned long rampDownElapsed = elapsed - rampDownStart;
        return (int)((long)MAX_SPEED * (long)(RAMP_DOWN_MS - rampDownElapsed) / (long)RAMP_DOWN_MS);
    }

    // Test complete
    return 0;
}

/**
 * mapFloat - Map a float value from one range to another
 * 
 * Linear interpolation from input range to output range
 */
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  // Prevent division by zero if input range is invalid
  if (in_max == in_min) {
    return out_min;  // Return minimum output value for degenerate case
  }
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
