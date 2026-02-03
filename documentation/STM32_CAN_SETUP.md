# STM32 CAN Setup Guide

## Issue: STM32_CAN Library Compilation Errors

If you see errors like:
```
error: 'CAN_HandleTypeDef' does not name a type
error: 'CAN_MODE_NORMAL' was not declared in this scope
error: 'CAN_FILTERMODE_IDMASK' was not declared in this scope
```

This indicates the STM32 HAL CAN module is not enabled in your board configuration.

## Solution: Enable CAN Support in Arduino IDE

### Method 1: Board Menu Configuration (Recommended)

1. In Arduino IDE, go to **Tools** menu
2. Select your board: **Generic STM32F4 series** → **STM32F407VE**
3. Look for **"U(S)ART support"** and set to **"Enabled (generic 'Serial')"**
4. Look for **"USB support"** and set to **"CDC (generic 'Serial' supersede U(S)ART)"**
5. **Most Important**: Look for **"CAN support"** or similar option
   - If available, enable it
   - If not visible, you may need to update STM32duino core

### Method 2: Alternative CAN Library

If the HAL CAN support is not available or continues to have issues, you can use an alternative library:

**Option A: Use nopnop2002's Arduino-STM32-CAN library**
1. Remove the current STM32_CAN library
2. Install from: https://github.com/nopnop2002/Arduino-STM32-CAN
3. This library has better compatibility with various STM32 boards

**Option B: Use STM32duino's built-in CAN library**
1. Instead of `#include <STM32_CAN.h>`, use `#include <STM32CAN.h>` (note: no underscore)
2. Update the code to use the slightly different API

### Method 3: Check Board Variant

The STM32F407VE should have CAN support, but verify:

1. Check if you're using the correct board variant
2. In Arduino IDE, verify: **Tools → Board part number → STM32F407VE** or **STM32F407VET6**
3. Some generic STM32F4 selections may not include CAN support

### Method 4: Update STM32duino Core

An outdated STM32duino core might not expose CAN support properly:

1. Go to **Tools → Board → Boards Manager**
2. Search for "STM32"
3. Update "STM32 MCU based boards" to the latest version (2.6.0 or newer recommended)

## Verifying CAN Support

To verify CAN support is available:

1. Create a simple test sketch:
```cpp
#ifdef STM32_CORE_VERSION
  #include <STM32_CAN.h>
  
  STM32_CAN can(CAN1, DEF);
  
  void setup() {
    Serial.begin(115200);
    can.begin();
    can.setBaudRate(500000);
    Serial.println("CAN initialized!");
  }
  
  void loop() {
    // Empty
  }
#else
  void setup() {
    Serial.begin(115200);
    Serial.println("Not STM32");
  }
  void loop() {}
#endif
```

2. Compile this sketch
3. If it compiles without errors, CAN support is properly enabled

## Alternative: Compile for Arduino Mega First

If you want to verify the code logic works before dealing with STM32 CAN:

1. Select **Tools → Board → Arduino AVR Boards → Arduino Mega or Mega 2560**
2. Compile the project
3. This will use the MCP2515 CAN code path and should compile successfully
4. Then switch back to STM32 and work on the CAN library issue

## Common Fixes

### Error: "STM32_CAN.h: No such file or directory"

**Solution**: Install the STM32_CAN library
```
Sketch → Include Library → Manage Libraries
Search for: "STM32_CAN" or "pazi88"
Install the library
```

### Error: FastLED pin 71 issue

**Solution**: This is due to TACH_DATA_PIN (PE7) being pin 71, which FastLED may not support directly on STM32. Update gauge_V4.ino:

```cpp
// Before FastLED.addLeds line, add:
#ifdef STM32_CORE_VERSION
  // For STM32, use a different LED library or disable for testing
  // FastLED may have limited STM32 support
  #define FASTLED_ALL_PINS_HARDWARE_SPI
#endif
FastLED.addLeds<WS2812, TACH_DATA_PIN, GRB>(leds, NUM_LEDS);
```

Or temporarily comment out the FastLED line to test CAN compilation.

## Summary

The main issue is that STM32 CAN peripheral support needs to be explicitly enabled in the Arduino IDE board configuration. Check your board settings and ensure you have:
1. Latest STM32duino core
2. Correct board variant (STM32F407VE with CAN support)
3. CAN peripheral enabled in board configuration menus
4. Compatible STM32_CAN library installed

If problems persist, try the alternative CAN libraries mentioned above or compile for Arduino Mega first to verify the code logic.
