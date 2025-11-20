# Coding Style Guide for Gauge Project

This document defines the coding conventions and style guidelines for the gauge project. Following these guidelines ensures consistency, readability, and maintainability across the codebase.

## Table of Contents
- [Naming Conventions](#naming-conventions)
- [Constants and Macros](#constants-and-macros)
- [Type Definitions](#type-definitions)
- [Functions](#functions)
- [Variables](#variables)
- [Code Organization](#code-organization)
- [Comments and Documentation](#comments-and-documentation)

## Naming Conventions

### Constants (UPPER_SNAKE_CASE)
Use `UPPER_SNAKE_CASE` for all compile-time constants and calibration parameters, including:
- Hardware pin assignments
- Calibration parameters (even if they're regular variables, not constexpr)
- Configuration values
- PROGMEM arrays (images, lookup tables)

**Examples:**
```cpp
constexpr uint8_t CAN0_CS = 53;          // Hardware constant (constexpr)
uint16_t SPEEDO_MAX = 100 * 100;         // Calibration parameter (regular variable)
uint8_t NUM_LEDS = 26;                   // Calibration parameter (regular variable)
const unsigned char IMG_FALCON_SCRIPT[] PROGMEM = { ... };
```

**Filter Naming Conventions:**
- `FILTER_*` - Simple filter coefficients (e.g., FILTER_VBATT, FILTER_FUEL, FILTER_THERM)
- `ALPHA_*` - Exponential Moving Average (EMA) filter coefficients (e.g., ALPHA_HALL_SPEED, ALPHA_ENGINE_RPM)

Both FILTER_* and ALPHA_* should be calibration parameters (regular variables, not constexpr) so they can be tuned by the user.

### Functions (lowerCamelCase)
Use `lowerCamelCase` for all function names.

**Examples:**
```cpp
void fetchGPSdata();
int speedometerAngle(int sweep);
void motorZeroSynchronous(void);
```

### Variables (lowerCamelCase)
Use `lowerCamelCase` for all variable names, both global and local.

**Examples:**
```cpp
float oilPrs = 25;
int coolantTemp = 0;
unsigned int timer0;
byte dispArray1[4] = {1, 0, 0, 0};
```

### Types (PascalCase)
Use `PascalCase` for user-defined types (structs, classes, enums).

**Examples:**
```cpp
struct SensorConfig {
    uint8_t pin;
    float scaleFactor;
};

class MotorController {
    // ...
};
```

### Enum Classes (PascalCase for type, UPPER_SNAKE_CASE for values)
Use `enum class` for strongly-typed enumerations. The enum type should be in `PascalCase`, and values in `UPPER_SNAKE_CASE`.

**Examples:**
```cpp
enum class DisplayMode {
    SPEEDOMETER,
    FUEL_LEVEL,
    COOLANT_TEMP,
    OIL_PRESSURE
};

enum class UnitSystem {
    METRIC,
    IMPERIAL
};
```

## Constants and Macros

### Avoid #define for Constants
**DO NOT USE** `#define` for constants that can be expressed as typed variables. Use `constexpr` or `const` instead for type safety and better debugging.

**❌ Bad:**
```cpp
#define CAN0_CS 53
#define M1_SWEEP (58*12)
#define NUM_LEDS 26
```

**✅ Good:**
```cpp
constexpr uint8_t CAN0_CS = 53;
constexpr uint16_t M1_SWEEP = 58 * 12;
constexpr uint8_t NUM_LEDS = 26;
```

### When to Use #define
`#define` macros are acceptable for:
- Include guards
- Conditional compilation flags
- Function-like macros (use sparingly; prefer inline functions)

**Examples:**
```cpp
#ifndef GAUGE_V4_H
#define GAUGE_V4_H

#ifdef DEBUG_MODE
  #define DEBUG_PRINT(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
#endif
```

### Choosing Between constexpr, const, and Regular Variables

**Use `constexpr`** for:
- Hardware pin numbers (never change)
- Hardware-defined constants (screen dimensions, fixed buffer sizes)
- True compile-time constants that will never be modified

```cpp
constexpr uint8_t PWR_PIN = 49;          // Hardware pin - never changes
constexpr uint8_t SCREEN_W = 128;        // Hardware spec - fixed
constexpr uint8_t TACH_DATA_PIN = 22;    // Hardware connection
```

**Use regular variables (non-const)** for:
- **Calibration parameters** that will be user-configurable via serial/EEPROM
- Values that need to be modified at runtime
- Tunable parameters (filter coefficients, thresholds, sweep ranges, timing values)

```cpp
uint16_t M1_SWEEP = 58 * 12;             // Calibration parameter - user adjustable
uint8_t FILTER_VBATT = 8;                // Filter coefficient - tunable
float ALPHA_HALL_SPEED = 0.8;            // EMA filter - calibratable
unsigned int TACH_MAX = 6000;            // Shift point - user configurable
```

**Use `const`** for:
- PROGMEM arrays (due to Arduino compatibility)
- Values that are initialized once at startup but not known at compile time
- Read-only data structures

```cpp
const unsigned char IMG_FALCON_SCRIPT[] PROGMEM = { ... };
```

**Important**: Even though calibration parameters use UPPER_SNAKE_CASE like constants, they should NOT be `constexpr` if they will be made user-configurable in the future. Using `constexpr` prevents runtime modification.

## Type Definitions

### Prefer Specific Integer Types
Use sized integer types from `<stdint.h>` for clarity and portability:
- `uint8_t` / `int8_t` for bytes (0-255 / -128 to 127)
- `uint16_t` / `int16_t` for words (0-65535 / -32768 to 32767)
- `uint32_t` / `int32_t` for double words
- `bool` for boolean values

**❌ Bad:**
```cpp
int ledCount = 26;        // Wastes memory (int is 16-bit on AVR)
unsigned char myByte;     // Less clear than uint8_t
```

**✅ Good:**
```cpp
uint8_t ledCount = 26;
uint8_t myByte;
```

## Functions

### Function Naming
- Use descriptive, verb-based names
- Start with a verb (get, set, calculate, read, write, etc.)
- Use lowerCamelCase

**Examples:**
```cpp
void readSensors();
int calculateMotorAngle(int value, int maxValue);
bool isEngineRunning();
```

### Function Documentation
Document all non-trivial functions with block comments explaining:
- Purpose
- Parameters
- Return value
- Side effects
- Calling context

**Example:**
```cpp
/**
 * speedometerAngle - Calculate speedometer needle angle from GPS speed
 * 
 * Interpolates between GPS updates for smooth needle movement and converts
 * speed to motor steps. Includes clamping and dead zone logic.
 * 
 * @param sweep - Maximum motor steps for full gauge sweep (e.g., 1416 for M3)
 * @return Motor angle in steps (1 to sweep-1)
 * 
 * Algorithm:
 * 1. Interpolate speed between GPS updates (5Hz to ~50Hz for smooth motion)
 * 2. Convert km/h to mph (* 0.6213712)
 * 3. Apply dead zone (< 0.5 mph reads as 0)
 * 4. Clamp to maximum (100 mph)
 * 5. Map speed to motor angle (0-100 mph -> 1 to sweep-1 steps)
 */
int speedometerAngle(int sweep) {
    // Implementation...
}
```

## Variables

### Variable Scope
- Minimize scope: declare variables as close to their use as possible
- Use local static variables instead of globals when appropriate
- Group related variables together

**Example:**
```cpp
// ❌ Bad: Global variable that could be local
int tempCounter = 0;

void myFunction() {
    tempCounter++;
    // use tempCounter
}

// ✅ Good: Local static variable
void myFunction() {
    static int tempCounter = 0;
    tempCounter++;
    // use tempCounter
}
```

### Initialization
Always initialize variables when declaring them:

```cpp
int count = 0;
float voltage = 12.0;
bool isActive = false;
```

## Code Organization

### File Structure
Organize the .ino file in this order:

1. **File header comment** - Description, author, date
2. **Library includes** - Grouped by function (display, communication, etc.)
3. **Hardware configuration** - Grouped by subsystem:
   - CAN bus hardware
   - Engine sensors
   - Power control
   - Stepper motors
   - Rotary encoder
   - OLED displays
   - LED tachometer
   - GPS configuration
   - Calibration constants
4. **Hardware object initialization** - Create instances of hardware objects
5. **Global variables** - Grouped by function with clear section headers
6. **PROGMEM data** - Images, lookup tables
7. **Function implementations** - Grouped by purpose with section headers

### Section Headers
Use clear section headers to organize code:

```cpp
///// HARDWARE OBJECT INITIALIZATION /////

// ===== CAN BUS HARDWARE =====

/*
 * ========================================
 * STEPPER MOTOR FUNCTIONS
 * ========================================
 */
```

### Logical Grouping
Group related constants together with comments:

```cpp
// ===== STEPPER MOTOR HARDWARE =====
constexpr uint8_t MOTOR_RST = 36;   // Shared reset pin

// Motor 1 Configuration (fuel level gauge)
constexpr uint16_t M1_SWEEP = 58 * 12;
constexpr uint8_t M1_STEP = 37;
constexpr uint8_t M1_DIR = 38;

// Motor 2 Configuration (coolant temp gauge)
constexpr uint16_t M2_SWEEP = 58 * 12;
constexpr uint8_t M2_STEP = 34;
constexpr uint8_t M2_DIR = 35;
```

## Comments and Documentation

### When to Comment
- **DO** comment complex algorithms and non-obvious code
- **DO** document function purposes and parameters
- **DO** explain hardware connections and calibration values
- **DO NOT** state the obvious or repeat what the code already says

**❌ Bad:**
```cpp
i++;  // Increment i
```

**✅ Good:**
```cpp
// Interpolate speed between GPS updates for smooth speedometer motion
float spd_g_float = map(t_curr, t_old, t_new, v_old, v_new) * 0.6213712;
```

### Inline Comments
- Place inline comments on their own line above the code they describe
- Use `//` for single-line comments
- Use `/* */` for multi-line comments

### Function Documentation
Use Doxygen-style comments for functions:

```cpp
/**
 * @brief Brief description
 * 
 * Detailed description
 * 
 * @param paramName Description of parameter
 * @return Description of return value
 */
```

### TODO Comments
Mark incomplete work or future improvements:

```cpp
// TODO: Add error handling for CAN bus timeout
// FIXME: Memory leak in display update function
```

## Formatting

### Indentation
- Use 2 spaces for indentation (matching existing Arduino IDE style)
- No tabs

### Braces
Use K&R style (opening brace on same line):

```cpp
void myFunction() {
    if (condition) {
        // code
    } else {
        // code
    }
}
```

### Whitespace
- One space after keywords: `if (`, `while (`, `for (`
- One space around operators: `a + b`, `x = y`
- No space before semicolon
- Blank lines to separate logical blocks

### Line Length
- Aim for 80-100 characters per line
- Break long lines at logical points

## Arduino-Specific Considerations

### PROGMEM Arrays
Use `const` with `PROGMEM` for arrays stored in flash:

```cpp
const unsigned char IMG_LOGO[] PROGMEM = {
    0x00, 0x01, 0x02, ...
};
```

### ISR Functions
Clearly mark interrupt service routines:

```cpp
/**
 * TIMER0_COMPA_vect - Timer0 compare interrupt for GPS reading
 * 
 * ISR called once per millisecond to read GPS data.
 */
SIGNAL(TIMER0_COMPA_vect) {
    char c = GPS.read();
}
```

### Pin Modes
Always use `constexpr` for pin numbers:

```cpp
constexpr uint8_t LED_PIN = 13;
pinMode(LED_PIN, OUTPUT);
```

## Version Control

### Commit Messages
Write clear, concise commit messages:
- Start with a verb (Add, Fix, Update, Refactor)
- Keep first line under 72 characters
- Add detailed explanation in body if needed

**Examples:**
```
Fix EEPROM read loop bug in setup()

Update motor pin definitions to use constexpr

Refactor display functions to use consistent naming
```

## Summary

Following these guidelines will help maintain a clean, consistent, and maintainable codebase. Key principles:

1. **Type safety**: Use `constexpr`/`const` instead of `#define` for constants
2. **Consistency**: Follow naming conventions religiously
3. **Clarity**: Write self-documenting code with clear names and minimal necessary comments
4. **Organization**: Group related code together with clear section headers
5. **Simplicity**: Keep functions focused and variables scoped appropriately

When in doubt, refer to existing well-written code in the project as examples.
