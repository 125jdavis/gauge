# Gauge V4 - Modularized Code Structure

This directory contains the modularized Arduino code for the vintage vehicle instrument panel controller.

## Overview

The codebase has been refactored from a single 3090-line monolithic file into a well-organized modular structure with separate header and implementation files for each functional area.

**Original:** `gauge_V4.ino` - 3090 lines  
**Modularized:** `gauge_V4.ino` - 329 lines (89% reduction)

## File Structure

### Main Entry Point
- **gauge_V4.ino** - Arduino sketch entry point containing only `setup()` and `loop()` functions with includes

### Configuration and Data Headers
- **HardwareConfig.h/.cpp** - Pin definitions, hardware object instances (CAN, GPS, displays, motors, encoder)
- **GlobalVariables.h/.cpp** - All global variables, state, lookup tables, and EEPROM addresses
- **ImageData.h/.cpp** - OLED bitmap images for splash screens and icons (8 images)

### Functional Modules

#### Utilities
- **Utils.h/.cpp** - Utility functions for sensor reading, filtering, and interpolation
  - `readSensor()` - Generic analog sensor reader with exponential filtering
  - `read30PSIAsensor()` - 30 PSI absolute pressure sensor reader
  - `readThermSensor()` - GM-style thermistor temperature sensor reader
  - `curveLookup()` - Lookup table with linear interpolation

#### Sensor Processing
- **SensorFunctions.h/.cpp** - Signal processing and unit conversion
  - `sigSelect()` - Central data router converting raw sensor/CAN data to display units

#### Communication
- **CANBus.h/.cpp** - CAN bus communication (4 functions)
  - `sendCAN_LE()` - Send Little Endian CAN messages
  - `sendCAN_BE()` - Send Big Endian CAN messages
  - `receiveCAN()` - Read CAN messages from buffer
  - `parseCAN()` - Parse Haltech ECU CAN messages

- **GPSFunctions.h/.cpp** - GPS data handling (2 functions)
  - `fetchGPSdata()` - Read and process GPS NMEA sentences
  - `useInterrupt()` - Enable/disable interrupt-based GPS reading

#### Output Control
- **MotorControl.h/.cpp** - Stepper motor control (6 functions)
  - `speedometerAngle()` - Calculate speedometer needle position
  - `fuelLvlAngle()` - Calculate fuel gauge needle position
  - `coolantTempAngle()` - Calculate temperature gauge needle position
  - `motorZeroSynchronous()` - Return all needles to zero
  - `motorSweepSynchronous()` - Full sweep test of all gauges
  - `shutdown()` - Graceful shutdown with EEPROM save

- **LEDControl.h/.cpp** - LED tachometer control (1 function)
  - `ledShiftLight()` - Update WS2812 LED strip based on RPM

#### User Interface
- **MenuSystem.h/.cpp** - Menu navigation and input (6 functions)
  - `swRead()` - Debounced rotary encoder button reading
  - `rotate()` - Interrupt handler for encoder rotation
  - `dispMenu()` - Multi-level menu controller
  - `goToLevel0()` - Return to main menu
  - `disp2()` - Handle second display menu
  - `incrementOffset()` - Adjust clock time zone offset

- **DisplayFunctions.h/.cpp** - OLED display rendering (24 functions)
  - Settings: `dispSettings()`, `dispDisp2Select()`, `dispUnits()`, `dispClockOffset()`
  - Text displays: `dispRPM()`, `dispSpd()`, `dispOilTemp()`, `dispFuelPrs()`, `dispFuelComp()`, `dispAFR()`
  - Logos: `dispFalconScript()`, `disp302CID()`, `disp302V()`
  - Graphical displays: `dispOilPrsGfx()`, `dispOilTempGfx()`, `dispCoolantTempGfx()`, `dispBattVoltGfx()`, `dispFuelLvlGfx()`
  - Trip functions: `dispTripOdo()`, `dispOdoResetYes()`, `dispOdoResetNo()`
  - Engine data: `dispIgnAng()`, `dispInjDuty()`, `dispClock()`

### Backup
- **gauge_V4.ino.backup** - Original monolithic code (preserved for reference)

## Building the Code

This modularized code is designed to be compiled with the Arduino IDE or Arduino CLI.

### Arduino IDE
1. Open `gauge_V4.ino` in the Arduino IDE
2. All .h and .cpp files in the same directory will be automatically included
3. Select board: Arduino Mega 2560
4. Compile and upload

### Required Libraries
- Adafruit_SSD1306
- Adafruit_GFX
- mcp_can
- Rotary
- FastLED
- Adafruit_GPS
- SwitecX25
- SwitecX12
- TimerOne
- EEPROM (built-in)
- SPI (built-in)
- Stepper (built-in)

## Benefits of Modularization

### Improved Readability
- Each file has a single, clear purpose
- Functions are grouped by functional area
- Much easier to navigate and understand

### Better Maintainability
- Changes to one subsystem don't affect others
- Easier to test individual components
- Clearer separation of concerns

### Enhanced Collaboration
- Multiple developers can work on different modules simultaneously
- Code reviews are more focused and effective
- Easier to onboard new contributors

### Future Extensibility
- New features can be added as new modules
- Easy to swap out implementations (e.g., different display types)
- Better support for unit testing

## Code Organization Principles

1. **Hardware abstraction** - All pin definitions and hardware objects in HardwareConfig
2. **Global state centralization** - All global variables in GlobalVariables
3. **Functional cohesion** - Related functions grouped together
4. **Clear interfaces** - Header files document all public functions
5. **Preserved documentation** - All original comments maintained or improved

## Verification

All module files have been verified for:
- ✓ Matching header/implementation pairs
- ✓ Proper include statements in main .ino file
- ✓ Correct extern declarations
- ✓ Function definitions present
- ✓ No functionality changes from original code

## Migration Notes

If you have the original gauge_V4.ino code:
1. The backup is preserved as `gauge_V4.ino.backup`
2. All functionality remains identical
3. No changes to pin assignments or hardware configuration
4. EEPROM layout unchanged

## License

[Same as original project]

## Author

Jesse Davis - Original implementation and modularization  
Date: 8/24/2024
