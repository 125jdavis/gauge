/*
 * ========================================
 * OUTPUT CONTROL FUNCTIONS
 * ========================================
 * 
 * Stepper motors, LED tachometer, and odometer control
 */

#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <Arduino.h>

// Stepper motor angle calculation functions
int speedometerAngle(int sweep);              // GPS speed to speedometer angle
int speedometerAngleGPS(int sweep);           // GPS speed (original version)
int speedometerAngleCAN(int sweep);           // CAN speed to angle
int speedometerAngleHall(int sweep);          // Hall sensor speed to angle
int speedometerAngleS(int sweep);             // Generic speed to angle for motorS (integer math)
void updateMotorSTarget(int sweep);           // Update final target angle for motorS (called at 50Hz)
void updateMotorSSmoothing(void);             // Interpolate motorS position for smooth motion (call frequently)
int fuelLvlAngle(int sweep);                  // Fuel level to gauge angle
int coolantTempAngle(int sweep);              // Coolant temp to gauge angle

// Stepper motor control functions
void motorZeroSynchronous(void);              // Return all motors to zero
void motorSweepSynchronous(void);             // Full sweep test for all motors

// LED tachometer control
void ledShiftLight(int ledRPM);               // Update LED tachometer display

// Odometer motor control
void moveOdometerMotor(float distanceKm);     // Queue distance for mechanical odometer motor
void updateOdometerMotor(void);                // Non-blocking motor update (call from main loop)

#endif // OUTPUTS_H
