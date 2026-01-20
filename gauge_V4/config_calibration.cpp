/*
 * ========================================
 * CALIBRATION PARAMETERS DEFINITIONS
 * ========================================
 */

#include "config_calibration.h"

// ===== STEPPER MOTOR SWEEP RANGES =====
uint16_t M1_SWEEP = 58 * 12;        // Motor 1: 58 degrees * 12 = 696 steps
uint16_t M2_SWEEP = 58 * 12;        // Motor 2: 58 degrees * 12 = 696 steps
uint16_t M3_SWEEP = 58 * 12;        // Motor 3: 58 degrees * 12 = 696 steps (same config as motor1)
uint16_t M4_SWEEP = 58 * 12;        // Motor 4: 58 degrees * 12 = 696 steps
uint16_t MS_SWEEP = 2098;           // Motor S: (118° / 0.9°) * 16 microsteps = 2097.78 ≈ 2098 steps (speedometer)

// ===== MOTOR SWEEP TIMING =====
uint16_t MOTOR_SWEEP_TIME_MS = 200;  // Time in milliseconds for motors to sweep full range (calibratable)

// ===== ANALOG SENSOR FILTER COEFFICIENTS =====
uint8_t FILTER_VBATT = 8;           // 8/64 = light filtering
float VBATT_SCALER = 0.040923;      // Voltage divider scaling factor
uint8_t FILTER_FUEL = 1;            // Light filter
uint8_t FILTER_THERM = 50;          // Medium filter
uint8_t FILTER_AV1 = 4;             // Barometric pressure filter
uint8_t FILTER_AV2 = 12;            // Sensor B filter
uint8_t FILTER_AV3 = 12;            // Sensor C filter

// ===== HALL EFFECT SPEED SENSOR PARAMETERS =====
uint16_t REVS_PER_KM = 1625;        // Revolutions of VSS per kilometer (8000 pulses per mile is standard)
uint8_t TEETH_PER_REV = 8;         // Teeth per revolution of the VSS
uint8_t FILTER_HALL_SPEED = 64;    // EMA filter coefficient (205/256 ≈ 0.8)
uint8_t HALL_SPEED_MIN = 50;        // Minimum reportable speed in km/h*100 (50 = 0.5 km/h)

// ===== ENGINE RPM SENSOR PARAMETERS =====
uint8_t CYL_COUNT = 8;              // Cylinder count (8 = 2x old PULSES_PER_REVOLUTION of 4.0)
uint8_t FILTER_ENGINE_RPM = 179;    // EMA filter coefficient (179/256 ≈ 0.7)
uint8_t ENGINE_RPM_MIN = 100;       // Minimum reportable RPM

// ===== SPEEDOMETER CALIBRATION =====
uint16_t SPEEDO_MAX = 100 * 100;    // Maximum speedometer reading

// ===== LED TACHOMETER CONFIGURATION =====
uint8_t NUM_LEDS = 26;              // Total number of LEDs
uint8_t WARN_LEDS = 6;              // Warning zone LEDs
uint8_t SHIFT_LEDS = 2;             // Shift light LEDs
unsigned int TACH_MAX = 6000;       // RPM at shift point
unsigned int TACH_MIN = 3000;       // Minimum RPM to show

// ===== ODOMETER MOTOR CALIBRATION =====
uint16_t ODO_STEPS = 4096;             // Steps per revolution
uint8_t ODO_MOTOR_TEETH = 16;       // Number of teeth on motor gear
uint8_t ODO_GEAR_TEETH = 20;        // Number of teeth on odometer gear

// ===== SIGNAL SOURCE SELECTION =====
uint8_t SPEED_SOURCE = 4;           // 0=off, 1=CAN, 2=Hall sensor, 3=GPS, 4=Synthetic (debug)
uint8_t RPM_SOURCE = 3;             // 0=off, 1=CAN, 2=coil negative, 3=Synthetic (debug)
uint8_t OIL_PRS_SOURCE = 5;         // 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3, 5=Synthetic (debug)
uint8_t FUEL_PRS_SOURCE = 5;        // 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3, 5=Synthetic (debug)
uint8_t COOLANT_TEMP_SOURCE = 5;    // 0=off, 1=CAN, 2=therm, 3=Synthetic (debug)
uint8_t OIL_TEMP_SOURCE = 2;        // 0=off, 1=CAN, 2=therm (default to therm sensor)
uint8_t MAP_SOURCE = 5;             // 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3, 5=Synthetic (debug)
uint8_t LAMBDA_SOURCE = 1;          // 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3

// ===== TIME ZONE OFFSET =====
byte clockOffset = 0;               // Hours to add to UTC time

// ===== FUEL TANK CAPACITY =====
float fuelCapacity = 16;            // Total fuel tank capacity in gallons
