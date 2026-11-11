/*
 * ========================================
 * CALIBRATION PARAMETERS DEFINITIONS
 * ========================================
 */

#include "config_calibration.h"

// ===== STEPPER MOTOR SWEEP RANGES =====
uint16_t M1_SWEEP = 58 * 12;        // Motor 1: 58 degrees * 3 steps/degree * 4 microsteps/step = 696 steps
uint16_t M2_SWEEP = 58 * 12;        // Motor 2: 58 degrees * 3 steps/degree * 4 microsteps/step = 696 steps
uint16_t M3_SWEEP = 58 * 12;        // Motor 3: 58 degrees * 3 steps/degree * 4 microsteps/step = 696 steps (same config as motor1)
uint16_t M4_SWEEP = 58 * 12;        // Motor 4: 58 degrees * 3 steps/degree * 4 microsteps/step = 696 steps
uint16_t MS_SWEEP = 8064;           // Motor S: (118° / 0.9°) * 32 microsteps = 4195.555 ≈ 4196 steps (speedometer)

// ===== MOTOR 1-4 FUNCTION ASSIGNMENT =====
// 0=Off, 1=Fuel level, 2=Coolant temp, 3=RPM, 4=vehicle speed,
// 5=oil pressure, 6=fuel pressure, 7=oil temp, 8=MAP, 9=Lambda
uint8_t M1_FUNCTION = MOTOR_FUNC_FUEL_LEVEL;
uint8_t M2_FUNCTION = MOTOR_FUNC_COOLANT_TEMP;
uint8_t M3_FUNCTION = MOTOR_FUNC_FUEL_LEVEL;
uint8_t M4_FUNCTION = MOTOR_FUNC_COOLANT_TEMP;

// ===== MOTOR 1-4 GAUGE SCALE (units follow the assigned function) =====
// Scale endpoints use the assigned function's units; Lambda is stored as Lambda * 1000.
int16_t M1_MIN = 10;
int16_t M1_MAX = 100;
int16_t M2_MIN = 50;
int16_t M2_MAX = 115;
int16_t M3_MIN = 10;
int16_t M3_MAX = 100;
int16_t M4_MIN = 50;
int16_t M4_MAX = 115;

// ===== MOTOR S (NEMA14 / TMC2209) ZEROING PARAMETERS =====
// 500 µs/step = 2000 steps/sec — a smooth, controlled rate for the NEMA14 during zeroing.
// The SwitecX12 accel table peaks at ~90 µs/step (11 111 steps/sec); this default is slower
// and steadier, eliminating acceleration-driven vibration.  Decrease toward 50 µs to go
// faster than the table max; increase to 1000+ µs for an even slower, quieter sweep.
uint16_t MS_ZERO_STEP_DELAY_US = 20;
// 0.5 = sweep back 50% of MS_SWEEP during zeroing (reduces vibration time at zero stop).
// Assumes needle is near zero at startup; increase toward 1.0 if needle could be far from zero.
float    MS_ZERO_SWEEP_FACTOR  = 0.25f;

// ===== MOTOR SWEEP TIMING =====
uint16_t MOTOR_SWEEP_TIME_MS = 1000;  // Time in milliseconds for motors to sweep full range during startup test

// ===== ANALOG SENSOR FILTER COEFFICIENTS =====
uint8_t FILTER_VBATT = 112;
float VBATT_SCALER = 0.040923;      // Voltage divider scaling factor
uint8_t FILTER_FUEL = 126;
uint8_t FILTER_THERM = 64;
uint8_t FILTER_AV1 = 96;
uint8_t FILTER_AV2 = 32;
uint8_t FILTER_AV3 = 32;
uint8_t AV1_TYPE = AV_TYPE_100PSI_G;  // AV1 sensor type (AV_TYPE_*)
uint8_t AV2_TYPE = AV_TYPE_100PSI_G;
uint8_t AV3_TYPE = AV_TYPE_100PSI_G;
// Default: 100 PSIG sensor, 0.5-4.5V -> 101.3-790.8 kPa absolute (kPa*10)
uint16_t AV_ADC_LO[3]  = {102, 102, 102};
uint16_t AV_ADC_HI[3]  = {921, 921, 921};
int16_t  AV_PHYS_LO[3] = {1013, 1013, 1013};
int16_t  AV_PHYS_HI[3] = {7908, 7908, 7908};

// ===== HALL EFFECT SPEED SENSOR PARAMETERS =====
uint16_t REVS_PER_KM = 1625;        // Revolutions of VSS per kilometer (8000 pulses per mile is standard)
uint8_t TEETH_PER_REV = 8;         // Teeth per revolution of the VSS
uint8_t FILTER_HALL_SPEED = 96;
uint8_t FILTER_GPS = 0;
uint8_t HALL_SPEED_MIN = 50;        // Minimum reportable speed in km/h*100 (50 = 0.5 km/h)

