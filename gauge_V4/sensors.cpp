/*
 * ========================================
 * SENSOR READING FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "sensors.h"
#include "globals.h"

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
    
    // Ignore unrealistic pulse intervals to prevent spikes
    // Minimum: 100 μs (filters electrical noise, allows up to ~500 km/h)
    // Maximum: MAX_VALID_PULSE_INTERVAL (filters stale intervals from standstill)
    if (pulseInterval > 100 && pulseInterval < MAX_VALID_PULSE_INTERVAL) {
        hallLastTime = currentTime;
        
        // Calculate speed in km/h * 100 using integer math:
        // km/h = (pulse freq [Hz] * 3600) / (TEETH_PER_REV * REVS_PER_KM)
        // pulse freq = 1,000,000 / pulseInterval (in microseconds)
        // km/h * 100 = (1,000,000 * 3600 * 100) / (pulseInterval * TEETH_PER_REV * REVS_PER_KM)
        // Simplify: (360,000,000,000) / (pulseInterval * TEETH_PER_REV * REVS_PER_KM)
        
        unsigned long divisor = (unsigned long)TEETH_PER_REV * (unsigned long)REVS_PER_KM;
        unsigned int speedRaw = (unsigned int)(360000000UL / (pulseInterval * divisor / 1000UL));
        hallSpeedRaw = speedRaw / 100.0;  // Keep for compatibility (MPH)
        
        // EMA filter with integer math:
        // FILTER_HALL_SPEED is 0-256: higher value = less filtering
        // Cast to unsigned long to prevent overflow in intermediate calculation
        spdHall = (unsigned int)(((unsigned long)speedRaw * FILTER_HALL_SPEED + (unsigned long)spdHall * (256 - FILTER_HALL_SPEED)) >> 8);
        Serial.println(spdHall);
    } else if (pulseInterval >= MAX_VALID_PULSE_INTERVAL) {
        // Very long interval detected - likely coming from standstill
        // Update hallLastTime to prevent spike on next valid pulse
        hallLastTime = currentTime;
    }
}

/**
 * hallSpeedUpdate - Handle Hall sensor timeout and minimum threshold
 */
