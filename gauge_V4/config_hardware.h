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
 * TARGET HARDWARE: STM32F407 (pazi88 MEGA F407 board)
 * - Native CAN controller (CAN1 on PA11/PA12)
 * - 12-bit ADC (0-4095) with 3.3V reference
 * - Analog inputs use 9.1k/4.7k voltage divider (5V -> 3.3V)
 * - UART3 for GPS (PB10/PB11)
 */

#ifndef CONFIG_HARDWARE_H
#define CONFIG_HARDWARE_H

#include <Arduino.h>

// ===== CAN BUS HARDWARE =====
// STM32F407 uses native CAN controller (not MCP2515)
constexpr uint8_t CAN_TX_PIN = PA12;  // CAN1 TX pin
constexpr uint8_t CAN_RX_PIN = PA11;  // CAN1 RX pin

// ===== ENGINE RPM SENSOR =====
constexpr uint8_t IGNITION_PULSE_PIN = PB9;  // STM32 pin PB9 - ignition coil pulses (was D53 on Mega)

// ===== POWER CONTROL =====
constexpr uint8_t PWR_PIN = PD4;     // STM32 pin PD4 - Power control (was D18 on Mega)

// ===== STEPPER MOTOR HARDWARE =====
constexpr uint8_t MOTOR_RST = PE10;   // STM32 pin PE10 - Motor reset (was D42 on Mega)

// Motor 1 Configuration (typically fuel level gauge)
constexpr uint8_t M1_STEP = PD12;     // STM32 pin PD12 - Motor 1 step (was D32 on Mega)
constexpr uint8_t M1_DIR = PE15;      // STM32 pin PE15 - Motor 1 direction (was D35 on Mega)

// Motor 2 Configuration (typically coolant temp or secondary gauge)
constexpr uint8_t M2_STEP = PE11;     // STM32 pin PE11 - Motor 2 step (was D45 on Mega)
constexpr uint8_t M2_DIR = PE12;      // STM32 pin PE12 - Motor 2 direction (was D41 on Mega)

// Motor 3 Configuration (typically speedometer - note larger sweep angle)
constexpr uint8_t M3_STEP = PE8;      // STM32 pin PE8 - Motor 3 step (was D49 on Mega)
constexpr uint8_t M3_DIR = PE9;       // STM32 pin PE9 - Motor 3 direction (was D47 on Mega)

// Motor 4 Configuration (typically fuel level or coolant temp)
constexpr uint8_t M4_STEP = PE14;     // STM32 pin PE14 - Motor 4 step (was D37 on Mega)
constexpr uint8_t M4_DIR = PE13;      // STM32 pin PE13 - Motor 4 direction (was D39 on Mega)

// Motor S Configuration (speedometer - 16 microsteps, 400 steps/rev, 0.9° per step)
constexpr uint8_t MS_STEP = PE2;      // STM32 pin PE2 - Motor S step (was D40 on Mega)
constexpr uint8_t MS_DIR = PE3;       // STM32 pin PE3 - Motor S direction (was D38 on Mega)
constexpr uint8_t MS_M1 = PE4;        // STM32 pin PE4 - Motor S M1 microstep (was D36 on Mega)
constexpr uint8_t MS_M2 = PE5;        // STM32 pin PE5 - Motor S M2 microstep (was D34 on Mega)

// ===== ROTARY ENCODER =====
constexpr uint8_t ROTARY_DT = PD7;    // STM32 pin PD7 - Rotary encoder DT (was D2 on Mega)
constexpr uint8_t ROTARY_CLK = PB6;   // STM32 pin PB6 - Rotary encoder CLK (was D3 on Mega)
constexpr uint8_t SWITCH = PD5;       // STM32 pin PD5 - Rotary encoder button (was D14 on Mega)

// ===== OLED DISPLAY HARDWARE =====
constexpr uint8_t SCREEN_W = 128;   // OLED display width in pixels
constexpr uint8_t SCREEN_H = 32;    // OLED display height in pixels

// SPI pins for displays (shared)
constexpr uint8_t OLED_SCK = PB13;   // STM32 pin PB13 - SPI SCK (was D11 on Mega)
constexpr uint8_t OLED_MISO = PB14;  // STM32 pin PB14 - SPI MISO (was D9 on Mega) 
constexpr uint8_t OLED_MOSI = PB15;  // STM32 pin PB15 - SPI MOSI (was D8 on Mega)

// Display 1 Configuration (SPI interface)
constexpr uint8_t OLED_DC_1 = PB12;  // STM32 pin PB12 - Display 1 Data/Command (was D10 on Mega)
constexpr uint8_t OLED_CS_1 = PA8;   // STM32 pin PA8 - Display 1 Chip Select (was D12 on Mega)
constexpr uint8_t OLED_RST_1 = PD8;  // STM32 pin PD8 - Display 1 Reset (was D7 on Mega)

// Display 2 Configuration (SPI interface)
constexpr uint8_t OLED_DC_2 = PD10;  // STM32 pin PD10 - Display 2 Data/Command (was D5 on Mega)
constexpr uint8_t OLED_CS_2 = PD9;   // STM32 pin PD9 - Display 2 Chip Select (was D6 on Mega)
constexpr uint8_t OLED_RST_2 = PD11; // STM32 pin PD11 - Display 2 Reset (was D4 on Mega)

// ===== LED TACHOMETER HARDWARE =====
constexpr uint8_t MAX_LEDS = 64;         // Maximum number of LEDs supported by the array
constexpr uint8_t TACH_DATA_PIN = PC6;   // STM32 pin PC6 - WS2812 data (was D50 on Mega, swapped with ODO_PIN4 for NeoPixel compatibility)

