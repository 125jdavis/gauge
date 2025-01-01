#include "sensors.h"

// Generic Sensor reader - reads, re-maps, and filters analog input values
unsigned long readSensor(int inputPin, int oldVal, int filt)  // read voltage, map to 0-5v, and filter
{
    int raw = analogRead (inputPin);
    unsigned long newVal = map( raw, 0, 1023, 0, 500);  
    unsigned long filtVal = ((newVal*filt) + (oldVal*(64-filt)))>>6;
    return filtVal; 
}

// Reads 30 PSI Absolute Sensor
unsigned long read30PSIAsensor(int inputPin, int oldVal, int filt)  // read voltage, map to 0-30 PSIA, and filter
{
    int raw = analogRead (inputPin);
    unsigned long newVal = map( raw, 102, 921, 0, 2068); 
    unsigned long filtVal = ((newVal*filt) + (oldVal*(16-filt)))>>4;
    return filtVal; 
}

// Reads GM CLT/IAT Thermistor
float readThermSensor(int inputPin, float oldVal, int filt)  // read voltage, map to -40-150 deg C, and filter
{
    int raw = analogRead (inputPin);
    float newVal = map( raw, 0, 1023, 0, 500)*0.01; 
    float filtVal = ((newVal*filt) + (oldVal*(100-filt)))*0.01;
    return filtVal; 
}

// Generic Curve Lookup
float curveLookup(float input, float brkpts[], float curve[], int curveLength){
  int index = 1;

  //find input's position within the breakpoints
  for (int i = 0; i <= curveLength-1; i++){
    if (input < brkpts[0]){
      float output = curve[0];
      return output;
    } 
    else if (input <= brkpts[i+1]){
      index = i+1;
      break;
    } 
    else if (input > brkpts[curveLength-1]){
      float output = curve[curveLength-1];
      return output;
    }
  } 

  // interpolation
  float x1 = brkpts[index];
  float x0 = brkpts[index-1];
  float y1 = curve[index];
  float y0 = curve[index-1];
  
  float output = (((y1-y0)/(x1-x0))*(input-x0))+y0;
  return output;
}

