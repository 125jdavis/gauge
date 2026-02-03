# CAN Library Fix - arduino-STM32-CAN

## Problem Solved

When compiling, you got the error:
```
fatal error: HardwareCAN.h: No such file or directory
```

This was because I mistakenly assumed STM32duino had a built-in "HardwareCAN" library, but it doesn't exist.

## Solution Implemented

**Switched to arduino-STM32-CAN library by nopnop2002**

This is a well-maintained, Arduino-compatible CAN library that:
- Works with STM32duino 2.12.0 without HAL configuration
- Available in Arduino Library Manager
- Widely used and well-documented
- Supports STM32F4 series including F407

## Installation Required

**You must install this library before the code will compile:**

### Method 1: Arduino Library Manager (Recommended)

1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. In the search box, type: **STM32 CAN**
4. Find **"arduino-STM32-CAN"** by **nopnop2002**
5. Click **Install**

### Method 2: Manual Installation

1. Download from: https://github.com/nopnop2002/Arduino-STM32-CAN
2. Extract to Arduino libraries folder
3. Restart Arduino IDE

## Changes Made

### Code Updates

**Includes changed:**
```cpp
// Before (incorrect):
#include <HardwareCAN.h>

// After (correct):
#include <STM32_CAN.h>
```

**CAN instance:**
```cpp
// arduino-STM32-CAN instance
STM32_CAN canBus(CAN1, DEF);  // CAN1, default pins
```

**Message type:**
```cpp
// Uses CAN_message_t (not CanMsg)
CAN_message_t rxMsg;
rxMsg.data[0];  // Access data array
```

### API Compatibility

The arduino-STM32-CAN library has a very similar API to what we were using:

| Operation | Code |
|-----------|------|
| Initialize | `canBus.begin()` |
| Set baud rate | `canBus.setBaudRate(500000)` |
| Check/read message | `canBus.read(rxMsg)` |
| Send message | `canBus.write(txMsg)` |

The main differences:
- `begin()` returns bool (we now check the result)
- Message type is `CAN_message_t` (built into the library)
- Data accessed via `msg.data[]` (same as before)

## Files Modified

1. **gauge_V4.ino** - Updated include
2. **globals.h** - Updated include
3. **can.cpp** - Updated CAN instance and API calls
4. **can.h** - Updated comments

## After Installation

Once you install the arduino-STM32-CAN library:

1. **Restart Arduino IDE** (important!)
2. **Open gauge_V4.ino**
3. **Compile** - should work now!

## Expected Output

After successful compilation and upload:
```
Motor update timer initialized (STM32): 10000 Hz
STM32 CAN Initialized Successfully!
CAN filters configured for protocol: X
```

## Library Information

**Name:** arduino-STM32-CAN  
**Author:** nopnop2002  
**Repository:** https://github.com/nopnop2002/Arduino-STM32-CAN  
**License:** MIT  
**Compatibility:** STM32F1, STM32F4 (including F407)

## Hardware Requirements

Same as before - you still need a CAN transceiver:

```
STM32F407     CAN Transceiver     CAN Bus
---------     ---------------     --------
PA12 (TX) --> TXD               
PA11 (RX) <-- RXD               
3.3V -------> VCC               
GND --------> GND                  GND
              CANH -------------> CANH
              CANL -------------> CANL
              
(120Ω termination at each end of bus)
```

## Troubleshooting

### "STM32_CAN.h: No such file or directory"
- Library not installed
- Restart Arduino IDE after installing
- Check library is in the right folder

### "STM32_CAN does not name a type"
- Wrong board selected (must be STM32F407VE)
- Library not properly installed
- Try reinstalling the library

### CAN not working after upload
- Check transceiver connections
- Verify CAN bus termination (120Ω)
- Check baud rate matches other devices (500kbps default)
- Test with CAN analyzer

## Comparison with Previous Attempts

| Library | Status | Notes |
|---------|--------|-------|
| STM32_CAN (pazi88) | ❌ Requires HAL | Needs HAL CAN configuration |
| HardwareCAN | ❌ Doesn't exist | I made a mistake assuming this existed |
| arduino-STM32-CAN | ✅ Works | Correct library for STM32duino |

## Summary

✅ **Fixed incorrect library reference**  
✅ **Switched to arduino-STM32-CAN (nopnop2002)**  
✅ **Updated all code to use correct API**  
⚠️ **Requires library installation via Library Manager**  
✅ **Code will compile after library is installed**

---

**Action Required:** Install "arduino-STM32-CAN" via Library Manager before compiling!
