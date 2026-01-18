/*
 * ========================================
 * HARDWARE CONFIGURATION
 * ========================================
 * 
 * Hardware pin definitions and fixed hardware constants
 * These values are determined by physical connections and should not change
 * unless the hardware is modified.
 * 
 * All pin assignments use constexpr for compile-time constants
 * Following STYLE.md: hardware pins use UPPER_SNAKE_CASE with constexpr
 */

#ifndef CONFIG_HARDWARE_H
#define CONFIG_HARDWARE_H

#include <Arduino.h>

// ===== CAN BUS HARDWARE =====
constexpr uint8_t CAN0_CS = 53;     // MCP2515 CAN controller chip select pin (SPI)
constexpr uint8_t CAN0_INT = 18;    // MCP2515 interrupt pin - triggers when CAN message received

// ===== ENGINE RPM SENSOR =====
constexpr uint8_t IGNITION_PULSE_PIN = 21;  // Digital pin D21 - ignition coil pulses via optocoupler (interrupt-capable)

// ===== POWER CONTROL =====
constexpr uint8_t PWR_PIN = 49;     // Power control pin - keeps system alive after ignition is off

// ===== STEPPER MOTOR HARDWARE =====
constexpr uint8_t MOTOR_RST = 36;   // Stepper motor driver reset pin - shared by all motor drivers

// Motor 1 Configuration (typically fuel level gauge)
constexpr uint8_t M1_STEP = 37;     // Motor 1 step pulse pin
constexpr uint8_t M1_DIR = 38;      // Motor 1 direction control pin

// Motor 2 Configuration (typically coolant temp or secondary gauge)
constexpr uint8_t M2_STEP = 34;     // Motor 2 step pulse pin
constexpr uint8_t M2_DIR = 35;      // Motor 2 direction control pin

// Motor 3 Configuration (typically speedometer - note larger sweep angle)
constexpr uint8_t M3_STEP = 33;     // Motor 3 step pulse pin
constexpr uint8_t M3_DIR = 32;      // Motor 3 direction control pin

// Motor 4 Configuration (typically fuel level or coolant temp)
constexpr uint8_t M4_STEP = 40;     // Motor 4 step pulse pin
constexpr uint8_t M4_DIR = 41;      // Motor 4 direction control pin

// Motor S Configuration (speedometer - 16 microsteps, 400 steps/rev, 0.9° per step)
constexpr uint8_t MS_STEP = 45;     // Motor S step pulse pin
constexpr uint8_t MS_DIR = 47;      // Motor S direction control pin

// ===== ROTARY ENCODER =====
constexpr uint8_t SWITCH = 1;       // Rotary encoder push button pin (V4 hardware uses pin 1, V3 used pin 24)

// ===== OLED DISPLAY HARDWARE =====
constexpr uint8_t SCREEN_W = 128;   // OLED display width in pixels
constexpr uint8_t SCREEN_H = 32;    // OLED display height in pixels

// Display 1 Configuration (SPI interface)
constexpr uint8_t OLED_DC_1 = 6;    // Display 1 Data/Command pin
constexpr uint8_t OLED_CS_1 = 5;    // Display 1 Chip Select pin
constexpr uint8_t OLED_RST_1 = 7;   // Display 1 Reset pin

// Display 2 Configuration (SPI interface)
constexpr uint8_t OLED_DC_2 = 28;   // Display 2 Data/Command pin
constexpr uint8_t OLED_CS_2 = 29;   // Display 2 Chip Select pin
constexpr uint8_t OLED_RST_2 = 26;  // Display 2 Reset pin

// ===== LED TACHOMETER HARDWARE =====
constexpr uint8_t MAX_LEDS = 64;    // Maximum number of LEDs supported by the array (must be compile-time constant)
constexpr uint8_t TACH_DATA_PIN = 22; // WS2812 data pin for LED tachometer strip

// ===== GPS CONFIGURATION =====
constexpr bool GPSECHO = false;     // Set to true to echo raw GPS data to serial monitor (debug only)

// ===== ANALOG SENSOR INPUT PINS =====
// Battery Voltage Sensor (Analog Pin A0)
constexpr uint8_t VBATT_PIN = A0;   // Analog input pin for battery voltage

// Fuel Level Sensor (Analog Pin A3)
constexpr uint8_t FUEL_PIN = A3;    // Analog input pin for fuel level sensor