// ===== GPS CONFIGURATION =====
constexpr bool GPSECHO = false;          // Set to true to echo raw GPS data to serial monitor
constexpr uint8_t GPS_TX = PB10;         // STM32 pin PB10 - GPS TX (board RX, was D16 on Mega)
constexpr uint8_t GPS_RX = PB11;         // STM32 pin PB11 - GPS RX (board TX, was D17 on Mega)

// ===== ANALOG SENSOR INPUT PINS =====
// STM32F407 ADC: 12-bit (0-4095), 3.3V reference
// All analog inputs use 9.1k/4.7k voltage divider (R2=9.1k, R1=4.7k)
// Divider ratio: 4.7/(4.7+9.1) = 0.3406 (reduces 5V to 1.703V for 3.3V ADC)
// To get original voltage: Vin = ADC_value * (3.3/4095) * (1/0.3406)

// Battery Voltage Sensor (Analog Pin A0)
constexpr uint8_t VBATT_PIN = PA0;   // STM32 pin PA0 (was A0 on Mega)

// Fuel Level Sensor (Analog Pin A3)
constexpr uint8_t FUEL_PIN = PA3;    // STM32 pin PA3 (was A3 on Mega)

// Coolant/Oil Temperature Thermistor Sensor (Analog Pin A4)
constexpr uint8_t THERM_PIN = PA4;   // STM32 pin PA4 (was A4 on Mega)

// Analog Inputs for 0-5V sensors
constexpr uint8_t PIN_AV1 = PA5;     // STM32 pin PA5 - Analog sensor 1 (was A5 on Mega)
constexpr uint8_t PIN_AV2 = PA6;     // STM32 pin PA6 - Analog sensor 2 (was A6 on Mega)
constexpr uint8_t PIN_AV3 = PA7;     // STM32 pin PA7 - Analog sensor 3 (was A7 on Mega)

// ===== HALL EFFECT SPEED SENSOR =====
constexpr uint8_t HALL_PIN = PD3;    // STM32 pin PD3 - Hall speed sensor (was D19 on Mega)

// ===== HALL EFFECT SPEED SENSOR TIMEOUT =====
constexpr unsigned long HALL_PULSE_TIMEOUT = 1000000UL; // Timeout (μs) for "vehicle stopped" (1 second)
constexpr unsigned long MAX_VALID_PULSE_INTERVAL = 500000UL; // Max pulse interval (μs) to accept (0.5 sec, ~0.5 km/h min)
constexpr unsigned long SPEED_DECAY_THRESHOLD = 200000UL; // Time (μs) before speed starts decaying (200ms)
constexpr uint8_t SPEED_DECAY_FACTOR = 230;  // Speed decay multiplier (230/256 ≈ 0.9, or 10% decay per cycle)
constexpr uint8_t PULSES_TO_SKIP_AFTER_STANDSTILL = 2;  // Number of initial pulses to skip after standstill

// ===== ENGINE RPM SENSOR TIMEOUT =====
constexpr unsigned long IGNITION_PULSE_TIMEOUT = 500000UL; // Timeout (μs) for "engine stopped" (0.5 second)

// ===== ODOMETER MOTOR HARDWARE =====
constexpr uint8_t ODO_PIN1 = PD13;   // STM32 pin PD13 - Odometer motor coil 1 (was D33 on Mega)
constexpr uint8_t ODO_PIN2 = PD14;   // STM32 pin PD14 - Odometer motor coil 2 (was D30 on Mega)
constexpr uint8_t ODO_PIN3 = PD15;   // STM32 pin PD15 - Odometer motor coil 3 (was D31 on Mega)
constexpr uint8_t ODO_PIN4 = PC7;    // STM32 pin PC7 - Odometer motor coil 4 (was D28 on Mega, swapped with TACH_DATA_PIN)

// ===== ADDITIONAL OUTPUT =====
constexpr uint8_t LS_OUTPUT = PE7;   // STM32 pin PE7 - LS output (was D26 on Mega)

// ===== STM32 ADC CONFIGURATION =====
// STM32F407 ADC specifications
constexpr uint16_t ADC_MAX_VALUE = 4095;  // 12-bit ADC (0-4095)
constexpr float ADC_VREF = 3.3;            // 3.3V reference voltage
constexpr float ADC_VOLTAGE_DIVIDER_R1 = 4.7;  // 4.7k ohms (top resistor)
constexpr float ADC_VOLTAGE_DIVIDER_R2 = 9.1;  // 9.1k ohms (bottom resistor)
// Voltage divider ratio: R1/(R1+R2) = 4.7/13.8 = 0.3406
constexpr float ADC_VOLTAGE_DIVIDER_RATIO = ADC_VOLTAGE_DIVIDER_R1 / (ADC_VOLTAGE_DIVIDER_R1 + ADC_VOLTAGE_DIVIDER_R2);
// Conversion factor: ADC value to original input voltage (before divider)
// Vin = ADC_reading * (ADC_VREF / ADC_MAX_VALUE) * (1 / ADC_VOLTAGE_DIVIDER_RATIO)
constexpr float ADC_TO_VOLTAGE = (ADC_VREF / ADC_MAX_VALUE) / ADC_VOLTAGE_DIVIDER_RATIO;

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
// STM32F407 uses hardware timer (TIM2) for precise motor update intervals
// TIM2 is a 32-bit timer suitable for precise frequency control
constexpr uint32_t MOTOR_UPDATE_FREQ_HZ = 10000;  // Target frequency: 10 kHz (100 µs period)
                                                    // This frequency ensures:
                                                    // - Steps don't accumulate delays at max motor speed
                                                    // - Overhead is reasonable (~10-20% CPU at 10kHz with 5 motors)
                                                    // - Compatible with SwitecX12 microDelay (min 90 µs)

#endif // CONFIG_HARDWARE_H
