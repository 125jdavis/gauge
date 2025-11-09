/*
 * ========================================
 * GPS FUNCTIONS
 * ========================================
 * 
 * This file contains functions for GPS data processing
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef GPS_FUNCTIONS_H
#define GPS_FUNCTIONS_H

#include <Arduino.h>

/**
 * fetchGPSdata - Read and process GPS data
 * 
 * Reads GPS NMEA sentences, parses them, and updates speed and odometer.
 * Handles both polled and interrupt-based GPS reading.
 */
void fetchGPSdata();

/**
 * useInterrupt - Enable/disable interrupt-based GPS reading
 * 
 * @param v - true to enable interrupts, false to disable
 */
void useInterrupt(boolean v);

#endif // GPS_FUNCTIONS_H