// Coolant/Oil Temperature Thermistor Sensor (Analog Pin A4)
constexpr uint8_t THERM_PIN = A4;   // Analog input pin for thermistor

// Analog Inputs for 0-5V sensors
constexpr uint8_t PIN_AV1 = A5;     // Analog pin 5 (barometric pressure sensor)
constexpr uint8_t PIN_AV2 = A6;     // Analog pin 6 (reserved for future sensor)
constexpr uint8_t PIN_AV3 = A7;     // Analog pin 7 (reserved for future sensor)

// ===== HALL EFFECT SPEED SENSOR =====
constexpr uint8_t HALL_PIN = 20;    // Digital speed input pin (D20, interrupt 1)

// ===== HALL EFFECT SPEED SENSOR TIMEOUT =====
constexpr unsigned long HALL_PULSE_TIMEOUT = 1000000UL; // Timeout (μs) for "vehicle stopped" (1 second)
constexpr unsigned long MAX_VALID_PULSE_INTERVAL = 500000UL; // Max pulse interval (μs) to accept (0.5 sec, ~0.5 km/h min)
constexpr unsigned long SPEED_DECAY_THRESHOLD = 200000UL; // Time (μs) before speed starts decaying (200ms)
constexpr uint8_t SPEED_DECAY_FACTOR = 230;  // Speed decay multiplier (230/256 ≈ 0.9, or 10% decay per cycle)
constexpr uint8_t PULSES_TO_SKIP_AFTER_STANDSTILL = 2;  // Number of initial pulses to skip after standstill

// ===== ENGINE RPM SENSOR TIMEOUT =====
constexpr unsigned long IGNITION_PULSE_TIMEOUT = 500000UL; // Timeout (μs) for "engine stopped" (0.5 second)

// ===== ODOMETER MOTOR HARDWARE =====
constexpr uint8_t ODO_PIN1 = 10;    // Odometer motor coil 1 pin
constexpr uint8_t ODO_PIN2 = 11;    // Odometer motor coil 2 pin
constexpr uint8_t ODO_PIN3 = 12;    // Odometer motor coil 3 pin
constexpr uint8_t ODO_PIN4 = 13;    // Odometer motor coil 4 pin

// ===== TIMING CONSTANTS =====
// Update rate periods (in milliseconds)
constexpr unsigned int CAN_SEND_RATE = 50;        // Send CAN messages every 50ms (20Hz)
constexpr unsigned int DISP_UPDATE_RATE = 75;     // Update displays every 75ms (~13Hz)
constexpr unsigned int SENSOR_READ_RATE = 10;     // Read analog sensors every 10ms (100Hz for responsive readings)
constexpr unsigned int TACH_UPDATE_RATE = 50;     // Update LED tachometer every 50ms (20Hz)
constexpr unsigned int TACH_FLASH_RATE = 50;      // Flash shift light every 50ms when over redline
constexpr unsigned int GPS_UPDATE_RATE = 100;     // GPS update check rate (might not be needed)
constexpr unsigned int CHECK_GPS_RATE = 1;        // Check for GPS data every 1ms
constexpr unsigned int ANGLE_UPDATE_RATE = 20;    // Update motor angles every 20ms (50Hz)
constexpr unsigned int SPLASH_TIME = 1500;        // Duration of startup splash screens (milliseconds)
constexpr unsigned int HALL_UPDATE_RATE = 20;     // Recalculate Hall sensor speed every 20ms (50Hz)
constexpr unsigned int ENGINE_RPM_UPDATE_RATE = 20; // Check engine RPM timeout every 20ms (50Hz)

// ===== MOTOR UPDATE TIMER CONFIGURATION =====
// Timer-based motor stepping for smooth, deterministic motion
// Uses Timer3 (16-bit) on Arduino Mega 2560 for precise motor update intervals
// Timer3 is chosen because:
// - Timer0 is used for millis() and GPS reading
// - Timer1 may be used for PWM or other functions
// - Timer3 is a 16-bit timer suitable for precise frequency control
// - Timer3 is independent and doesn't conflict with existing ISRs
constexpr uint32_t MOTOR_UPDATE_FREQ_HZ = 10000;  // Target frequency: 10 kHz (100 µs period)
                                                    // This frequency ensures:
                                                    // - Steps don't accumulate delays at max motor speed
                                                    // - Overhead is reasonable (~1% CPU at 10kHz with 5 motors)
                                                    // - Compatible with SwitecX12 microDelay (min 90 µs)

#endif // CONFIG_HARDWARE_H
