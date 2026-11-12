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

// ===== MOTOR 1-4 FUNCTION ASSIGNMENT =====
// Selects which signal each X25.168 gauge motor displays. Motor S is always the speedometer.
#define MOTOR_FUNC_OFF          0   // Needle parked at zero
#define MOTOR_FUNC_FUEL_LEVEL   1
#define MOTOR_FUNC_COOLANT_TEMP 2
#define MOTOR_FUNC_RPM          3
#define MOTOR_FUNC_VEHICLE_SPEED 4
#define MOTOR_FUNC_OIL_PRESSURE 5
#define MOTOR_FUNC_FUEL_PRESSURE 6
#define MOTOR_FUNC_OIL_TEMP     7
#define MOTOR_FUNC_MAP          8
#define MOTOR_FUNC_LAMBDA       9
#define MOTOR_FUNC_COUNT        10  // Number of valid function values (0..MOTOR_FUNC_COUNT-1)
extern uint8_t M1_FUNCTION;
extern uint8_t M2_FUNCTION;
extern uint8_t M3_FUNCTION;
extern uint8_t M4_FUNCTION;

// Gauge scale endpoints for motors 1-4, in the units shown by the assigned function.
// Lambda endpoints use lambda * 1000; vehicle speed uses whole km/h.
// The needle sits at its zero stop at the min value and at full sweep at the max value.
// min may be greater than max to reverse the needle's direction of travel.
extern int16_t M1_MIN;
extern int16_t M1_MAX;
extern int16_t M2_MIN;
extern int16_t M2_MAX;
extern int16_t M3_MIN;
extern int16_t M3_MAX;
extern int16_t M4_MIN;
extern int16_t M4_MAX;

// ===== MOTOR S (NEMA14 / TMC2209) ZEROING PARAMETERS =====
// motorS uses a different driver (TMC2209) and motor (NEMA14) than motors 1-4 (AX1201728SG / Switec X25.168).
// These parameters control zeroing in motorZeroSynchronous() to avoid vibration on the NEMA14.
extern uint16_t MS_ZERO_STEP_DELAY_US;  // Step delay (µs) for motorS during zeroing; bypasses SwitecX12 accel table
extern float    MS_ZERO_SWEEP_FACTOR;   // Fraction of MS_SWEEP used for motorS zeroing (0.0–1.0); reduces vibration time

// ===== MOTOR SWEEP TIMING =====
extern uint16_t MOTOR_SWEEP_TIME_MS;  // Time in milliseconds for motors to sweep full range during startup test

// ===== EMA FILTER COEFFICIENTS =====
// Retained previous-sample weight out of 128: 0 = off, 127 = heavy.
extern uint8_t FILTER_VBATT;

// Battery Voltage Sensor - Voltage divider scaling factor
extern float VBATT_SCALER;      // Formula: Vbatt = ADC_reading * (5.0/1023) * ((R1+R2)/R2)
                                    // R1=10k, R2=3.3k

extern uint8_t FILTER_FUEL;
extern uint8_t FILTER_THERM;

extern uint8_t FILTER_AV1;
extern uint8_t FILTER_AV2;
extern uint8_t FILTER_AV3;

// Analog input sensor types (AV_TYPE_*). Presets are applied by the config tool; the firmware
// maps every type except AV_TYPE_NONE with the ADC/physical calibration points below.
#define AV_TYPE_2BAR_ABS   0   // 2 bar absolute (30 PSIA), 0.5-4.5V
#define AV_TYPE_3BAR_ABS   1   // 3 bar absolute (43.5 PSIA), 0.5-4.5V
#define AV_TYPE_100PSI_G   2   // 100 PSI gauge, 0.5-4.5V
#define AV_TYPE_NONE       3   // Raw voltage (0.01V units)
#define AV_TYPE_CUSTOM     4   // User-defined calibration points
#define AV_TYPE_LAMBDA     5   // Lambda sensor; physical values are Lambda*1000
extern uint8_t AV1_TYPE;
extern uint8_t AV2_TYPE;
extern uint8_t AV3_TYPE;

// Calibration points for AV1-AV3 (index 0-2): ADC counts (0-1023) map linearly to physical values.
// Physical units: absolute kPa*10 for pressure sensors, Lambda*1000 for lambda sensors.
extern uint16_t AV_ADC_LO[3];
extern uint16_t AV_ADC_HI[3];
extern int16_t  AV_PHYS_LO[3];
extern int16_t  AV_PHYS_HI[3];

// ===== HALL EFFECT SPEED SENSOR PARAMETERS =====
extern uint16_t REVS_PER_KM;        // Revolutions per kilometer (vehicle-specific)
extern uint8_t TEETH_PER_REV;         // Teeth per revolution (sensor-specific)
extern uint8_t FILTER_HALL_SPEED;
extern uint8_t FILTER_GPS;
extern uint8_t HALL_SPEED_MIN;      // Minimum reportable speed in km/h*100 (e.g., 50 = 0.5 km/h)

// ===== ENGINE RPM SENSOR PARAMETERS =====
// Cylinder count (for 4-stroke engines)
// Examples: 4-cyl=4, 6-cyl=6, 8-cyl=8, 3-cyl=3
extern uint8_t CYL_COUNT;

extern uint8_t FILTER_ENGINE_RPM;

