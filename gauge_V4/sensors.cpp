/*
 * ========================================
 * SENSOR READING FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "sensors.h"
#include "globals.h"
#include "odometer.h"

/**
 * readSensor - Generic analog sensor reader with filtering
 */
unsigned long readSensor(int inputPin, int oldVal, int filt)  
{
    int raw = analogRead (inputPin);  // Read ADC: 0-1023 for 0-5V input
    unsigned long newVal = map( raw, 0, 1023, 0, 500);  // Map to 0-500 (0.00-5.00V in 0.01V steps)
    unsigned long filtVal = ((newVal*filt) + (oldVal*(64-filt)))>>6;  // Exponential filter (>>6 is divide by 64)
    return filtVal; 
}

/**
 * read30PSIAsensor - Read 30 PSI absolute pressure sensor
 */
unsigned long read30PSIAsensor(int inputPin, int oldVal, int filt)
{
    int raw = analogRead (inputPin);  // Read ADC: 0-1023
    unsigned long newVal = map( raw, 102, 921, 0, 2068);  // Map 0.5-4.5V to 0-30 PSIA (0-206.8 kPa)
    unsigned long filtVal = ((newVal*filt) + (oldVal*(16-filt)))>>4;  // Filter (>>4 is divide by 16)
    return filtVal; 
}

/**
 * readThermSensor - Read GM-style thermistor temperature sensor
 */
float readThermSensor(int inputPin, float oldVal, int filt)
{
    int raw = analogRead (inputPin);  // Read ADC: 0-1023
    float newVal = map( raw, 0, 1023, 0, 500)*0.01;  // Map to 0-5V as float
    float filtVal = ((newVal*filt) + (oldVal*(100-filt)))*0.01;  // Filter (*0.01 for percentage)
    return filtVal; 
}

/**
 * hallSpeedISR - Hall effect speed sensor interrupt handler
 */
void hallSpeedISR() {
    unsigned long currentTime = micros();
    unsigned long pulseInterval = currentTime - hallLastTime;
    hallLastTime = currentTime;

    // Ignore any implausibly short intervals (can set a minimum if needed, e.g. electrical noise)
    // For 150 mph, the shortest plausible pulse interval is much less than 1ms; let's allow anything > 100 μs
    if (pulseInterval > 100) {
        // Calculate speed in km/h * 100 using integer math:
        // km/h = (pulse freq [Hz] * 3600) / (TEETH_PER_REV * REVS_PER_KM)
        // pulse freq = 1,000,000 / pulseInterval (in microseconds)
        // km/h * 100 = (1,000,000 * 3600 * 100) / (pulseInterval * TEETH_PER_REV * REVS_PER_KM)
        // Simplify: (360,000,000,000) / (pulseInterval * TEETH_PER_REV * REVS_PER_KM)
        
        unsigned long divisor = (unsigned long)TEETH_PER_REV * (unsigned long)REVS_PER_KM;
        int speedRaw = (int)(360000000UL / (pulseInterval * divisor / 1000UL));
        hallSpeedRaw = speedRaw / 100.0;  // Keep for compatibility (MPH)
        
        // EMA filter with integer math:
        // Use fixed-point: multiply alpha by 256 for integer calculations
        int alpha256 = (int)(ALPHA_HALL_SPEED * 256);
        spdHall = (speedRaw * alpha256 + spdHall * (256 - alpha256)) >> 8;
        Serial.println(spdHall);
    }
}

/**
 * hallSpeedUpdate - Handle Hall sensor timeout and minimum threshold
 */
