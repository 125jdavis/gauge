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
 * 
 * TARGET PLATFORM: STM32F407 (pazi88/STM32_mega board)
 * - Uses STM32's native CAN controller (no external MCP2515)
 * - Pin mappings follow STM32F407 port/pin naming convention
 */

#ifndef CONFIG_HARDWARE_H
#define CONFIG_HARDWARE_H

#include <Arduino.h>

// ===== CAN BUS HARDWARE =====
// STM32F407 has built-in CAN controller - no external chip needed
constexpr uint8_t CAN_TX = PA12;    // STM32 CAN TX pin
constexpr uint8_t CAN_RX = PA11;    // STM32 CAN RX pin

// ===== ENGINE RPM SENSOR =====
constexpr uint8_t IGNITION_PULSE_PIN = PB9;  // STM32 pin PB9 - ignition coil pulses via optocoupler (same as COIL_NEG)

// ===== POWER CONTROL =====
constexpr uint8_t PWR_PIN = PD4;     // Power control pin - keeps system alive after ignition is off

// ===== STEPPER MOTOR HARDWARE =====
constexpr uint8_t MOTOR_RST = PE10;  // Stepper motor driver reset pin - shared by all motor drivers

// Motor 1 Configuration (typically fuel level gauge)
constexpr uint8_t M1_STEP = PD12;    // Motor 1 step pulse pin
constexpr uint8_t M1_DIR = PE15;     // Motor 1 direction control pin

// Motor 2 Configuration (typically coolant temp or secondary gauge)
constexpr uint8_t M2_STEP = PE11;    // Motor 2 step pulse pin
constexpr uint8_t M2_DIR = PE12;     // Motor 2 direction control pin

// Motor 3 Configuration (typically speedometer - note larger sweep angle)
constexpr uint8_t M3_STEP = PE8;     // Motor 3 step pulse pin
constexpr uint8_t M3_DIR = PE9;      // Motor 3 direction control pin

// Motor 4 Configuration (typically fuel level or coolant temp)
constexpr uint8_t M4_STEP = PE14;    // Motor 4 step pulse pin
constexpr uint8_t M4_DIR = PE13;     // Motor 4 direction control pin

// Motor S Configuration (speedometer - 16 microsteps, 400 steps/rev, 0.9° per step)
constexpr uint8_t MS_STEP = PE2;     // Motor S step pulse pin
constexpr uint8_t MS_DIR = PE3;      // Motor S direction control pin
constexpr uint8_t MS_M1 = PE4;       // Motor S microstep control M1
constexpr uint8_t MS_M2 = PE5;       // Motor S microstep control M2

// ===== ROTARY ENCODER =====
constexpr uint8_t ROTARY_DT = PD7;   // Rotary encoder DT pin (data)
constexpr uint8_t ROTARY_CLK = PB6;  // Rotary encoder CLK pin (clock)
constexpr uint8_t SWITCH = PD5;      // Rotary encoder push button pin

// ===== OLED DISPLAY HARDWARE =====
constexpr uint8_t SCREEN_W = 128;   // OLED display width in pixels
constexpr uint8_t SCREEN_H = 32;    // OLED display height in pixels

// SPI pins for STM32F407
constexpr uint8_t SPI_SCK = PB13;   // SPI clock
constexpr uint8_t SPI_MISO = PB14;  // SPI MISO
constexpr uint8_t SPI_MOSI = PB15;  // SPI MOSI

// Display 1 Configuration (SPI interface)
constexpr uint8_t OLED_DC_1 = PB12;  // Display 1 Data/Command pin
constexpr uint8_t OLED_CS_1 = PA8;   // Display 1 Chip Select pin
constexpr uint8_t OLED_RST_1 = PD8;  // Display 1 Reset pin

// Display 2 Configuration (SPI interface)
constexpr uint8_t OLED_DC_2 = PD10;  // Display 2 Data/Command pin
constexpr uint8_t OLED_CS_2 = PD9;   // Display 2 Chip Select pin
constexpr uint8_t OLED_RST_2 = PD11; // Display 2 Reset pin

// ===== LED TACHOMETER HARDWARE =====
constexpr uint8_t MAX_LEDS = 64;    // Maximum number of LEDs supported by the array (must be compile-time constant)
constexpr uint8_t TACH_DATA_PIN = PE7; // WS2812 data pin for LED tachometer strip

// ===== GPS CONFIGURATION =====
constexpr bool GPSECHO = false;     // Set to true to echo raw GPS data to serial monitor (debug only)
constexpr uint8_t GPS_TX = PB10;    // STM32 UART RX pin (board receives from GPS TX)
constexpr uint8_t GPS_RX = PB11;    // STM32 UART TX pin (board transmits to GPS RX)

// ===== ANALOG SENSOR INPUT PINS =====
// STM32F407 has 12-bit ADC (0-4095) vs Arduino Mega 10-bit (0-1023)
// Battery Voltage Sensor
constexpr uint8_t VBATT_PIN = PA0;   // Analog input pin for battery voltage

