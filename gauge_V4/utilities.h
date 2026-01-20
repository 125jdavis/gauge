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
 * without a running engine. Time-based implementation with configurable rates.
 * 
 * RPM range: 900-7000 RPM
 * Ramp up rate: 2400 RPM/second (calibratable)
 * Ramp down rate: 3200 RPM/second (calibratable)
 * 
 * Called from: sigSelect() when RPM_SOURCE == 3 (synthetic mode)
 * 
 * Returns: Simulated RPM value (integer, 900-7000)
 */
int generateRPM(void);

/**
 * generateSyntheticSpeed - Generate realistic synthetic speed signal for debugging
 * 
 * Creates a realistic speed profile with random accelerations, decelerations,
 * and constant speed periods. Respects maximum acceleration limits.
 * 
 * Speed range: 0-160 km/h
 * Max acceleration: 20 m/s² (≈2g, very aggressive)
 * 
 * Returns: Speed in km/h * 100 format (e.g., 5000 = 50 km/h)
 * 
 * Called from: sigSelect() when SPEED_SOURCE == 4 (synthetic mode)
 */
int generateSyntheticSpeed(void);

/**
 * generateSyntheticCoolantTemp - Generate synthetic coolant temperature signal
 * 
 * Temperature range: -10°C to 230°C
 * Max rate: 20°C/second
 * Spends 75% of time between 60°C and 210°C
 * 
 * Returns: Temperature in °C (float)
 * 
 * Called from: sigSelect() when COOLANT_TEMP_SOURCE == 3 (synthetic mode)
 */
float generateSyntheticCoolantTemp(void);

/**
 * generateSyntheticOilPressure - Generate synthetic oil pressure signal
 * 
 * Pressure range: 0-600 kPa
 * Max rate: 300 kPa/second
 * 
 * Returns: Pressure in kPa (float)
 * 
 * Called from: sigSelect() when OIL_PRS_SOURCE == 5 (synthetic mode)
 */
float generateSyntheticOilPressure(void);

/**
 * generateSyntheticFuelPressure - Generate synthetic fuel pressure signal
 * 
 * Pressure range: 0-600 kPa
 * Max rate: 600 kPa/second
 * 
 * Returns: Pressure in kPa (float)
 * 
 * Called from: sigSelect() when FUEL_PRS_SOURCE == 5 (synthetic mode)
 */
float generateSyntheticFuelPressure(void);

/**
 * generateSyntheticFuelLevel - Generate synthetic fuel level signal
 * 
 * Level range: 0-100%
 * Max rate: 10%/second
 * 
 * Returns: Fuel level in percent (float)
 * 
 * Called from: main loop or dedicated fuel level function
 */
float generateSyntheticFuelLevel(void);

/**
 * generateSyntheticManifoldPressure - Generate synthetic manifold pressure signal
 * 
 * Pressure range: 0-250 kPa
 * Max rate: 600 kPa/second
 * 
 * Returns: Pressure in kPa (float)
 * 
 * Called from: sigSelect() when MAP_SOURCE == 5 (synthetic mode)
 */
float generateSyntheticManifoldPressure(void);

#endif // UTILITIES_H
