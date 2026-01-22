# MCP2515 Hardware CAN Filtering

## Overview

The gauge system now implements MCP2515 hardware-level CAN message filtering to significantly reduce MCU processing overhead. Only CAN messages relevant to the selected protocol will trigger interrupts and be processed by the microcontroller.

## What is Hardware Filtering?

The MCP2515 CAN controller has built-in acceptance filters and masks that can reject unwanted CAN messages at the hardware level, before they ever reach the Arduino. This is much more efficient than software filtering.

### Hardware Capabilities

**MCP2515 Filtering Architecture:**
- 2 receive buffers (RXB0 and RXB1)
- 6 acceptance filters total:
  - 2 filters for RXB0 (Filter 0, Filter 1)
  - 4 filters for RXB1 (Filter 2, Filter 3, Filter 4, Filter 5)
- 2 acceptance masks:
  - Mask 0 for RXB0
  - Mask 1 for RXB1

### How Masks Work

Masks specify which bits of the CAN ID should be checked:
- **Mask bit = 1**: This bit must match the filter
- **Mask bit = 0**: This bit is "don't care" (any value accepted)

**Example:**
```
Filter: 0x360 = 0b0011 0110 0000
Mask:   0x7F0 = 0b0111 1111 0000

Result: Accepts IDs 0x360-0x36F (last 4 bits are "don't care")
```

## Filter Configurations by Protocol

### Haltech v2 Protocol

**Message IDs:** 0x301, 0x360-0x362, 0x368-0x369, 0x3E0-0x3E1, 0x470-0x473

**Filter Setup:**
- **RXB0** (Mask 0x7F0):
  - Filter 0: 0x360 → accepts 0x360-0x36F
  - Filter 1: 0x3E0 → accepts 0x3E0-0x3EF
  
- **RXB1** (Mask 0x7F0):
  - Filter 2: 0x470 → accepts 0x470-0x47F (wheel speeds)
  - Filter 3: 0x300 → accepts 0x300-0x30F (test messages)
  - Filter 4: 0x360 → redundant for reliability
  - Filter 5: 0x3E0 → redundant for reliability

**Coverage:** All Haltech protocol messages are accepted.

---

### Megasquirt Protocol

**Message IDs:** 0x5EC, 0x5F0-0x5F4

**Filter Setup:**
- **RXB0** (Mask 0x7F0):
  - Filter 0: 0x5E0 → accepts 0x5E0-0x5EF (includes VSS1 at 0x5EC)
  - Filter 1: 0x5F0 → accepts 0x5F0-0x5FF
  
- **RXB1** (Mask 0x7F0):
  - Filter 2: 0x5F0 → accepts 0x5F0-0x5FF
  - Filter 3: 0x5F0 → redundant for reliability
  - Filter 4: 0x5E0 → accepts 0x5E0-0x5EF
  - Filter 5: 0x5E0 → redundant for reliability

**Coverage:** All Megasquirt protocol messages are accepted.

---

### AiM Protocol

**Message IDs:** 0x0B0-0x0B3

**Filter Setup:**
- **RXB0** (Mask 0x7F0):
  - Filter 0: 0x0B0 → accepts 0x0B0-0x0BF
  - Filter 1: 0x0B0 → redundant for reliability
  
- **RXB1** (Mask 0x7F0):
  - Filter 2-5: 0x0B0 → all accept 0x0B0-0x0BF (high reliability)

**Coverage:** All AiM protocol messages are accepted. This protocol has the most compact ID range.

---

### OBDII Protocol

**Message IDs:** 0x7E8-0x7EF (response IDs)

**Filter Setup:**
- **RXB0** (Mask 0x7F8):
  - Filter 0: 0x7E8 → accepts 0x7E8-0x7EF
  - Filter 1: 0x7E8 → redundant for reliability
  
- **RXB1** (Mask 0x7F8):
  - Filter 2-5: 0x7E8 → all accept 0x7E8-0x7EF (high reliability)

**Coverage:** All OBDII response messages are accepted. The tighter mask (0x7F8) accepts exactly 8 IDs.

---

## Performance Benefits

### Without Hardware Filtering
1. All CAN messages on the bus trigger an interrupt
2. MCU wakes up for every message
3. Software checks if message is relevant
4. Most messages are discarded (wasted CPU cycles)

**Example:** On a busy CAN bus with 100 messages/second, if only 10 are relevant, the MCU processes 90 unnecessary interrupts.

### With Hardware Filtering
1. Only relevant CAN messages trigger interrupts
2. MCU only wakes up for messages it needs
3. All received messages are processed (no waste)
4. Significant reduction in CPU overhead

**Example:** Same bus, only 10 interrupts/second instead of 100. **90% reduction in interrupt load!**

### Real-World Impact

**Typical CAN Bus Load:**
- Modern ECU: 50-200 messages/second
- Full vehicle bus: 500-2000 messages/second

**Gauge Requirements:**
- Haltech: ~12 message IDs
- Megasquirt: ~6 message IDs
- AiM: ~4 message IDs
- OBDII: ~8 response IDs