void hallSpeedUpdate() {
    static unsigned long lastUpdateTime = 0;
    unsigned long currentTime = micros();
    unsigned long timeSinceLastPulse = currentTime - hallLastTime;
    
    // If it's been too long since last pulse, set speed to zero
    if (timeSinceLastPulse > HALL_PULSE_TIMEOUT) {
        hallSpeedRaw = 0;
        spdHall = 0;
    } 
    // If pulses have slowed significantly, decay speed more aggressively
    // This prevents "hanging" at low speeds when coming to a stop
    else if (timeSinceLastPulse > SPEED_DECAY_THRESHOLD && spdHall > 0) {
        // Decay speed by ~10% every update cycle when no recent pulses
        // This makes the speed drop more naturally when slowing to a stop
        spdHall = (spdHall * SPEED_DECAY_FACTOR) >> 8;
    }
    
    // Optionally, clamp very low speeds to zero for display stability
    if (spdHall < HALL_SPEED_MIN) {
        spdHall = 0;
    }
    
    // Update odometer based on Hall sensor speed (called every HALL_UPDATE_RATE ms)
    // Only update if Hall sensor is selected as speed source
    if (SPEED_SOURCE == 2 && lastUpdateTime != 0) {
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
    // At 12,000 RPM with 8 cylinders: interval = 1,000,000 / (12000*8/120) = 1250 μs
    // Minimum threshold of 500 μs allows up to ~18,750 RPM (very high for automotive)
    if (pulseInterval > 500) {
        // Calculate RPM using integer math:
        // RPM = (1,000,000 μs/sec * 120 sec/2min) / (pulseInterval * CYL_COUNT)
        // RPM = 120,000,000 / (pulseInterval * CYL_COUNT)
        // Note: CYL_COUNT is 2x old PULSES_PER_REVOLUTION, so we use 120M instead of 60M
        int rpmRaw = (int)(120000000.0 / (pulseInterval * CYL_COUNT));
        
        engineRPMRaw = rpmRaw;
        
        // Apply exponential moving average filter with integer math
        // FILTER_ENGINE_RPM is 0-256: higher value = less filtering
        engineRPMEMA = (rpmRaw * FILTER_ENGINE_RPM + engineRPMEMA * (256 - FILTER_ENGINE_RPM)) >> 8;
        
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
    // Select vehicle speed source: 0=off, 1=CAN, 2=Hall sensor, 3=GPS
    switch (SPEED_SOURCE) {
        case 0:  // Off
            spd = 0;
            break;
        case 1:  // CAN speed source
            spd = spdCAN;  // Already in km/h * 100 format
            break;
        case 2:  // Hall sensor speed source
            spd = spdHall;  // Already in km/h * 100 format
            break;
        case 3:  // GPS speed source
            spd = spdGPS;  // Already in km/h * 100 format
            break;
        default:  // Fallback to off
            spd = 0;
            break;
    }
    
    // Select engine RPM source: 0=off, 1=CAN, 2=coil negative
    switch (RPM_SOURCE) {
        case 0:  // Off
            RPM = 0;
            break;
        case 1:  // CAN RPM source
            RPM = rpmCAN;
            break;
        case 2:  // Coil negative tachometer
            RPM = engineRPMEMA;
            break;
        default:  // Fallback to off
            RPM = 0;
            break;
    }
    
    // Select oil pressure source: 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3
    switch (OIL_PRS_SOURCE) {
        case 0:  // Off
            oilPrs = 0;
            break;
        case 1:  // CAN oil pressure
            oilPrs = (oilPrsCAN/10.0) - 101.3;  // Convert from absolute kPa to gauge pressure
            break;
        case 2:  // Analog sensor AV1
            oilPrs = sensor_av1 / 10.0;
            break;
        case 3:  // Analog sensor AV2
            oilPrs = sensor_av2 / 10.0;
            break;
        case 4:  // Analog sensor AV3
            oilPrs = sensor_av3 / 10.0;
            break;
        default:  // Fallback to off
            oilPrs = 0;
            break;
    }
    
    // Select fuel pressure source: 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3
    switch (FUEL_PRS_SOURCE) {
        case 0:  // Off
            fuelPrs = 0;
            break;
        case 1:  // CAN fuel pressure
            fuelPrs = (fuelPrsCAN/10.0) - 101.3;  // Convert from absolute kPa to gauge pressure
            break;
        case 2:  // Analog sensor AV1
            fuelPrs = sensor_av1 / 10.0;
            break;
        case 3:  // Analog sensor AV2
            fuelPrs = sensor_av2 / 10.0;
            break;
        case 4:  // Analog sensor AV3
            fuelPrs = sensor_av3 / 10.0;
            break;
        default:  // Fallback to off
            fuelPrs = 0;
            break;
    }
    
    // Select coolant temperature source: 0=off, 1=CAN, 2=therm
    switch (COOLANT_TEMP_SOURCE) {
        case 0:  // Off
            coolantTemp = 0;
            break;
        case 1:  // CAN coolant temperature
            coolantTemp = (coolantTempCAN/10.0) - 273.15;  // Convert from Kelvin*10 to Celsius
            break;
        case 2:  // Thermistor sensor
            coolantTemp = therm;
            break;
        default:  // Fallback to off
            coolantTemp = 0;
            break;
    }
    
    // Select oil temperature source: 0=off, 1=CAN, 2=therm
    switch (OIL_TEMP_SOURCE) {
        case 0:  // Off
            oilTemp = 0;
            break;
        case 1:  // CAN oil temperature
            oilTemp = oilTempCAN / 10.0;  // Convert from Celsius*10 to Celsius
            break;
        case 2:  // Thermistor sensor
            oilTemp = therm;
            break;
        default:  // Fallback to off
            oilTemp = 0;
            break;
    }
    
    // Select manifold pressure/boost source: 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3
    switch (MAP_SOURCE) {
        case 0:  // Off
            manifoldPrs = 0;
            break;
        case 1:  // CAN manifold pressure
            manifoldPrs = mapCAN / 10.0;  // Convert from kPa*10 to kPa
            break;
        case 2:  // Analog sensor AV1
            manifoldPrs = sensor_av1 / 10.0;
            break;
        case 3:  // Analog sensor AV2
            manifoldPrs = sensor_av2 / 10.0;
            break;
        case 4:  // Analog sensor AV3
            manifoldPrs = sensor_av3 / 10.0;
            break;
        default:  // Fallback to off
            manifoldPrs = 0;
            break;
    }
    
    // Select Lambda/AFR source: 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3
    switch (LAMBDA_SOURCE) {
        case 0:  // Off
            afr = 0;
            break;
        case 1:  // CAN Lambda/AFR
            afr = afr1CAN / 1000.0;  // Convert from AFR*1000 to AFR
            break;
        case 2:  // Analog sensor AV1
            afr = sensor_av1 / 100.0;  // Assuming sensor gives AFR*100
            break;
        case 3:  // Analog sensor AV2
            afr = sensor_av2 / 100.0;
            break;
        case 4:  // Analog sensor AV3
            afr = sensor_av3 / 100.0;
            break;
        default:  // Fallback to off
            afr = 0;
            break;
    }
    
    // These remain unchanged as they don't have alternate sources
    fuelComp = fuelCompCAN/10.0;  // Fuel composition - divide by 10 (e.g., 850 becomes 85%)
    fuelLvlCAN = (int)((fuelLvl/fuelCapacity)*100);  // Calculate fuel level percentage for CAN transmission

}
