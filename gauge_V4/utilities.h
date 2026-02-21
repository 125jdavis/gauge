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
 * Returns 0 before the test starts (< 5 s) and after the test completes (> 70 s).
 * 
 * Returns: Speed in km/h * 100 format (e.g., 9656 = 96.56 km/h)
 * 
 * Called from: sigSelect() when SPEED_SOURCE == 5 (odometer test mode)
 */
int generateOdometerTestSpeed(void);

/**
 * mapFloat - Map a float value from one range to another
 * 
 * Similar to Arduino's map() but for floating point values.
 * Linearly interpolates a value from input range to output range.
 * 
 * @param x - Value to map
 * @param in_min - Lower bound of input range
 * @param in_max - Upper bound of input range
 * @param out_min - Lower bound of output range
 * @param out_max - Upper bound of output range
 * @return Mapped value in output range
 * 
 * Example: mapFloat(50, 0, 100, 0.0, 1.0) returns 0.5
 */
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);

#endif // UTILITIES_H
