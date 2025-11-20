/*
 * ========================================
 * CALIBRATION PARAMETERS DEFINITIONS
 * ========================================
 */

#include "config_calibration.h"

// ===== STEPPER MOTOR SWEEP RANGES =====
uint16_t M1_SWEEP = 58 * 12;        // Motor 1: 58 degrees * 12 = 696 steps
uint16_t M2_SWEEP = 58 * 12;        // Motor 2: 58 degrees * 12 = 696 steps
uint16_t M3_SWEEP = 118 * 12;       // Motor 3: 118 degrees * 12 = 1416 steps
uint16_t M4_SWEEP = 58 * 12;        // Motor 4: 58 degrees * 12 = 696 steps

// ===== ANALOG SENSOR FILTER COEFFICIENTS =====
uint8_t FILTER_VBATT = 8;           // 8/64 = light filtering
float VBATT_SCALER = 0.040923;      // Voltage divider scaling factor
uint8_t FILTER_FUEL = 1;            // Light filter
uint8_t FILTER_THERM = 50;          // Medium filter
uint8_t FILTER_AV1 = 4;             // Barometric pressure filter
uint8_t FILTER_AV2 = 12;            // Sensor B filter
uint8_t FILTER_AV3 = 12;            // Sensor C filter

// ===== HALL EFFECT SPEED SENSOR PARAMETERS =====
uint16_t REVS_PER_MILE = 6234;      // Revolutions per mile
uint8_t TEETH_PER_REV = 12;         // Teeth per revolution
float ALPHA_HALL_SPEED = 0.8;       // EMA filter coefficient
float HALL_SPEED_MIN = 0.5;         // Minimum reportable speed (MPH)

// ===== ENGINE RPM SENSOR PARAMETERS =====
float PULSES_PER_REVOLUTION = 4.0;  // Pulses per engine revolution
float ALPHA_ENGINE_RPM = 0.7;       // EMA filter coefficient
float ENGINE_RPM_MIN = 100.0;       // Minimum reportable RPM

// ===== SPEEDOMETER CALIBRATION =====
uint16_t SPEEDO_MAX = 100 * 100;    // Maximum speedometer reading

// ===== LED TACHOMETER CONFIGURATION =====
uint8_t NUM_LEDS = 26;              // Total number of LEDs
uint8_t WARN_LEDS = 6;              // Warning zone LEDs
uint8_t SHIFT_LEDS = 2;             // Shift light LEDs
unsigned int TACH_MAX = 6000;       // RPM at shift point
unsigned int TACH_MIN = 3000;       // Minimum RPM to show

// ===== ODOMETER MOTOR CALIBRATION =====
uint8_t ODO_STEPS = 32;             // Steps per revolution
uint8_t ODO_MOTOR_TEETH = 10;       // Number of teeth on motor gear
uint8_t ODO_GEAR_TEETH = 60;        // Number of teeth on odometer gear

// ===== SPEED SOURCE SELECTION =====
uint8_t SPEED_SOURCE = 1;           // 0=GPS, 1=Hall sensor, 2=CAN (default to Hall sensor)

// ===== TIME ZONE OFFSET =====
byte clockOffset = 0;               // Hours to add to UTC time

// ===== FUEL TANK CAPACITY =====
float fuelCapacity = 16;            // Total fuel tank capacity in gallons
