/*
 * ========================================
 * ODOMETER FUNCTIONS
 * ========================================
 * 
 * Modular odometer distance calculation and motor control
 * Can be called from GPS, Hall Effect sensor, or CAN speed sources
 */

#ifndef ODOMETER_H
#define ODOMETER_H

#include <Arduino.h>

/**
 * updateOdometer - Calculate and update odometer based on speed and time
 * 
 * Calculates distance traveled based on current speed and time interval,
 * then updates both total and trip odometers. This function is designed
 * to be called from any speed data source (GPS, Hall sensor, or CAN).
 * 
 * @param speedKmh - Current vehicle speed in km/h
 * @param timeIntervalMs - Time elapsed since last update in milliseconds
 * 
 * Processing:
 * 1. Only integrates distance if speed > 2 km/h (reduces drift when stationary)
 * 2. Calculates distance: distance (km) = speed (km/h) * time (ms) * 2.77778e-7
 *    - Conversion factor: 1 km/h = 1000m/3600s = 0.277778 m/s = 2.77778e-7 km/ms
 * 3. Updates global odo and odoTrip variables
 * 4. Returns distance traveled for potential odometer motor movement
 * 
 * Global variables modified:
 * - odo: Total odometer reading (km)
 * - odoTrip: Trip odometer reading (km)
 * 
 * Called from:
 * - fetchGPSdata() - GPS speed source
 * - hallSpeedISR() - Hall effect speed sensor source
 * - Or any future speed source (CAN, etc.)
 * 
 * @return Distance traveled since last update in kilometers
 */
float updateOdometer(float speedKmh, unsigned long timeIntervalMs);

/**
 * moveOdometerMotor - Move mechanical odometer motor by calculated distance
 * 
 * Calculates the number of steps required to advance the mechanical odometer
 * based on distance traveled, motor characteristics, and gear ratios.
 * 
 * @param distanceKm - Distance to advance the odometer in kilometers
 * 
 * Calculation:
 * 1. Convert distance to steps based on:
 *    - ODO_STEPS: Steps per revolution of the stepper motor
 *    - ODO_MOTOR_TEETH: Number of teeth on motor gear
 *    - ODO_GEAR_TEETH: Number of teeth on odometer gear
 *    - Odometer scale: How many km per full rotation of odometer
 * 
 * 2. Gear ratio: motor_revs = (ODO_GEAR_TEETH / ODO_MOTOR_TEETH) * odo_revs
 * 3. Motor steps = motor_revs * ODO_STEPS
 * 
 * Example:
 * - Motor: 32 steps/rev, 10 teeth
 * - Odometer gear: 60 teeth, 1 km per revolution
 * - For 0.1 km: 
 *   - Odometer revs = 0.1
 *   - Motor revs = (60/10) * 0.1 = 0.6
 *   - Steps = 0.6 * 32 = 19.2 ≈ 19 steps
 * 
 * Note: This function currently calculates steps but does not move the motor.
 * Motor movement code should be added based on the specific Stepper library
 * implementation and hardware constraints (e.g., non-blocking movement).
 * 
 * Called from:
 * - fetchGPSdata() after updateOdometer()
 * - hallSpeedISR() after updateOdometer()
 * - Or periodically from main loop
 */
void moveOdometerMotor(float distanceKm);

#endif // ODOMETER_H