// Debounce window in microseconds: pulses arriving sooner than this after the last accepted
// pulse are rejected as coil-ringdown echoes. Default 5000 µs (5 ms) suits most engines
// (covers echoes up to ~3000 RPM on a V8). Reduce for high-revving engines if needed.
extern uint16_t RPM_DEBOUNCE_MICROS;

// Minimum reportable RPM (engine idle ~600-800)
extern uint8_t ENGINE_RPM_MIN;

// ===== SPEEDOMETER CALIBRATION =====
extern uint16_t SPEEDO_MAX;    // Maximum speedometer reading: 100 mph * 100 (stored as integer for precision)

// ===== LED TACHOMETER CONFIGURATION =====
extern uint8_t NUM_LEDS;              // Total number of LEDs in the tachometer strip
extern uint8_t WARN_LEDS;              // Warning start: steps before the final step
extern uint8_t SHIFT_LEDS;             // Shift start: steps before the final step
extern uint8_t TACH_SWEEP;             // 0=left-right, 1=right-left, 2=edge-center, 3=center-edge
extern uint32_t TACH_NORMAL_COLOR;
extern uint32_t TACH_WARNING_COLOR;
extern uint32_t TACH_SHIFT_COLOR;
extern unsigned int TACH_MAX; // RPM at which the last tach step illuminates
extern unsigned int TACH_FLASH; // RPM at which illuminated shift LEDs flash
extern unsigned int TACH_MIN;       // Minimum RPM to show on tach (below this LEDs are off)

// ===== SPEEDOMETER BACKLIGHT =====
// When enabled, the LED tachometer strip provides warm white (3500K) backlight to the speedometer
// at 25% brightness whenever LEDs would otherwise be off (RPM below TACH_MIN or blacked-out pairs).
// Set to 1 to enable, 0 to disable.
extern uint8_t SPEEDO_BACKLIGHT_ENABLED;  // Speedometer backlight: 0=disabled, 1=enabled

// ===== ODOMETER MOTOR CALIBRATION =====
extern uint16_t ODO_STEPS;             // Steps per revolution for odometer motor
extern uint8_t ODO_MOTOR_TEETH;       // Number of teeth on motor gear
extern uint8_t ODO_GEAR_TEETH;        // Number of teeth on odometer gear

// ===== SIGNAL SOURCE SELECTION =====
// These parameters determine which sensor/source to use for each signal
// This allows flexible configuration of data sources

// Speed source for both speedometer and odometer: 0=off, 1=CAN, 2=Hall sensor, 3=GPS, 4=Synthetic (debug), 5=Odometer test (1-mile profile), 6=Serial (set via serial commands)
extern uint8_t SPEED_SOURCE;

// Engine RPM source: 0=off, 1=CAN, 2=coil negative, 3=synthetic (debug), 4=Serial (set via serial commands)
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

// Fuel level source: 0=off, 1=analog sensor, 2=Synthetic (debug)
extern uint8_t FUEL_LVL_SOURCE;

// ===== FAULT WARNING THRESHOLDS =====
// Thresholds that trigger flashing display to alert the driver
extern float OIL_PRS_WARN_THRESHOLD;      // Oil pressure warning (kPa gauge) - flash when below this while engine running
extern float COOLANT_TEMP_WARN_THRESHOLD; // Coolant temperature warning (°C) - flash when above this while engine running
extern float BATT_VOLT_WARN_THRESHOLD;    // Battery voltage warning (V) - flash when below this
extern int   ENGINE_RUNNING_RPM_MIN;      // Minimum RPM to consider the engine running (RPM)
extern uint8_t FUEL_LVL_WARN_THRESHOLD_PCT; // Low fuel warning (%) - flash when fuel level drops below this

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

// ===== THERMISTOR / FUEL SENDER CURVE SETUP =====
// The PC tool converts resistances to voltages (5V reference, bias resistor to +5V, sensor to ground).
// The firmware only uses thermTable_x/thermTable_l (mV -> degC) and FUEL_V_EMPTY_MV/FUEL_V_FULL_MV;
// the type, bias and resistance values are stored so the tool can show them again after a read.
#define THERM_TYPE_GM_CLT    0
#define THERM_TYPE_BOSCH_M12 1
#define THERM_TYPE_CUSTOM    2
#define FUEL_TYPE_CUSTOM     6   // 0-5 are presets (Ford/GM/Mopar), listed in the PC tool
extern uint8_t  THERM_TYPE;
extern uint16_t THERM_BIAS_OHMS;
extern uint32_t THERM_R_OHMS[6];   // Resistance at each thermTable_l temperature
extern uint8_t  FUEL_SENDER_TYPE;
extern uint16_t FUEL_BIAS_OHMS;
extern uint16_t FUEL_R_EMPTY_OHMS;
extern uint16_t FUEL_R_MID_OHMS;
extern uint16_t FUEL_R_FULL_OHMS;
extern uint8_t  FUEL_MID_PCT;      // Fuel level (%) at the mid point
extern uint16_t FUEL_V_EMPTY_MV;   // Sender voltage at empty (linear interpolation to full)
extern uint16_t FUEL_V_MID_MV;
extern uint16_t FUEL_V_FULL_MV;
#endif // CONFIG_CALIBRATION_H