void hallSpeedUpdate() {
    static unsigned long lastUpdateTime = 0;
    unsigned long currentTime = micros();
    
    // If it's been too long since last pulse, set speed to zero
    if ((currentTime - hallLastTime) > HALL_PULSE_TIMEOUT) {
        hallSpeedRaw = 0;
        spdHall = 0;
    }
    // Optionally, clamp very low speeds to zero for display stability (0.5 km/h = 50 in km/h*100)
    if (spdHall < 50) {
        spdHall = 0;
    }
    
    // Update odometer based on Hall sensor speed (called every HALL_UPDATE_RATE ms)
    // Only update if Hall sensor is selected as speed source
    if (SPEED_SOURCE == 1 && lastUpdateTime != 0) {
        unsigned long timeIntervalMicros = currentTime - lastUpdateTime;
        unsigned long timeIntervalMs = timeIntervalMicros / 1000;
        // spdHall is already in km/h * 100, convert to km/h for updateOdometer
        float speedKmh = spdHall * 0.01;
        updateOdometer(speedKmh, timeIntervalMs);
    }
    lastUpdateTime = currentTime;
}

/**
 * updateOdometer - Calculate and update odometer based on speed and time
 * 
 * Calculates distance traveled based on current speed and time interval,
 * then updates both total and trip odometers. This function is designed
 * to be called from any speed data source (GPS, Hall sensor, or CAN).
 * 
 * @param speedKmh - Current vehicle speed in km/h
 * @param timeIntervalMs - Time elapsed since last update in milliseconds
 * 
 * Processing:
 * 1. Only integrates distance if speed > 2 km/h (reduces drift when stationary)
 * 2. Calculates distance: distance (km) = speed (km/h) * time (ms) * 2.77778e-7
 *    - Conversion factor: 1 km/h = 1000m/3600s = 0.277778 m/s = 2.77778e-7 km/ms
 * 3. Updates global odo and odoTrip variables
 * 4. Returns distance traveled for potential odometer motor movement
 * 
 * Global variables modified:
 * - odo: Total odometer reading (km)
 * - odoTrip: Trip odometer reading (km)
 * 
 * @return Distance traveled since last update in kilometers
 */
float updateOdometer(float speedKmh, unsigned long timeIntervalMs) {
    float distanceTraveled = 0;
    
    // Only integrate if speed > 2 km/h (reduces GPS drift errors when stationary)
    if (speedKmh > 2) {
        // Calculate distance traveled: distance (km) = speed (km/h) * time (ms) * conversion factor
        // Conversion: 1 km/h = 1000m/3600s = 0.277778 m/s = 2.77778e-7 km/ms
        distanceTraveled = speedKmh * timeIntervalMs * 2.77778e-7;
    }
    
    // Update odometers
    odo = odo + distanceTraveled;
    odoTrip = odoTrip + distanceTraveled;
    
    return distanceTraveled;
}

/**
 * ignitionPulseISR - Interrupt service routine for engine RPM measurement
 */
void ignitionPulseISR() {
    unsigned long currentTime = micros();
    unsigned long pulseInterval = currentTime - ignitionLastTime;
    ignitionLastTime = currentTime;

    // Ignore implausibly short intervals to filter electrical noise
    // At 12,000 RPM with 4 pulses/rev: interval = 1,000,000 / (12000*4/60) = 1250 μs
    // Minimum threshold of 500 μs allows up to ~18,750 RPM (very high for automotive)
    if (pulseInterval > 500) {
        // Calculate pulse frequency in Hz: freq = 1,000,000 μs/sec / interval
        float pulseFreq = 1000000.0 / pulseInterval;
        
        // Convert pulse frequency to RPM
        // RPM = (pulses per second * 60 seconds per minute) / pulses per revolution
        float rpmRaw = (pulseFreq * 60.0) / PULSES_PER_REVOLUTION;
        
        engineRPMRaw = rpmRaw;
        
        // Apply exponential moving average filter for smooth display
        // EMA formula: new_EMA = (alpha * new_value) + ((1 - alpha) * old_EMA)
        // Higher alpha (e.g., 0.7) = more responsive, lower alpha = more smoothing
        engineRPMEMA = (ALPHA_ENGINE_RPM * rpmRaw) + ((1.0 - ALPHA_ENGINE_RPM) * engineRPMEMA);
        
        // Uncomment for debugging (note: Serial.print in ISR can cause timing issues)
        // Serial.print("RPM: ");
        // Serial.println(engineRPMEMA);
    }
}

