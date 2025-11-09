/*
 * ========================================
 * SENSOR FUNCTIONS
 * ========================================
 * 
 * This file contains functions for processing sensor data
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef SENSOR_FUNCTIONS_H
#define SENSOR_FUNCTIONS_H

#include <Arduino.h>

/**
 * sigSelect - Process and route sensor data
 * 
 * This function acts as the central data router, converting raw CAN bus and sensor
 * readings into the appropriate units and formats for display and gauge output.
 * 
 * Processing steps:
 * 1. Convert GPS speed from km/h to appropriate formats
 * 2. Extract RPM from CAN bus
 * 3. Convert temperatures from Kelvin to Celsius
 * 4. Convert absolute pressures to gauge pressures (subtract atmospheric)
 * 5. Scale AFR and fuel composition values
 * 6. Calculate fuel level percentage for CAN transmission
 * 
 * Called from: main loop (every cycle)
 */
void sigSelect(void);

#endif // SENSOR_FUNCTIONS_H
