/*
 * ========================================
 * MOTOR CONTROL FUNCTIONS
 * ========================================
 * 
 * This file contains functions for stepper motor control
 * 
 * Author: Jesse Davis
 * Date: 8/24/2024
 */

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

/**
 * speedometerAngle - Calculate speedometer needle angle from speed
 * 
 * @param sweep - Total sweep range of motor in steps
 * @return Target angle in motor steps
 */
int speedometerAngle(int sweep);

/**
 * fuelLvlAngle - Calculate fuel gauge needle angle from fuel level
 * 
 * @param sweep - Total sweep range of motor in steps
 * @return Target angle in motor steps
 */
int fuelLvlAngle(int sweep);

/**
 * coolantTempAngle - Calculate temperature gauge needle angle from temperature
 * 
 * @param sweep - Total sweep range of motor in steps
 * @return Target angle in motor steps
 */
int coolantTempAngle(int sweep);

/**
 * motorZeroSynchronous - Return all gauge needles to zero position
 * 
 * Moves all motors to their zero position and waits for completion.
 * Blocking function - waits until all motors have finished moving.
 */
void motorZeroSynchronous();

/**
 * motorSweepSynchronous - Sweep all gauge needles through full range
 * 
 * Moves all motors to maximum position, then back to zero.
 * Used for startup self-test and motor homing.
 * Blocking function.
 */
void motorSweepSynchronous();

/**
 * shutdown - Perform graceful shutdown sequence
 * 
 * Saves odometer values to EEPROM, returns gauge needles to zero,
 * and cuts power to the system.
 */
void shutdown();

#endif // MOTOR_CONTROL_H