/**
 * engineRPMUpdate - Handle engine RPM timeout and minimum threshold
 */
void engineRPMUpdate() {
    unsigned long currentTime = micros();
    
    // If it's been too long since last pulse, engine has stopped
    if ((currentTime - ignitionLastTime) > IGNITION_PULSE_TIMEOUT) {
        engineRPMRaw = 0;
        engineRPMEMA = 0;
    }
    
    // Clamp very low RPM to zero for display stability
    // Prevents needle flutter during engine start/stop
    if (engineRPMEMA < ENGINE_RPM_MIN) {
        engineRPMEMA = 0;
    }
}

/**
 * curveLookup - Generic lookup table with linear interpolation
 */
float curveLookup(float input, float brkpts[], float curve[], int curveLength){
  int index = 1;

  // Find input's position within the breakpoints
  for (int i = 0; i <= curveLength-1; i++){
    if (input < brkpts[0]){
      // Input below range - return minimum value (flat extrapolation)
      float output = curve[0];
      return output;
    } 
    else if (input <= brkpts[i+1]){
      // Found the interval containing input value
      index = i+1;
      break;
    } 
    else if (input > brkpts[curveLength-1]){
      // Input above range - return maximum value (flat extrapolation)
      float output = curve[curveLength-1];
      return output;
    }
  } 

  // Linear interpolation between breakpoints at index-1 and index
  float x1 = brkpts[index];      // Upper breakpoint X
  float x0 = brkpts[index-1];    // Lower breakpoint X
  float y1 = curve[index];       // Upper breakpoint Y
  float y0 = curve[index-1];     // Lower breakpoint Y
  
  // Calculate interpolated value: slope * distance + offset
  float output = (((y1-y0)/(x1-x0))*(input-x0))+y0;
  return output;
}

/**
 * sigSelect - Process and route sensor data
 */
void sigSelect (void) {
    // Select speed source based on SPEED_SOURCE configuration
    // Store speed in km/h * 100 (integer format for efficiency)
    // 0 = GPS, 1 = Hall sensor, 2 = CAN
    switch (SPEED_SOURCE) {
        case 0:  // GPS speed source
            spd = spdGPS;  // Already in km/h * 100 format
            break;
        case 1:  // Hall sensor speed source
            spd = spdHall;  // Already in km/h * 100 format
            break;
        case 2:  // CAN speed source
            spd = spdCAN;  // Already in km/h * 100 format
            break;
        default:  // Fallback to Hall sensor
            spd = spdHall;
            break;
    }
    
    //spdMph = spd *0.6213712;  // Unused conversion to mph
    //spdCAN = (int)(v*16);  // Speed formatted for CAN bus transmission (km/h * 16 per Haltech protocol)
    //RPM = rpmCAN;  // Direct copy of RPM from CAN bus
    RPM = engineRPMEMA; //RPM as measured from coil negative tachometer
    coolantTemp = (coolantTempCAN/10)-273.15; // Convert from Kelvin*10 to Celsius (K to C: subtract 273.15)
    oilPrs = (oilPrsCAN/10)-101.3;   // Convert from absolute kPa to gauge pressure (subtract atmospheric ~101.3 kPa)
    fuelPrs = (fuelPrsCAN/10)-101.3;  // Convert from absolute kPa to gauge pressure
    oilTemp = therm;  // Oil temperature from thermistor sensor (already in Celsius)
    afr = (float)afr1CAN/1000;  // Air/Fuel Ratio - divide by 1000 (e.g., 14700 becomes 14.7)
    fuelComp = fuelCompCAN/10;  // Fuel composition - divide by 10 (e.g., 850 becomes 85%)
    fuelLvlCAN = (int)((fuelLvl/fuelCapacity)*100);  // Calculate fuel level percentage for CAN transmission

}
