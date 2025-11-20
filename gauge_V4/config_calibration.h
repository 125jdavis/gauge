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
extern uint16_t M1_SWEEP;        // Motor 1: 58 degrees * 12 = 696 steps (typically fuel level gauge)
extern uint16_t M2_SWEEP;        // Motor 2: 58 degrees * 12 = 696 steps (typically secondary gauge)
extern uint16_t M3_SWEEP;       // Motor 3: 118 degrees * 12 = 1416 steps (typically speedometer - wider range)
extern uint16_t M4_SWEEP;        // Motor 4: 58 degrees * 12 = 696 steps (typically coolant temp)

// ===== ANALOG SENSOR FILTER COEFFICIENTS =====
// Battery Voltage Sensor - Filter coefficient out of 64
extern uint8_t FILTER_VBATT;           // 8/64 = light filtering

// Battery Voltage Sensor - Voltage divider scaling factor
extern float VBATT_SCALER;      // Formula: Vbatt = ADC_reading * (5.0/1023) * ((R1+R2)/R2)
                                    // R1=10k, R2=3.3k

// Fuel Level Sensor - Light filter: 1/64 = very responsive to changes
extern uint8_t FILTER_FUEL;

// Coolant/Oil Temperature Thermistor - Medium filter: 50/100 for stable temp reading
extern uint8_t FILTER_THERM;

// Analog Inputs for 0-5V sensors
extern uint8_t FILTER_AV1;             // Filter coefficient for barometric pressure (4/16 = moderate filtering)
extern uint8_t FILTER_AV2;            // Filter coefficient for sensor B (12/16)
extern uint8_t FILTER_AV3;            // Filter coefficient for sensor C (12/16)

// ===== HALL EFFECT SPEED SENSOR PARAMETERS =====
extern uint16_t REVS_PER_KM;        // Revolutions per kilometer (vehicle-specific)
extern uint8_t TEETH_PER_REV;         // Teeth per revolution (sensor-specific)
extern float ALPHA_HALL_SPEED;       // EMA filter coefficient (lower value is more filtered)
extern float HALL_SPEED_MIN;         // Minimum reportable speed (MPH)

// ===== ENGINE RPM SENSOR PARAMETERS =====
// Pulses per engine revolution (for 4-stroke engines: cylinders / 2)
// Examples: 4-cyl=2, 6-cyl=3, 8-cyl=4, 3-cyl=1.5
extern float PULSES_PER_REVOLUTION;

// EMA filter coefficient (lower value = more filtered)
// Range: 0.0 to 1.0 (0.7 balances smoothing and responsiveness)
extern float ALPHA_ENGINE_RPM;

// Minimum reportable RPM (engine idle ~600-800)
extern float ENGINE_RPM_MIN;

// ===== SPEEDOMETER CALIBRATION =====
extern uint16_t SPEEDO_MAX;    // Maximum speedometer reading: 100 mph * 100 (stored as integer for precision)

// ===== LED TACHOMETER CONFIGURATION =====
extern uint8_t NUM_LEDS;              // Total number of LEDs in the tachometer strip
extern uint8_t WARN_LEDS;              // Warning zone LEDs on each side of center (turns yellow/orange)
extern uint8_t SHIFT_LEDS;             // Shift light LEDs on each side of center (turns red at shift point)
extern unsigned int TACH_MAX;       // RPM at which shift light activates and flashes
extern unsigned int TACH_MIN;       // Minimum RPM to show on tach (below this LEDs are off)

// ===== ODOMETER MOTOR CALIBRATION =====
extern uint8_t ODO_STEPS;             // Steps per revolution for odometer motor
extern uint8_t ODO_MOTOR_TEETH;       // Number of teeth on motor gear
extern uint8_t ODO_GEAR_TEETH;        // Number of teeth on odometer gear

// ===== SPEED SOURCE SELECTION =====
// Speed source for both speedometer and odometer: 0=GPS, 1=Hall sensor, 2=CAN
// This unified parameter ensures both speedometer display and odometer use the same source
extern uint8_t SPEED_SOURCE;

// ===== TIME ZONE OFFSET =====
// Hours to add to UTC time for local time zone (-12 to +12)
// Note: This is also saved/loaded from EEPROM
extern byte clockOffset;

// ===== FUEL TANK CAPACITY =====
extern float fuelCapacity;            // Total fuel tank capacity in gallons (used for percentage calculations)

#endif // CONFIG_CALIBRATION_H