// Fuel Level Sensor
constexpr uint8_t FUEL_PIN = PA3;    // Analog input pin for fuel level sensor

// Coolant/Oil Temperature Thermistor Sensor
constexpr uint8_t THERM_PIN = PA4;   // Analog input pin for thermistor

// Analog Inputs for 0-5V sensors
constexpr uint8_t PIN_AV1 = PA5;     // Analog pin (barometric pressure sensor)
constexpr uint8_t PIN_AV2 = PA6;     // Analog pin (reserved for future sensor)
constexpr uint8_t PIN_AV3 = PA7;     // Analog pin (reserved for future sensor)

// ===== HALL EFFECT SPEED SENSOR =====
constexpr uint8_t HALL_PIN = PD3;    // Digital speed input pin (interrupt-capable)

// ===== HALL EFFECT SPEED SENSOR TIMEOUT =====
constexpr unsigned long HALL_PULSE_TIMEOUT = 1000000UL; // Timeout (μs) for "vehicle stopped" (1 second)
constexpr unsigned long MAX_VALID_PULSE_INTERVAL = 500000UL; // Max pulse interval (μs) to accept (0.5 sec, ~0.5 km/h min)
constexpr unsigned long SPEED_DECAY_THRESHOLD = 200000UL; // Time (μs) before speed starts decaying (200ms)
constexpr uint8_t SPEED_DECAY_FACTOR = 230;  // Speed decay multiplier (230/256 ≈ 0.9, or 10% decay per cycle)
constexpr uint8_t PULSES_TO_SKIP_AFTER_STANDSTILL = 2;  // Number of initial pulses to skip after standstill

// ===== ENGINE RPM SENSOR TIMEOUT =====
constexpr unsigned long IGNITION_PULSE_TIMEOUT = 500000UL; // Timeout (μs) for "engine stopped" (0.5 second)

// ===== ODOMETER MOTOR HARDWARE =====
constexpr uint8_t ODO_PIN1 = PD13;   // Odometer motor coil 1 pin
constexpr uint8_t ODO_PIN2 = PD14;   // Odometer motor coil 2 pin
constexpr uint8_t ODO_PIN3 = PD15;   // Odometer motor coil 3 pin
constexpr uint8_t ODO_PIN4 = PC6;    // Odometer motor coil 4 pin

// ===== LIGHT/OUTPUT CONTROL =====
constexpr uint8_t LS_OUTPUT = PC7;   // Light/signal output control pin
constexpr uint8_t COIL_NEG = PB9;    // Coil negative control pin

// ===== TIMING CONSTANTS =====
// Update rate periods (in milliseconds)
constexpr unsigned int CAN_SEND_RATE = 50;        // Send CAN messages every 50ms (20Hz)
constexpr unsigned int DISP_UPDATE_RATE = 100;     // Update displays every 100ms (~10Hz)
constexpr unsigned int SENSOR_READ_RATE = 20;     // Read analog sensors every 20ms (50Hz for responsive readings)
constexpr unsigned int TACH_UPDATE_RATE = 50;     // Update LED tachometer every 50ms (20Hz)
constexpr unsigned int TACH_FLASH_RATE = 50;      // Flash shift light every 50ms when over redline
constexpr unsigned int SIG_SELECT_UPDATE_RATE = 10; // Update signal selection/synthetic generators every 10ms (100Hz)
constexpr unsigned int GPS_UPDATE_RATE = 100;     // GPS update check rate (might not be needed)
constexpr unsigned int CHECK_GPS_RATE = 1;        // Check for GPS data every 1ms
constexpr unsigned int ANGLE_UPDATE_RATE = 20;    // Update motor angles every 20ms (50Hz)
constexpr unsigned int SPLASH_TIME = 1500;        // Duration of startup splash screens (milliseconds)
constexpr unsigned int HALL_UPDATE_RATE = 20;     // Recalculate Hall sensor speed every 20ms (50Hz)
constexpr unsigned int ENGINE_RPM_UPDATE_RATE = 20; // Check engine RPM timeout every 20ms (50Hz)

// ===== MOTOR UPDATE TIMER CONFIGURATION =====
// Timer-based motor stepping for smooth, deterministic motion
// Uses STM32 Hardware Timer for precise motor update intervals
// STM32F407 has multiple hardware timers available
// Timer is configured for precise frequency control independent of other functions
constexpr uint32_t MOTOR_UPDATE_FREQ_HZ = 10000;  // Target frequency: 10 kHz (100 µs period)
                                                    // This frequency ensures:
                                                    // - Steps don't accumulate delays at max motor speed
                                                    // - Overhead is reasonable (~10-20% CPU at 10kHz with 5 motors)
                                                    // - Compatible with SwitecX12 microDelay (min 90 µs)

#endif // CONFIG_HARDWARE_H
