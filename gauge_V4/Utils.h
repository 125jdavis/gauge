/*
 * ========================================
 * UTILITY FUNCTIONS
 * ========================================
 * 
 * This file contains utility functions for sensor reading,
 * filtering, and interpolation
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef UTILS_H
#define UTILS_H

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
 * @return Filtered voltage reading (0.00-5.00V as float)
 */
float readThermSensor(int inputPin, float oldVal, int filt);

/**
 * curveLookup - Generic lookup table with linear interpolation
 * 
 * Converts an input value to an output value using a piecewise-linear lookup table.
 * Uses linear interpolation between breakpoints for smooth, accurate conversion.
 * 
 * @param input - Input value (e.g., voltage from sensor)
 * @param brkpts[] - Array of X-axis breakpoints (must be in ascending order)
 * @param curve[] - Array of corresponding Y-axis values
 * @param curveLength - Number of points in the table
 * @return Interpolated output value
 */
float curveLookup(float input, float brkpts[], float curve[], int curveLength);

#endif // UTILS_H
