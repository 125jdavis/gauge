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
#include "tach_preview.h"

// Stepper motor angle calculation functions
int speedometerAngle(int sweep);              // GPS speed to speedometer angle
int speedometerAngleGPS(int sweep);           // GPS speed (original version)
int speedometerAngleCAN(int sweep);           // CAN speed to angle
int speedometerAngleHall(int sweep);          // Hall sensor speed to angle
int speedometerAngleS(int sweep);             // Generic speed to angle for motorS (integer math)
void updateMotorSTarget(int sweep);           // Update final target angle for motorS (called at 50Hz)
void updateMotorSSmoothing(void);             // Interpolate motorS position for smooth motion (call frequently)
void updateMotors1to4Target(int t1, int t2, int t3, int t4);  // Update final targets for motors 1-4
void updateMotors1to4Smoothing(void);         // Interpolate motors 1-4 positions for smooth motion (call frequently)
int fuelLvlAngle(int sweep, int minPct, int maxPct);        // Fuel level to gauge angle
int coolantTempAngle(int sweep, int minTemp, int maxTemp);  // Coolant temp to gauge angle
int motorFunctionAngle(uint8_t func, int sweep, int minVal, int maxVal);  // Angle for a configured MOTOR_FUNC_* assignment

// Stepper motor control functions
void motorZeroSynchronous(void);              // Return all motors to zero (fast, for startup calibration)
void motorZeroTimed(void);                    // Return all motors to zero with synchronized timed stepping (for shutdown)
void motorSweepSynchronous(void);             // Full sweep test for all motors

// LED tachometer control
void ledShiftLight(int ledRPM);               // Update LED tachometer display
void setTachPreview(const TachPreview &preview);
void stopTachPreview(void);

// Odometer motor control
void moveOdometerMotor(float distanceKm);     // Queue distance for mechanical odometer motor
void moveOdometerMotorRevs(int revs);         // Queue signed motor revolutions for serial command
void stopOdometerMotor(void);                 // Cancel pending serial-commanded movement
void updateOdometerMotor(void);                // Non-blocking motor update (call from main loop)

#endif // OUTPUTS_H
