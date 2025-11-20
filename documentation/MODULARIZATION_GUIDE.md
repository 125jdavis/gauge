# Modularization Guide

This guide explains how to complete the modularization of gauge_V4.ino.

## Module Structure

The code has been organized into the following modules:

### Configuration Files
- **config_hardware.h** - Hardware pin definitions (constexpr, never change)
- **config_calibration.h** - Calibration parameters (tunable at runtime)

### Core Modules  
- **globals.h** - Global variable declarations
- **gps.h/cpp** - GPS functions (✓ Complete)
- **can.h/cpp** - CAN bus functions (✓ Complete)
- **sensors.h/cpp** - Sensor reading functions (✓ Complete)
- **display.h/cpp** - OLED display functions (⚠ Headers only)
- **outputs.h/cpp** - Motor and LED control (⚠ Headers only)
- **menu.h/cpp** - Menu navigation (⚠ Headers only)
- **utilities.h/cpp** - Utilities and helpers (⚠ Headers only)
- **image_data.h/cpp** - OLED image bitmaps (✓ Complete)

## Remaining Work

### 1. Extract display.cpp functions from gauge_V4.ino

Functions to extract (lines 1904-2526 in original):
- dispSettings, dispDisp2Select, dispUnits, dispClockOffset
- dispRPM, dispSpd, dispOilTemp, dispFuelPrs, dispFuelComp, dispAFR
- dispFalconScript, disp302CID, disp302V
- dispOilPrsGfx, dispOilTempGfx, dispCoolantTempGfx, dispBattVoltGfx, dispFuelLvlGfx
- dispTripOdo, dispOdoResetYes, dispOdoResetNo
- dispIgnAng, dispInjDuty, dispClock
- digits (helper function)
- dispMenu (large menu controller function, lines 1374-1792)
- disp2 (display 2 controller, lines 1795-1838)

### 2. Extract outputs.cpp functions from gauge_V4.ino

Functions to extract (lines 2746-3219 in original):
- ledShiftLight (lines 2747-2806)
- speedometerAngle (lines 2967-2981)
- speedometerAngleGPS (lines 2983-2993)
- speedometerAngleCAN (lines 2995-2999)
- speedometerAngleHall (lines 3001-3005)
- fuelLvlAngle (lines 3007-3024)
- coolantTempAngle (lines 3026-3053)
- motorZeroSynchronous (lines 3137-3163)
- motorSweepSynchronous (lines 3181-3219)

### 3. Extract menu.cpp functions from gauge_V4.ino

Functions to extract (lines 1277-1873 in original):
- swRead (lines 1277-1309)
- rotate (lines 1326-1342)
- incrementOffset (lines 1856-1873)

### 4. Extract utilities.cpp functions from gauge_V4.ino

Functions to extract (lines 3085-3307 in original):
- shutdown (lines 3085-3118)
- generateRPM (lines 3245-3272)
- serialInputFunc (lines 3289-3307)

### 5. Create global variable definitions file

Create **globals.cpp** with actual variable definitions (not just extern declarations).
Variables are defined in lines 162-375 of original file.

Include:
- Lookup tables (thermTable_x, thermTable_l, fuelLvlTable_x, fuelLvlTable_l)
- All sensor readings
- Hardware object instances
- Timing variables
- CAN bus variables
- Menu variables
- EEPROM addresses

### 6. Update gauge_V4.ino

The main .ino file should become minimal:
1. Include all module headers
2. Define hardware object instances (or move to globals.cpp)
3. setup() function
4. loop() function

## Compilation Steps

After completing extraction:

1. Verify all #include statements are correct
2. Ensure all extern declarations in globals.h match definitions in globals.cpp
3. Test compilation in Arduino IDE
4. Fix any missing includes or forward declarations
5. Test on hardware (if available)

## Notes

- All functions follow STYLE.md naming conventions
- Hardware constants use constexpr
- Calibration parameters are non-const for future EEPROM tuning
- Image data uses PROGMEM for flash storage
- Interrupt handlers are marked appropriately

## Function Extraction Template

```cpp
// In module.cpp:
#include "module.h"
#include "globals.h"
// Add other includes as needed

void functionName() {
    // Copy function body from original gauge_V4.ino
}
```

