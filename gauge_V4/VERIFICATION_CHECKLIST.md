# Verification Checklist

## Pre-Compilation Checks

### File Structure
- [x] All header files (.h) created
- [x] All implementation files (.cpp) created
- [x] Main gauge_V4.ino updated
- [x] Original backed up as gauge_V4_monolithic_backup.ino

### Module Completeness
- [x] config_hardware.h - All hardware pins defined
- [x] config_calibration.h - All calibration parameters
- [x] globals.h - All extern declarations
- [x] globals.cpp - All variable definitions
- [x] gps.h/cpp - GPS functions complete
- [x] can.h/cpp - CAN functions complete
- [x] sensors.h/cpp - Sensor functions complete
- [x] display.h/cpp - Display functions complete
- [x] image_data.h/cpp - Image bitmaps complete
- [x] outputs.h/cpp - Motor/LED functions complete
- [x] menu.h/cpp - Menu functions complete
- [x] utilities.h/cpp - Utility functions complete

### Code Style Compliance
- [x] Hardware constants use constexpr with UPPER_SNAKE_CASE
- [x] Calibration parameters use UPPER_SNAKE_CASE (non-const)
- [x] Functions use lowerCamelCase
- [x] Variables use lowerCamelCase
- [x] Includes use quotes for local, angle brackets for libraries
- [x] Comments preserved from original

### Dependencies
- [x] All #include statements present
- [x] Forward declarations where needed
- [x] No circular dependencies
- [x] Proper include guards in headers

## Compilation Checklist (To be done in Arduino IDE)

### Step 1: Open Project
- [ ] Open gauge_V4.ino in Arduino IDE
- [ ] Verify all module files appear in tabs
- [ ] Check Tools > Board is set to Arduino Mega 2560

### Step 2: Verify Library Dependencies
- [ ] Adafruit_SSD1306 library installed
- [ ] Adafruit_GFX library installed
- [ ] mcp_can library installed
- [ ] Rotary library installed
- [ ] FastLED library installed
- [ ] Adafruit_GPS library installed
- [ ] SwitecX25 library installed
- [ ] SwitecX12 library installed

### Step 3: Compile
- [ ] Click Verify/Compile button
- [ ] Check for compilation errors
- [ ] Fix any errors found
- [ ] Verify binary size fits in memory

### Step 4: Common Issues to Watch For

1. **Missing lookup table definitions in globals.cpp**
   - Check thermTable_x, thermTable_l arrays
   - Check fuelLvlTable_x, fuelLvlTable_l arrays

2. **Extern vs Definition Mismatches**
   - Every extern in globals.h must have definition in globals.cpp
   - Check variable types match exactly

3. **Include Order Issues**
   - config files should be included before globals
   - globals should be included before other modules

4. **PROGMEM Variables**
   - Image arrays must have both extern and const unsigned char
   - Verify PROGMEM keyword is present

5. **Hardware Object Initialization**
   - Motor objects need sweep parameters from config_calibration.h
   - Verify constructors have correct pin assignments

### Step 5: Upload and Test (If Hardware Available)
- [ ] Upload to Arduino Mega
- [ ] Verify startup splash screens appear
- [ ] Test gauge sweep on startup
- [ ] Test CAN bus communication
- [ ] Test GPS acquisition
- [ ] Test menu navigation
- [ ] Test sensor readings
- [ ] Test motor control
- [ ] Test LED tachometer
- [ ] Test shutdown sequence

## Known Potential Issues

1. **Lookup Tables:** The thermistor and fuel level lookup tables need to be defined in globals.cpp with actual values from the original file (lines 343-352 of original).

2. **Hardware Objects:** Some hardware objects may need to be extern in globals.h and defined in globals.cpp to resolve initialization order issues.

3. **Static Variables:** Functions with static local variables (swRead, generateRPM, ledShiftLight) should work correctly as they're in the .cpp files.

## Quick Fixes

If compilation fails:

1. Check the error message for which variable/function is undefined
2. Find it in the original gauge_V4_monolithic_backup.ino
3. Add the definition to the appropriate module
4. Ensure proper includes

## Success Criteria

- [ ] Compiles without errors
- [ ] Compiles without warnings (or only expected warnings)
- [ ] Binary size similar to original (~100-120KB)
- [ ] All functionality works on hardware
- [ ] EEPROM data loads correctly
- [ ] Settings persist across power cycles

