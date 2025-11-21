/*
 * ========================================
 * SENSOR READING FUNCTIONS
 * ========================================
 * 
 * Handle both analog and digital sensor reading with filtering
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

/**
 * readSensor - Generic analog sensor reader with filtering
 * 
 * Reads an analog input, maps it to 0-5V range, and applies exponential filtering
 * to reduce noise while maintaining responsiveness.
 * 
 * @param inputPin - Arduino analog pin to read (A0-A15)
 * @param oldVal - Previous filtered value (0-500 representing 0.00-5.00V)
 * @param filt - Filter coefficient (0-64): 
 *               - 64 = no filtering (instant response)
 *               - 32 = moderate filtering
 *               - 8 = heavy filtering (slow response, very smooth)
 *               Formula: newFiltered = (newRaw * filt + oldVal * (64-filt)) / 64
 * @return Filtered sensor value (0-500 representing 0-5V in 0.01V increments)
 * 
 * Example: Battery voltage with filt=8 means 8/64 = 12.5% new value, 87.5% old value
 */
unsigned long readSensor(int inputPin, int oldVal, int filt);

/**
 * read30PSIAsensor - Read 30 PSI absolute pressure sensor
 * 
 * Reads a 30 PSIA sensor (typical barometric or MAP sensor) with 0.5-4.5V output range.
 * Includes filtering for stable pressure readings.
 * 
 * @param inputPin - Arduino analog pin for pressure sensor
 * @param oldVal - Previous filtered value (kPa * 10)
 * @param filt - Filter coefficient (0-16): similar to readSensor but /16 instead of /64
 * @return Filtered pressure in kPa * 10 (e.g., 1013 = 101.3 kPa = 1 atmosphere)
 * 
 * Sensor characteristics:
 * - 0.5V = 0 PSIA
 * - 4.5V = 30 PSIA (206.8 kPa)
 * - ADC 102 (0.5V) = 0 kPa
 * - ADC 921 (4.5V) = 2068 (206.8 kPa * 10)
 */
unsigned long read30PSIAsensor(int inputPin, int oldVal, int filt);

/**
 * readThermSensor - Read GM-style thermistor temperature sensor
 * 
 * GM thermistors have a non-linear resistance vs. temperature curve.
 * This function reads the voltage from a voltage divider circuit and applies filtering.
 * Actual temperature conversion is done via curveLookup() with thermTable.
 * 
 * @param inputPin - Arduino analog pin for thermistor
 * @param oldVal - Previous filtered voltage value (0.00-5.00V)
 * @param filt - Filter coefficient (0-100): percentage of new value to use
 *               - 100 = no filtering
 *               - 50 = half new, half old (good for temperature - slow changing)
 * @return Filtered voltage reading (0.00-5.00V as float)
 * 
 * Note: Higher filter values (e.g., 50/100) provide more stability for slowly-changing
 * temperature readings, preventing gauge needle jitter.
 */
float readThermSensor(int inputPin, float oldVal, int filt);

/**
 * hallSpeedISR - Hall effect speed sensor interrupt handler
 * 
 * Triggered on each falling edge of the Hall sensor signal.
 * Calculates vehicle speed based on time between pulses.
 * 
 * Called from: Hardware interrupt on HALL_PIN falling edge
 */
void hallSpeedISR();

/**
 * hallSpeedUpdate - Handle Hall sensor timeout and minimum threshold
 * 
 * Called periodically from main loop to detect when vehicle has stopped
 * and to clamp very low speed values to zero for stable display.
 * 
 * Called from: main loop every 20ms (HALL_UPDATE_RATE)
 */
void hallSpeedUpdate();

/**
 * ignitionPulseISR - Interrupt service routine for engine RPM measurement
 * 
 * Triggered on each falling edge of the ignition coil pulse signal (via optocoupler).
 * Calculates engine RPM based on time between pulses and applies exponential moving average filter.
 * 
 * Called from: Hardware interrupt on IGNITION_PULSE_PIN falling edge
 */
void ignitionPulseISR();

/**
 * engineRPMUpdate - Handle engine RPM timeout and minimum threshold
 * 
 * Called periodically from main loop to detect when engine has stopped
 * and to clamp very low RPM values to zero for stable display.
 * 
 * Called from: main loop every 20ms (ENGINE_RPM_UPDATE_RATE)
 */
void engineRPMUpdate();

/**
 * curveLookup - Generic lookup table with linear interpolation
 * 
 * Converts an input value to an output value using a piecewise-linear lookup table.
 * Uses linear interpolation between breakpoints for smooth, accurate conversion.
 * 
 * This is essential for non-linear sensors like thermistors and fuel senders where
 * the relationship between voltage and physical quantity isn't linear.
 * 
 * @param input - Input value (e.g., voltage from sensor)
 * @param brkpts[] - Array of X-axis breakpoints (must be in ascending order)
 * @param curve[] - Array of corresponding Y-axis values
 * @param curveLength - Number of points in the table
 * @return Interpolated output value
 * 
 * Behavior:
 * - If input < first breakpoint: returns first curve value (flat extrapolation)
 * - If input > last breakpoint: returns last curve value (flat extrapolation)
 * - If input between breakpoints: linear interpolation between the two nearest points
 */
float curveLookup(float input, float brkpts[], float curve[], int curveLength);

/**
 * sigSelect - Process and route sensor data
 * 
 * This function acts as the central data router, converting raw CAN bus and sensor
 * readings into the appropriate units and formats for display and gauge output.
 * 
 * Called from: main loop (every cycle)
 */
void sigSelect(void);

/**
 * updateOdometer - Calculate and update odometer based on speed and time
 * 
 * Calculates distance traveled based on current speed and time interval,
 * then updates both total and trip odometers. This function is designed
 * to be called from any speed data source (GPS, Hall sensor, or CAN).
 * 
 * @param speedKmh - Current vehicle speed in km/h
 * @param timeIntervalMs - Time elapsed since last update in milliseconds
 * @return Distance traveled since last update in kilometers
 * 
 * Global variables modified:
 * - odo: Total odometer reading (km)
 * - odoTrip: Trip odometer reading (km)
 */
float updateOdometer(float speedKmh, unsigned long timeIntervalMs);

#endif // SENSORS_H
