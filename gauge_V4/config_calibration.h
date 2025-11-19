/*
 * ========================================
 * CALIBRATION PARAMETERS
 * ========================================
 * 
 * User-adjustable calibration parameters
 * These values can be tuned for different vehicle configurations
 * 
 * Following STYLE.md: calibration parameters use UPPER_SNAKE_CASE but are NOT constexpr
 * to allow for future runtime configuration via serial/EEPROM
 */

#ifndef CONFIG_CALIBRATION_H
#define CONFIG_CALIBRATION_H

#include <Arduino.h>

// ===== STEPPER MOTOR SWEEP RANGES =====
// Total steps for full sweep: degrees * 12 steps/degree
// X25.168 motors have 315° range at 1/3° per step
uint16_t M1_SWEEP = 58 * 12;        // Motor 1: 58 degrees * 12 = 696 steps (typically fuel level gauge)
uint16_t M2_SWEEP = 58 * 12;        // Motor 2: 58 degrees * 12 = 696 steps (typically secondary gauge)
uint16_t M3_SWEEP = 118 * 12;       // Motor 3: 118 degrees * 12 = 1416 steps (typically speedometer - wider range)
uint16_t M4_SWEEP = 58 * 12;        // Motor 4: 58 degrees * 12 = 696 steps (typically coolant temp)

// ===== ANALOG SENSOR FILTER COEFFICIENTS =====
// Battery Voltage Sensor - Filter coefficient out of 64
uint8_t FILTER_VBATT = 8;           // 8/64 = light filtering

// Battery Voltage Sensor - Voltage divider scaling factor
float VBATT_SCALER = 0.040923;      // Formula: Vbatt = ADC_reading * (5.0/1023) * ((R1+R2)/R2)
                                    // R1=10k, R2=3.3k

// Fuel Level Sensor - Light filter: 1/64 = very responsive to changes
uint8_t FILTER_FUEL = 1;

// Coolant/Oil Temperature Thermistor - Medium filter: 50/100 for stable temp reading
uint8_t FILTER_THERM = 50;

// Analog Inputs for 0-5V sensors
uint8_t FILTER_AV1 = 4;             // Filter coefficient for barometric pressure (4/16 = moderate filtering)
uint8_t FILTER_AV2 = 12;            // Filter coefficient for sensor B (12/16)
uint8_t FILTER_AV3 = 12;            // Filter coefficient for sensor C (12/16)

// ===== HALL EFFECT SPEED SENSOR PARAMETERS =====
uint16_t REVS_PER_MILE = 6234;      // Revolutions per mile (vehicle-specific)
uint8_t TEETH_PER_REV = 12;         // Teeth per revolution (sensor-specific)
float ALPHA_HALL_SPEED = 0.8;       // EMA filter coefficient (lower value is more filtered)
float HALL_SPEED_MIN = 0.5;         // Minimum reportable speed (MPH)

// ===== ENGINE RPM SENSOR PARAMETERS =====
// Pulses per engine revolution (for 4-stroke engines: cylinders / 2)
// Examples: 4-cyl=2, 6-cyl=3, 8-cyl=4, 3-cyl=1.5
float PULSES_PER_REVOLUTION = 4.0;

// EMA filter coefficient (lower value = more filtered)
// Range: 0.0 to 1.0 (0.7 balances smoothing and responsiveness)
float ALPHA_ENGINE_RPM = 0.7;

// Minimum reportable RPM (engine idle ~600-800)
float ENGINE_RPM_MIN = 100.0;

// ===== SPEEDOMETER CALIBRATION =====
uint16_t SPEEDO_MAX = 100 * 100;    // Maximum speedometer reading: 100 mph * 100 (stored as integer for precision)

// ===== LED TACHOMETER CONFIGURATION =====
uint8_t NUM_LEDS = 26;              // Total number of LEDs in the tachometer strip
uint8_t WARN_LEDS = 6;              // Warning zone LEDs on each side of center (turns yellow/orange)
uint8_t SHIFT_LEDS = 2;             // Shift light LEDs on each side of center (turns red at shift point)
unsigned int TACH_MAX = 6000;       // RPM at which shift light activates and flashes
unsigned int TACH_MIN = 3000;       // Minimum RPM to show on tach (below this LEDs are off)

// ===== ODOMETER MOTOR CALIBRATION =====
uint8_t ODO_STEPS = 32;             // Steps per revolution for odometer motor

// ===== TIME ZONE OFFSET =====
// Hours to add to UTC time for local time zone (-12 to +12)
// Note: This is also saved/loaded from EEPROM
byte clockOffset = 0;

// ===== FUEL TANK CAPACITY =====
float fuelCapacity = 16;            // Total fuel tank capacity in gallons (used for percentage calculations)

#endif // CONFIG_CALIBRATION_H
