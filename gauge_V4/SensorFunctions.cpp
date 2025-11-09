/*
 * ========================================
 * SENSOR FUNCTIONS IMPLEMENTATION
 * ========================================
 * 
 * This file contains the implementation of sensor processing functions
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#include "SensorFunctions.h"
#include "GlobalVariables.h"

/**
 * sigSelect - Process and route sensor data
 */
void sigSelect (void) {
    spd = v_new; // Speed in km/h * 100 from GPS
    //spdMph = spd *0.6213712;  // Unused conversion to mph
    spdCAN = (int)(v*16);  // Speed formatted for CAN bus transmission (km/h * 16 per Haltech protocol)
    RPM = rpmCAN;  // Direct copy of RPM from CAN bus
    coolantTemp = (coolantTempCAN/10)-273.15; // Convert from Kelvin*10 to Celsius (K to C: subtract 273.15)
    oilPrs = (oilPrsCAN/10)-101.3;   // Convert from absolute kPa to gauge pressure (subtract atmospheric ~101.3 kPa)
    fuelPrs = (fuelPrsCAN/10)-101.3;  // Convert from absolute kPa to gauge pressure
    oilTemp = therm;  // Oil temperature from thermistor sensor (already in Celsius)
    afr = (float)afr1CAN/1000;  // Air/Fuel Ratio - divide by 1000 (e.g., 14700 becomes 14.7)
    fuelComp = fuelCompCAN/10;  // Fuel composition - divide by 10 (e.g., 850 becomes 85%)
    fuelLvlCAN = (int)((fuelLvl/fuelCapacity)*100);  // Calculate fuel level percentage for CAN transmission
}