**Result:** Filtering can reduce interrupt load by 95-99% on busy buses!

## How It Works

### Initialization Sequence

1. **CAN Controller Initialization:**
   ```cpp
   CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)
   ```

2. **Configure Filters (before setting mode):**
   ```cpp
   configureCANFilters();  // Sets up protocol-specific filters
   ```

3. **Activate Normal Mode:**
   ```cpp
   CAN0.setMode(MCP_NORMAL);
   ```

### Filter Configuration Process

The `configureCANFilters()` function:
1. Reads `CAN_PROTOCOL` variable
2. Sets masks for each receive buffer
3. Configures filters for relevant message IDs
4. Uses redundant filters for reliability

### Automatic Configuration

Filters are automatically configured based on the `CAN_PROTOCOL` setting in `config_calibration.cpp`. No manual configuration needed!

```cpp
// In config_calibration.cpp
uint8_t CAN_PROTOCOL = CAN_PROTOCOL_HALTECH_V2;  // Filters auto-configured
```

## Troubleshooting

### No CAN Messages Received

**Symptom:** No data after changing protocol

**Causes:**
1. Filters configured for wrong protocol
2. Custom ECU message IDs don't match filters

**Solutions:**
1. Verify `CAN_PROTOCOL` matches your ECU
2. Check serial output: "CAN filters configured for protocol: X"
3. For custom IDs, see "Custom Message IDs" section below

### Some Messages Missing

**Symptom:** Some parameters update, others don't

**Causes:**
1. ECU uses non-standard message IDs
2. Filter mask too restrictive

**Solutions:**
1. Check ECU documentation for actual message IDs
2. Modify filters in `can.cpp` `configureCANFilters()` function
3. Enable serial debug to see which IDs are being received

## Custom Message IDs

If your ECU uses custom message IDs not in the standard protocol:

### Option 1: Adjust Filters

Edit `gauge_V4/can.cpp` in the `configureCANFilters()` function:

```cpp
case CAN_PROTOCOL_HALTECH_V2:
  // Add custom ID range
  CAN0.init_Mask(0, 0, 0x7F0);
  CAN0.init_Filt(0, 0, 0x400);  // Accept 0x400-0x40F
  // ... rest of filters
  break;
```

### Option 2: Widen Mask

Make the mask less restrictive to accept more IDs:

```cpp
// Original: Accept only 0x360-0x36F
CAN0.init_Mask(0, 0, 0x7F0);  // Checks bits 11-4

// Wider: Accept 0x300-0x3FF
CAN0.init_Mask(0, 0, 0x700);  // Checks bits 11-8 only
```

### Option 3: Disable Filtering

For debugging or custom setups:

```cpp
// Accept all messages (no filtering)
CAN0.init_Mask(0, 0, 0x00000000);
CAN0.init_Mask(1, 0, 0x00000000);
```

## Technical Details

### MCP2515 Acceptance Filtering Process

1. **Message arrives** at CAN controller
2. **Hardware extracts** message ID
3. **For each filter:**
   - Apply mask to message ID
   - Compare result to filter value
   - If match found, accept message
4. **If no match:** Message rejected (no interrupt)
5. **If match:** Message stored in buffer, interrupt triggered

### Filter Evaluation Order

- Filters are checked in parallel (hardware)
- Any single filter match = message accepted
- No filter match = message rejected
- Two buffers can receive different messages simultaneously

### Extended vs Standard IDs

All filters configured for **standard 11-bit IDs** (not extended 29-bit):
```cpp
CAN0.init_Filt(num, 0, id);  // Second parameter: 0=standard, 1=extended
```

Standard IDs are used by all supported protocols (Haltech, Megasquirt, AiM, OBDII).

## Code Reference

### Files Modified

- **gauge_V4/can.h**: Added `configureCANFilters()` declaration
- **gauge_V4/can.cpp**: Implemented filter configuration
- **gauge_V4/gauge_V4.ino**: Added filter setup call during initialization

### Key Functions

```cpp
void configureCANFilters()  // Main configuration function
CAN0.init_Mask(num, ext, mask)  // Set acceptance mask
CAN0.init_Filt(num, ext, id)    // Set acceptance filter
```

## Benefits Summary

✅ **Reduced CPU Load:** 90-99% fewer interrupts on busy buses
✅ **Better Performance:** More CPU time for other tasks
✅ **Lower Power:** Fewer wake-ups = less power consumption
✅ **Automatic:** Configured based on protocol selection
✅ **Reliable:** Redundant filters prevent message loss
✅ **Efficient:** Hardware filtering is faster than software

## Future Enhancements

Possible improvements:
1. Runtime filter reconfiguration (change protocol without reboot)
2. User-configurable custom filter masks
3. Filter statistics/monitoring
4. Dynamic filter adjustment based on bus load

---

**Implementation Status:** ✅ Complete and tested
**Compatibility:** All protocols supported
**Performance Impact:** Significant improvement on busy CAN buses
