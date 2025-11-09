/*
 * ========================================
 * UTILITY FUNCTIONS IMPLEMENTATION
 * ========================================
 * 
 * This file contains the implementation of utility functions for sensor reading,
 * filtering, and interpolation
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#include "Utils.h"

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
 * 
 * Sensor characteristics:
 * - 0.5V = 0 PSIA
 * - 4.5V = 30 PSIA (206.8 kPa)
 * - ADC 102 (0.5V) = 0 kPa
 * - ADC 921 (4.5V) = 2068 (206.8 kPa * 10)
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
 * 
 * Note: Higher filter values (e.g., 50/100) provide more stability for slowly-changing
 * temperature readings, preventing gauge needle jitter.
 */
float readThermSensor(int inputPin, float oldVal, int filt)
{
    int raw = analogRead (inputPin);  // Read ADC: 0-1023
    float newVal = map( raw, 0, 1023, 0, 500)*0.01;  // Map to 0-5V as float
    float filtVal = ((newVal*filt) + (oldVal*(100-filt)))*0.01;  // Filter (*0.01 for percentage)
    return filtVal; 
}

/**
 * curveLookup - Generic lookup table with linear interpolation
 * 
 * This is essential for non-linear sensors like thermistors and fuel senders where
 * the relationship between voltage and physical quantity isn't linear.
 * 
 * Behavior:
 * - If input < first breakpoint: returns first curve value (flat extrapolation)
 * - If input > last breakpoint: returns last curve value (flat extrapolation)
 * - If input between breakpoints: linear interpolation between the two nearest points
 * 
 * Interpolation formula: y = y0 + (y1-y0)*(x-x0)/(x1-x0)
 * where (x0,y0) and (x1,y1) are the surrounding breakpoints
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
