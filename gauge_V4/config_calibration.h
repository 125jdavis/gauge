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
// Total steps for full sweep: degrees * 12 steps/degree (for X25.168 motors)
// X25.168 motors have 315° range at 1/3° per step
extern uint16_t M1_SWEEP;        // Motor 1: 58 degrees * 12 = 696 steps (typically fuel level gauge)
extern uint16_t M2_SWEEP;        // Motor 2: 58 degrees * 12 = 696 steps (typically secondary gauge)
extern uint16_t M3_SWEEP;        // Motor 3: 58 degrees * 12 = 696 steps (same config as motor1)
extern uint16_t M4_SWEEP;        // Motor 4: 58 degrees * 12 = 696 steps (typically coolant temp)
extern uint16_t MS_SWEEP;        // Motor S: 118 degrees (speedometer - 16 microsteps, 400 steps/rev, 0.9°/step)

// ===== MOTOR SWEEP TIMING =====
extern uint16_t MOTOR_SWEEP_TIME_MS;  // Time in milliseconds for motors to sweep full range during startup test

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
extern uint8_t FILTER_HALL_SPEED;   // EMA filter coefficient (0-256): 256=no filter, 128=moderate, 64=heavy
extern uint8_t HALL_SPEED_MIN;      // Minimum reportable speed in km/h*100 (e.g., 50 = 0.5 km/h)

// ===== ENGINE RPM SENSOR PARAMETERS =====
// Cylinder count (for 4-stroke engines)
// Examples: 4-cyl=4, 6-cyl=6, 8-cyl=8, 3-cyl=3
extern uint8_t CYL_COUNT;

// EMA filter coefficient (0-256): 256=no filter, 179=moderate (0.7*256), 128=more filtered
extern uint8_t FILTER_ENGINE_RPM;

// Minimum reportable RPM (engine idle ~600-800)
extern uint8_t ENGINE_RPM_MIN;

// ===== SPEEDOMETER CALIBRATION =====
extern uint16_t SPEEDO_MAX;    // Maximum speedometer reading: 100 mph * 100 (stored as integer for precision)

// ===== LED TACHOMETER CONFIGURATION =====
extern uint8_t NUM_LEDS;              // Total number of LEDs in the tachometer strip
extern uint8_t WARN_LEDS;              // Warning zone LEDs on each side of center (turns yellow/orange)
extern uint8_t SHIFT_LEDS;             // Shift light LEDs on each side of center (turns red at shift point)
extern unsigned int TACH_MAX;       // RPM at which shift light activates and flashes
extern unsigned int TACH_MIN;       // Minimum RPM to show on tach (below this LEDs are off)

// ===== ODOMETER MOTOR CALIBRATION =====
extern uint16_t ODO_STEPS;             // Steps per revolution for odometer motor
extern uint8_t ODO_MOTOR_TEETH;       // Number of teeth on motor gear
extern uint8_t ODO_GEAR_TEETH;        // Number of teeth on odometer gear

// ===== SIGNAL SOURCE SELECTION =====
// These parameters determine which sensor/source to use for each signal
// This allows flexible configuration of data sources

// Speed source for both speedometer and odometer: 0=off, 1=CAN, 2=Hall sensor, 3=GPS, 4=Synthetic (debug), 5=Odometer test (1-mile profile)
extern uint8_t SPEED_SOURCE;

// Engine RPM source: 0=off, 1=CAN, 2=coil negative
extern uint8_t RPM_SOURCE;

// Oil pressure source: 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3
extern uint8_t OIL_PRS_SOURCE;

// Fuel pressure source: 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3
extern uint8_t FUEL_PRS_SOURCE;

// Coolant temperature source: 0=off, 1=CAN, 2=therm
extern uint8_t COOLANT_TEMP_SOURCE;

// Oil temperature source: 0=off, 1=CAN, 2=therm
extern uint8_t OIL_TEMP_SOURCE;

// Manifold pressure/boost source: 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3
extern uint8_t MAP_SOURCE;

// Lambda/AFR source: 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3
extern uint8_t LAMBDA_SOURCE;

// ===== TIME ZONE OFFSET =====
// Hours to add to UTC time for local time zone (-12 to +12)
// Note: This is also saved/loaded from EEPROM
extern byte clockOffset;
extern byte clockOffset_prev;  // Previous clock offset for dirty tracking

// ===== FUEL TANK CAPACITY =====
extern float fuelCapacity;            // Total fuel tank capacity in gallons (used for percentage calculations)

// ===== CAN PROTOCOL SELECTION =====
// CAN protocol options for ECU communication
enum CANProtocol {
  CAN_PROTOCOL_HALTECH_V2 = 0,  // Haltech v2 protocol
  CAN_PROTOCOL_MEGASQUIRT = 1,   // Megasquirt CAN broadcast
  CAN_PROTOCOL_AIM = 2,          // AiM CAN protocol
  CAN_PROTOCOL_OBDII = 3         // OBDII polling protocol
};

extern uint8_t CAN_PROTOCOL;          // Selected CAN protocol (0=Haltech v2, 1=Megasquirt, 2=Aim, 3=OBDII)

#endif // CONFIG_CALIBRATION_H