// ===== ENGINE RPM SENSOR PARAMETERS =====
uint8_t CYL_COUNT = 8;              // Cylinder count (8 = 2x old PULSES_PER_REVOLUTION of 4.0)
uint8_t FILTER_ENGINE_RPM = 39;
uint16_t RPM_DEBOUNCE_MICROS = 5000; // Debounce window: reject pulses within 5 ms of last accepted pulse
uint8_t ENGINE_RPM_MIN = 100;       // Minimum reportable RPM

// ===== SPEEDOMETER CALIBRATION =====
uint16_t SPEEDO_MAX = 100 * 100;    // Maximum speedometer reading

// ===== LED TACHOMETER CONFIGURATION =====
uint8_t NUM_LEDS = 26;              // Total number of LEDs
uint8_t WARN_LEDS = 6;              // Warning zone LEDs
uint8_t SHIFT_LEDS = 2;             // Shift light LEDs
uint8_t TACH_SWEEP = 2;             // Edge to center
uint32_t TACH_NORMAL_COLOR = 0x1E0F00;
uint32_t TACH_WARNING_COLOR = 0x500A00;
uint32_t TACH_SHIFT_COLOR = 0x500000;
unsigned int TACH_MAX = 6000; // RPM at the final LED step
unsigned int TACH_FLASH = 6000; // RPM at which the shift zone flashes
unsigned int TACH_MIN = 3000;       // Minimum RPM to show

// ===== SPEEDOMETER BACKLIGHT =====
// Warm white 3500K at 25% brightness using the LED tachometer strip.
// Set to 1 to enable backlight, 0 to disable.
uint8_t SPEEDO_BACKLIGHT_ENABLED = 1;  // 0=disabled, 1=enabled

// ===== ODOMETER MOTOR CALIBRATION =====
// 28BYJ-48 stepper motor step count depends on drive mode:
//   Half-step mode : 4096 steps per revolution (NOT used here)
//   Full-step / wave drive: 2048 steps per revolution (THIS is what the driver uses —
//     one coil at a time, 4-state sequence = full-step equivalent)
// ODO_STEPS = 4096 caused the odometer to register 2× the expected distance because
// each actual revolution only consumes 2048 steps, not 4096.
uint16_t ODO_STEPS = 2048;          // Steps per revolution (wave drive / full-step mode)
uint8_t ODO_MOTOR_TEETH = 16;       // Number of teeth on motor gear
uint8_t ODO_GEAR_TEETH = 20;        // Number of teeth on odometer gear

// ===== SIGNAL SOURCE SELECTION =====
uint8_t SPEED_SOURCE = 1;           // 0=off, 1=CAN, 2=Hall sensor, 3=GPS, 4=Synthetic (debug), 5=Odometer test (1-mile profile), 6=Serial (set via serial commands)
uint8_t RPM_SOURCE = 1;             // 0=off, 1=CAN, 2=coil negative, 3=Synthetic (debug), 4=Serial (set via serial commands)
uint8_t OIL_PRS_SOURCE = 1;         // 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3, 5=Synthetic (debug)
uint8_t FUEL_PRS_SOURCE = 1;        // 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3, 5=Synthetic (debug)
uint8_t COOLANT_TEMP_SOURCE = 1;    // 0=off, 1=CAN, 2=therm, 3=Synthetic (debug)
uint8_t OIL_TEMP_SOURCE = 1;        // 0=off, 1=CAN, 2=therm (default to therm sensor)
uint8_t MAP_SOURCE = 1;             // 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3, 5=Synthetic (debug)
uint8_t LAMBDA_SOURCE = 1;          // 0=off, 1=CAN, 2=sensor_av1, 3=sensor_av2, 4=sensor_av3
uint8_t FUEL_LVL_SOURCE = 1;       // 0=off, 1=analog sensor, 2=Synthetic (debug)

// ===== FAULT WARNING THRESHOLDS =====
float OIL_PRS_WARN_THRESHOLD      = 60.0;   // Oil pressure warning: flash below 60 kPa (gauge) while engine running
float COOLANT_TEMP_WARN_THRESHOLD = 110.0;  // Coolant temperature warning: flash above 110 °C while engine running
float BATT_VOLT_WARN_THRESHOLD    = 11.0;   // Battery voltage warning: flash below 11 V
int   ENGINE_RUNNING_RPM_MIN      = 400;    // Engine considered running above 400 RPM
uint8_t FUEL_LVL_WARN_THRESHOLD_PCT = 5;   // Low fuel warning: flash below 5% fuel level

// ===== TIME ZONE OFFSET =====
byte clockOffset = 0;               // Hours to add to UTC time
byte clockOffset_prev = 0;          // Previous clock offset for dirty tracking

// ===== FUEL TANK CAPACITY =====
float fuelCapacity = 16;            // Total fuel tank capacity in gallons

// ===== CAN PROTOCOL SELECTION =====
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_HALTECH_V2;  // Default to Haltech v2 protocol
