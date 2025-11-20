/*
 * ========================================
 * UTILITY FUNCTIONS
 * ========================================
 * 
 * Shutdown, EEPROM, demo, and helper functions
 */

#ifndef UTILITIES_H
#define UTILITIES_H

#include <Arduino.h>

/**
 * shutdown - Gracefully shut down the gauge system
 * 
 * Saves all settings to EEPROM, displays shutdown screens,
 * zeros gauge needles, and cuts power to the Arduino
 * 
 * Called from: main loop when vBatt < 1V
 */
void shutdown(void);

/**
 * generateRPM - Generate simulated RPM for demo mode
 * 
 * Creates a realistic RPM sweep for testing the LED tachometer
 * without a running engine
 * 
 * Called from: main loop when demo mode is enabled
 */
void generateRPM(void);

/**
 * serialInputFunc - Serial port input for manual testing
 * 
 * Reads integer values from serial monitor and updates test variables
 * Used for debugging sensor values without physical sensors
 * 
 * Called from: main loop when debugging
 */
void serialInputFunc(void);

#endif // UTILITIES_H
